#include "WebServerService.h"
#include "PaperOS.h"
#include <LittleFS.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <esp_system.h>

namespace paperos {
static const char kEmergencyPortal[] PROGMEM = R"paperosportal(
<!doctype html><html lang="it"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<meta name="theme-color" content="#111315"><title>PaperOS Setup</title>
<style>
:root{color-scheme:light;font:16px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;color:#151719;background:#eef0ed}
*{box-sizing:border-box}body{margin:0;min-height:100vh;display:grid;place-items:center;padding:18px}
main{width:min(720px,100%);background:white;border:1px solid #d9ddd8;border-radius:24px;padding:clamp(22px,5vw,42px);box-shadow:0 24px 70px #11131518}
.brand{display:flex;align-items:center;gap:12px;margin-bottom:34px}.mark{width:46px;height:46px;border-radius:14px;background:#111315;color:white;display:grid;place-items:center;font-weight:800;font-size:22px}.brand small{display:block;color:#6b716d;margin-top:3px}
.eyebrow{font-size:12px;letter-spacing:.14em;font-weight:800;color:#6b716d}h1{font-size:clamp(28px,5vw,42px);line-height:1.08;letter-spacing:-.04em;margin:8px 0 10px}p{color:#626a65;line-height:1.55}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}.wide{grid-column:1/-1}label{font-weight:700;font-size:13px;color:#505652}input,select{display:block;width:100%;margin-top:7px;padding:13px;border:1px solid #cfd4cf;border-radius:12px;background:white;font:inherit}button{width:100%;margin-top:18px;border:0;border-radius:12px;padding:14px;background:#111315;color:white;font:inherit;font-weight:700;font-size:15px}.status{min-height:24px;color:#8a3b2e}.tip{background:#f5f6f4;padding:14px;border-radius:12px;font-size:14px}.hidden{display:none!important}@media(max-width:560px){.grid{grid-template-columns:1fr}.wide{grid-column:auto}}
</style><body><main><div class="brand"><div class="mark">P</div><div><strong>PaperOS</strong><small>Setup &amp; device console</small></div></div>
<section id="setup" class="hidden"><div class="eyebrow">FIRST START</div><h1>Connect your M5Paper</h1><p>Set an administrator password and connect PaperOS to your 2.4 GHz Wi-Fi network.</p>
<div class="grid"><label>Device name<input id="device" value="PaperOS"></label><label>Language<select id="language"><option value="it">Italiano</option><option value="en">English</option></select></label><label>Wi-Fi name (SSID)<input id="ssid" autocomplete="off"></label><label>Wi-Fi password<input id="wifi" type="password"></label><label class="wide">Administrator password (at least 6 characters)<input id="admin" type="password"></label></div>
<button onclick="setupDevice()">Save and connect</button><div id="status" class="status"></div></section>
<section id="missing" class="hidden"><div class="eyebrow">WEB CONSOLE FILES NOT INSTALLED</div><h1>PaperOS is running</h1><p>The firmware is responding, but its Web UI filesystem has not been uploaded yet. Connect by USB to a computer and run:</p><div class="tip"><code>pio run -e m5paper -t uploadfs</code></div><p>After the upload, reconnect to the device and open <b>http://paperos.local</b>. During first setup, use <b>http://192.168.4.1</b>.</p></section>
</main><script>
fetch('/api/setup/status').then(r=>r.json()).then(s=>document.getElementById(s.setupComplete?'missing':'setup').classList.remove('hidden')).catch(()=>document.getElementById('missing').classList.remove('hidden'));
async function setupDevice(){const status=document.getElementById('status');status.textContent='Saving settings…';try{const r=await fetch('/api/setup',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({deviceName:document.getElementById('device').value,language:document.getElementById('language').value,timezone:'Europe/Rome',ssid:document.getElementById('ssid').value,wifiPassword:document.getElementById('wifi').value,password:document.getElementById('admin').value})});const d=await r.json();status.textContent=d.ok?'Saved. PaperOS is restarting…':(d.error||'Setup failed');}catch(e){status.textContent='Cannot reach PaperOS. Stay connected to its Wi-Fi and retry.'}}
</script></body></html>
)paperosportal";

String WebServerService::newToken() { char b[33]; for(int i=0;i<32;i++) b[i]="0123456789abcdef"[esp_random()&15]; b[32]=0; return String(b); }
String WebServerService::body() { return server_.arg("plain"); }
void WebServerService::sendJson(int code,const String& json) { server_.send(code,"application/json",json); }

bool WebServerService::authorized() {
  if (!config_.get().setupComplete) return true;
  if (!sessionToken_.length()) return false;
  if (server_.hasHeader("Cookie") && server_.header("Cookie").indexOf("paperos="+sessionToken_)>=0) return true;
  if (server_.hasHeader("X-PaperOS-Token") && server_.header("X-PaperOS-Token")==sessionToken_) return true;
  return false;
}

void WebServerService::begin() {
  const char* headers[]={"Cookie","X-PaperOS-Token"}; server_.collectHeaders(headers,2); routes(); server_.begin();
}

void WebServerService::routes() {
  server_.on("/",HTTP_GET,[this](){
    if (LittleFS.exists("/index.html")) { File f=LittleFS.open("/index.html",FILE_READ); server_.streamFile(f,"text/html"); f.close(); }
    else server_.send_P(200,"text/html; charset=utf-8",kEmergencyPortal);
  });
  server_.on("/style.css",HTTP_GET,[this](){ File f=LittleFS.open("/style.css",FILE_READ); if(!f){server_.send(404);return;} server_.streamFile(f,"text/css"); f.close(); });
  server_.on("/app.js",HTTP_GET,[this](){ File f=LittleFS.open("/app.js",FILE_READ); if(!f){server_.send(404);return;} server_.streamFile(f,"application/javascript"); f.close(); });
  server_.on("/generate_204",HTTP_ANY,[this](){ server_.sendHeader("Location","http://192.168.4.1/",true); server_.send(302,"text/plain",""); });
  server_.on("/hotspot-detect.html",HTTP_ANY,[this](){ server_.sendHeader("Location","http://192.168.4.1/",true); server_.send(302,"text/plain",""); });
  server_.on("/connecttest.txt",HTTP_ANY,[this](){ server_.sendHeader("Location",wifi_.setupApActive()?"http://192.168.4.1/":"http://paperos.local/",true); server_.send(302,"text/plain",""); });
  server_.on("/ncsi.txt",HTTP_ANY,[this](){ server_.sendHeader("Location",wifi_.setupApActive()?"http://192.168.4.1/":"http://paperos.local/",true); server_.send(302,"text/plain",""); });
  server_.onNotFound([this](){ if(wifi_.setupApActive()){ server_.sendHeader("Location","http://192.168.4.1/",true); server_.send(302,"text/plain",""); } else server_.send(404,"application/json","{\"error\":\"not_found\"}"); });

  server_.on("/api/setup/status",HTTP_GET,[this](){ sendJson(200,String("{\"setupComplete\":")+(config_.get().setupComplete?"true":"false")+"}"); });
  server_.on("/api/setup",HTTP_POST,[this](){
    if(config_.get().setupComplete){sendJson(403,"{\"error\":\"already_configured\"}");return;}
    DynamicJsonDocument d(4096); if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;}
    String pw=d["password"]|""; if(pw.length()<6){sendJson(400,"{\"error\":\"password_too_short\"}");return;}
    config_.edit().deviceName=String((const char*)(d["deviceName"]|"PaperOS"));
    config_.edit().language=String((const char*)(d["language"]|"it"));
    config_.edit().timezone=String((const char*)(d["timezone"]|"Europe/Rome"));
    config_.edit().sleepMinutes=d["sleepMinutes"]|15;
    config_.setAdminPassword(pw);
    String ssid=d["ssid"]|"", wpass=d["wifiPassword"]|""; if(ssid.length()) config_.upsertNetwork(ssid,wpass,100);
    config_.edit().setupComplete=true; config_.save(); sessionToken_=newToken();
    server_.sendHeader("Set-Cookie","paperos="+sessionToken_+"; Path=/; HttpOnly; SameSite=Strict"); sendJson(200,"{\"ok\":true,\"rebooting\":true}"); delay(300); ESP.restart();
  });

  server_.on("/api/login",HTTP_POST,[this](){ DynamicJsonDocument d(1024); if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;} String p=d["password"]|""; if(!config_.checkAdminPassword(p)){sendJson(401,"{\"error\":\"unauthorized\"}");return;} sessionToken_=newToken(); server_.sendHeader("Set-Cookie","paperos="+sessionToken_+"; Path=/; HttpOnly; SameSite=Strict"); sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/logout",HTTP_POST,[this](){sessionToken_="";server_.sendHeader("Set-Cookie","paperos=; Max-Age=0; Path=/");sendJson(200,"{\"ok\":true}");});

  server_.on("/api/system/status",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(3584); d["name"]=NAME;d["version"]=VERSION;d["deviceName"]=config_.get().deviceName;d["uptimeMs"]=millis();d["heapFree"]=ESP.getFreeHeap();d["heapMin"]=ESP.getMinFreeHeap();d["psramTotal"]=ESP.getPsramSize();d["psramFree"]=ESP.getFreePsram();d["batteryPercent"]=power_.batteryPercent();d["batteryMv"]=power_.batteryMillivolts();d["batteryTrend"]=power_.batteryTrendLabel();d["chargingLikely"]=power_.chargingLikely();d["batteryCurrentSupported"]=power_.batteryCurrentSupported();d["wakeReason"]=power_.wakeReason();d["sdMounted"]=storage_.available();d["sdFree"]=storage_.freeBytes();d["ip"]=wifi_.ip().toString();d["mac"]=WiFi.macAddress(); String o;serializeJson(d,o);sendJson(200,o); });
  server_.on("/api/battery",HTTP_GET,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(1024);
    d["percent"]=power_.batteryPercent();
    d["millivolts"]=power_.batteryMillivolts();
    d["nominalCapacityMah"]=power_.nominalBatteryCapacityMah();
    d["currentSupported"]=power_.batteryCurrentSupported();
    d["currentMa"]=nullptr;
    d["trend"]=power_.batteryTrendLabel();
    d["trendDeltaMv"]=power_.batteryTrendDeltaMv();
    d["chargingLikely"]=power_.chargingLikely();
    d["wakeReason"]=power_.wakeReason();
    d["hardwareNote"]="M5Paper V1 cannot measure charger state or battery current";
    String o; serializeJson(d,o); sendJson(200,o);
  });
  server_.on("/api/wifi",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} sendJson(200,wifi_.statusJson()); });
  server_.on("/api/wifi/connect",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(2048); if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;} bool ok=wifi_.connect(d["ssid"]|"",d["password"]|"",true);sendJson(ok?200:503,ok?"{\"ok\":true}":"{\"ok\":false}"); });

  server_.on("/api/files",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} String p=server_.arg("path");if(!p.length())p="/PaperOS";sendJson(200,storage_.listJson(p)); });
  server_.on("/api/files/mkdir",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(1024);deserializeJson(d,body()); bool ok=storage_.makeDir(d["path"]|"");sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });
  server_.on("/api/files/delete",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(1024);deserializeJson(d,body()); bool ok=storage_.removePath(d["path"]|"");sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });
  server_.on("/api/files/rename",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(1536);deserializeJson(d,body()); bool ok=storage_.renamePath(d["from"]|"",d["to"]|"");sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });
  server_.on("/api/files/copy",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(1536);deserializeJson(d,body()); bool ok=storage_.copyFile(d["from"]|"",d["to"]|"");sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });
  server_.on("/api/files/download",HTTP_GET,[this](){ if(!authorized()){server_.send(401);return;} String p=server_.arg("path"); if(!storage_.validPath(p)||!SD.exists(p)){server_.send(404);return;} File f=SD.open(p,FILE_READ); server_.sendHeader("Content-Disposition","attachment; filename=\""+String(f.name())+"\""); server_.streamFile(f,"application/octet-stream"); f.close(); });
  server_.on("/api/files/upload",HTTP_POST,[this](){ if(!authorized()){server_.send(401);return;} if(uploadFile_)uploadFile_.close();sendJson(200,"{\"ok\":true}"); },[this](){
    if(!authorized()) return; HTTPUpload& up=server_.upload(); if(up.status==UPLOAD_FILE_START){ String dir=server_.arg("path"); if(!storage_.validPath(dir))dir="/PaperOS/Downloads"; String p=dir+"/"+up.filename; uploadFile_=SD.open(p,FILE_WRITE);} else if(up.status==UPLOAD_FILE_WRITE){if(uploadFile_)uploadFile_.write(up.buf,up.currentSize);} else if(up.status==UPLOAD_FILE_END||up.status==UPLOAD_FILE_ABORTED){if(uploadFile_)uploadFile_.close();}
  });

  server_.on("/api/notes",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} sendJson(200,notes_.listJson()); });
  server_.on("/api/notes/item",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} sendJson(200,notes_.get(server_.arg("id"))); });
  server_.on("/api/notes/item",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} String id=server_.arg("id");bool ok=notes_.save(id,body());sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });
  server_.on("/api/notes/delete",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(512);deserializeJson(d,body());bool ok=notes_.remove(d["id"]|"");sendJson(ok?200:400,ok?"{\"ok\":true}":"{\"ok\":false}"); });

  server_.on("/api/phone/status",HTTP_GET,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(1024);
    d["active"]=phoneLink_.active();
    d["connected"]=phoneLink_.connected();
    d["status"]=phoneLink_.statusText();
    d["notifications"]=phoneLink_.notificationCount();
    d["lastCommand"]=phoneLink_.lastCommand();
    d["phoneName"]=phoneLink_.phoneName();
    d["phoneBattery"]=phoneLink_.phoneBatteryKnown()?phoneLink_.phoneBatteryPercent():-1;
    d["phoneChargingKnown"]=phoneLink_.phoneChargingKnown();
    d["phoneCharging"]=phoneLink_.phoneCharging();
    d["phoneStatusAgeMs"]=phoneLink_.phoneStatusAgeMs();
    String o; serializeJson(d,o); sendJson(200,o);
  });

  server_.on("/api/phone/notifications",HTTP_GET,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(6144);
    JsonArray arr=d.createNestedArray("items");
    size_t count=phoneLink_.notificationCount();
    for(size_t i=0;i<count && i<30;++i){
      const PhoneNotification* n=phoneLink_.notification(i);
      if(!n) continue;
      JsonObject o=arr.createNestedObject();
      o["app"]=n->app;o["title"]=n->title;o["body"]=n->body;o["receivedMs"]=n->receivedMs;
    }
    String o;serializeJson(d,o);sendJson(200,o);
  });

  server_.on("/api/phone/status",HTTP_POST,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(1024);
    if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;}
    int battery=d.containsKey("battery")?d["battery"].as<int>():-1;
    bool charging=d.containsKey("charging")?d["charging"].as<bool>():false;
    bool chargingKnown=d.containsKey("charging");
    String name=d["device"]|"iPhone";
    phoneLink_.updatePhoneStatus(battery,charging,chargingKnown,name);
    sendJson(200,"{\"ok\":true}");
  });

  server_.on("/api/phone/notify",HTTP_POST,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(2048);
    if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;}
    phoneLink_.pushNotification(d["app"]|"Phone",d["title"]|"",d["body"]|"");
    sendJson(200,"{\"ok\":true}");
  });

  server_.on("/api/phone/command",HTTP_POST,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    DynamicJsonDocument d(1536);
    if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;}
    String cmd=d["command"]|"";
    bool ok=phoneLink_.sendCommand(cmd);
    sendJson(ok?200:409,ok?"{\"ok\":true}":"{\"ok\":false,\"error\":\"bridge_inactive\"}");
  });

  server_.on("/api/phone/start",HTTP_POST,[this](){
    if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}
    bool ok=phoneLink_.begin();
    sendJson(ok?200:500,ok?"{\"ok\":true}":"{\"ok\":false}");
  });

  server_.on("/api/settings",HTTP_GET,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(1536);d["deviceName"]=config_.get().deviceName;d["language"]=config_.get().language;d["timezone"]=config_.get().timezone;d["sleepMinutes"]=config_.get().sleepMinutes;d["touchWakeEnabled"]=config_.get().touchWakeEnabled;d["scheduledWakeMinutes"]=config_.get().scheduledWakeMinutes; String o;serializeJson(d,o);sendJson(200,o); });
  server_.on("/api/settings",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;} DynamicJsonDocument d(2048);if(deserializeJson(d,body())){sendJson(400,"{\"error\":\"invalid_json\"}");return;} if(d.containsKey("deviceName"))config_.edit().deviceName=String((const char*)d["deviceName"]);if(d.containsKey("language"))config_.edit().language=String((const char*)d["language"]);if(d.containsKey("timezone"))config_.edit().timezone=String((const char*)d["timezone"]);if(d.containsKey("sleepMinutes"))config_.edit().sleepMinutes=d["sleepMinutes"].as<uint32_t>();if(d.containsKey("touchWakeEnabled"))config_.edit().touchWakeEnabled=d["touchWakeEnabled"].as<bool>();if(d.containsKey("scheduledWakeMinutes"))config_.edit().scheduledWakeMinutes=d["scheduledWakeMinutes"].as<uint32_t>();bool ok=config_.save();sendJson(ok?200:500,ok?"{\"ok\":true}":"{\"ok\":false}"); });

  server_.on("/api/device/reboot",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}sendJson(200,"{\"ok\":true}");delay(250);ESP.restart(); });
  server_.on("/api/device/sleep",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}sendJson(200,"{\"ok\":true}");delay(250);power_.sleepNow(); });
  server_.on("/api/device/home",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showHome();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/apps",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showApps();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/battery",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showBattery();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/clock",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showClock();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/focus",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showFocus();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/fun",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showFun();sendJson(200,"{\"ok\":true}"); });
  server_.on("/api/device/refresh",HTTP_POST,[this](){ if(!authorized()){sendJson(401,"{\"error\":\"unauthorized\"}");return;}ui_.showHome(true);sendJson(200,"{\"ok\":true}"); });

  server_.on("/api/ota",HTTP_POST,[this](){ if(!authorized()){server_.send(401);return;} bool ok=!Update.hasError()&&otaOk_; sendJson(ok?200:500,ok?"{\"ok\":true,\"rebooting\":true}":"{\"ok\":false}"); if(ok){delay(300);ESP.restart();} },[this](){
    if(!authorized())return; HTTPUpload& up=server_.upload(); if(up.status==UPLOAD_FILE_START){otaOk_=Update.begin(UPDATE_SIZE_UNKNOWN,U_FLASH);} else if(up.status==UPLOAD_FILE_WRITE){if(otaOk_&&Update.write(up.buf,up.currentSize)!=up.currentSize)otaOk_=false;} else if(up.status==UPLOAD_FILE_END){if(otaOk_)otaOk_=Update.end(true);} else if(up.status==UPLOAD_FILE_ABORTED){Update.abort();otaOk_=false;}
  });
}
}

#include "NetworkToolsService.h"
#include <esp_system.h>

namespace paperos {

NetworkToolsService* NetworkToolsService::callbackOwner_ = nullptr;

NetworkToolsService::NetworkToolsService() : mqtt_(mqttNet_) {
  callbackOwner_ = this;
  mqtt_.setCallback(NetworkToolsService::mqttCallback);
  mqtt_.setBufferSize(1024);
  mqtt_.setSocketTimeout(3);
}

bool NetworkToolsService::dnsLookup(const String& host, IPAddress& out) {
  String h = host;
  h.trim();
  if (!h.length()) return false;
  return WiFi.hostByName(h.c_str(), out) == 1;
}

PingResult NetworkToolsService::ping(const String& host, uint8_t count) {
  PingResult r;
  r.target = host;

  if (WiFi.status() != WL_CONNECTED) {
    r.error = "Wi-Fi not connected";
    return r;
  }

  if (!dnsLookup(host, r.ip)) {
    r.error = "DNS/host lookup failed";
    return r;
  }

  bool ok = Ping.ping(r.ip, count ? count : 1);
  r.ok = ok;
  r.averageMs = ok ? Ping.averageTime() : 0.0f;
  if (!ok) r.error = "No ICMP reply";
  return r;
}

std::vector<LanServiceHost> NetworkToolsService::quickLanScan(uint8_t firstHost, uint8_t lastHost) {
  std::vector<LanServiceHost> found;
  if (WiFi.status() != WL_CONNECTED) return found;

  IPAddress local = WiFi.localIP();
  if (lastHost < firstHost) return found;

  const uint16_t ports[] = {80, 443, 22, 1883, 8080};
  for (uint16_t host = firstHost; host <= lastHost; ++host) {
    IPAddress ip(local[0], local[1], local[2], static_cast<uint8_t>(host));
    if (ip == local) continue;

    for (uint16_t port : ports) {
      WiFiClient c;
      bool ok = c.connect(ip, port, 35);
      if (ok) {
        LanServiceHost item;
        item.ip = ip;
        item.port = port;
        found.push_back(item);
        c.stop();
        break;
      }
      c.stop();
      delay(1);
    }

    if (found.size() >= 12) break;
  }
  return found;
}

ApiResult NetworkToolsService::httpRequest(const String& methodRaw, const String& urlRaw, const String& body) {
  ApiResult r;
  if (WiFi.status() != WL_CONNECTED) {
    r.error = "Wi-Fi not connected";
    return r;
  }

  String url = urlRaw;
  url.trim();
  if (!url.length()) {
    r.error = "Empty URL";
    return r;
  }
  if (!url.startsWith("http://") && !url.startsWith("https://")) url = "http://" + url;

  String method = methodRaw;
  method.toUpperCase();
  if (!method.length()) method = "GET";

  HTTPClient http;
  http.setConnectTimeout(4000);
  http.setTimeout(6000);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("PaperOS-NetworkToolkit/0.1");

  WiFiClient plain;
  WiFiClientSecure secure;
  bool begun = false;
  if (url.startsWith("https://")) {
    secure.setInsecure();
    begun = http.begin(secure, url);
  } else {
    begun = http.begin(plain, url);
  }
  if (!begun) {
    r.error = "Unable to start request";
    return r;
  }

  const char* headers[] = {"Content-Type"};
  http.collectHeaders(headers, 1);
  if (body.length()) http.addHeader("Content-Type", "application/json");

  int code = 0;
  if (method == "GET") code = http.GET();
  else if (method == "POST") code = http.POST(body);
  else if (method == "PUT") code = http.PUT(body);
  else code = http.sendRequest(method.c_str(), body);

  r.status = code;
  if (code <= 0) {
    r.error = http.errorToString(code);
    http.end();
    return r;
  }

  r.contentType = http.header("Content-Type");
  r.body = http.getString();
  if (r.body.length() > 6000) r.body = r.body.substring(0, 6000);
  r.ok = code >= 200 && code < 400;
  if (!r.ok) r.error = String("HTTP ") + code;
  http.end();
  return r;
}

bool NetworkToolsService::parseMac(const String& text, uint8_t out[6]) const {
  String s = text;
  s.trim();
  s.replace("-", ":");
  int values[6];
  if (sscanf(s.c_str(), "%x:%x:%x:%x:%x:%x",
             &values[0], &values[1], &values[2],
             &values[3], &values[4], &values[5]) != 6) return false;
  for (int i = 0; i < 6; ++i) {
    if (values[i] < 0 || values[i] > 255) return false;
    out[i] = static_cast<uint8_t>(values[i]);
  }
  return true;
}

bool NetworkToolsService::wakeOnLan(const String& macText, uint16_t port) {
  if (WiFi.status() != WL_CONNECTED) return false;
  uint8_t mac[6];
  if (!parseMac(macText, mac)) return false;

  uint8_t packet[102];
  memset(packet, 0xFF, 6);
  for (int i = 0; i < 16; ++i) memcpy(packet + 6 + i * 6, mac, 6);

  WiFiUDP udp;
  if (!udp.begin(0)) return false;

  IPAddress local = WiFi.localIP();
  IPAddress mask = WiFi.subnetMask();
  IPAddress broadcast(
    local[0] | (~mask[0] & 0xFF),
    local[1] | (~mask[1] & 0xFF),
    local[2] | (~mask[2] & 0xFF),
    local[3] | (~mask[3] & 0xFF)
  );

  bool ok = udp.beginPacket(broadcast, port) == 1;
  if (ok) {
    udp.write(packet, sizeof(packet));
    ok = udp.endPacket() == 1;
  }
  udp.stop();
  return ok;
}

bool NetworkToolsService::mqttConnect(const String& host, uint16_t port) {
  if (WiFi.status() != WL_CONNECTED) return false;
  mqttHost_ = host;
  mqttHost_.trim();
  mqttPort_ = port ? port : 1883;
  if (!mqttHost_.length()) return false;

  mqtt_.setServer(mqttHost_.c_str(), mqttPort_);
  String clientId = "PaperOS-";
  clientId += String(static_cast<uint32_t>(esp_random()), HEX);
  return mqtt_.connect(clientId.c_str());
}

void NetworkToolsService::mqttDisconnect() {
  mqtt_.disconnect();
}

bool NetworkToolsService::mqttPublish(const String& topic, const String& payload, bool retained) {
  if (!mqtt_.connected() || !topic.length()) return false;
  return mqtt_.publish(topic.c_str(), payload.c_str(), retained);
}

bool NetworkToolsService::mqttSubscribe(const String& topic) {
  if (!mqtt_.connected() || !topic.length()) return false;
  return mqtt_.subscribe(topic.c_str());
}

bool NetworkToolsService::mqttConnected() const {
  return mqtt_.connected();
}

int NetworkToolsService::mqttState() const {
  return mqtt_.state();
}

void NetworkToolsService::loop() {
  if (mqtt_.connected()) mqtt_.loop();
}

void NetworkToolsService::mqttCallback(char* topic, uint8_t* payload, unsigned int length) {
  if (!callbackOwner_) return;
  callbackOwner_->lastMqttTopic_ = topic ? String(topic) : String();
  callbackOwner_->lastMqttPayload_ = "";
  unsigned int limit = length > 768 ? 768 : length;
  callbackOwner_->lastMqttPayload_.reserve(limit);
  for (unsigned int i = 0; i < limit; ++i) callbackOwner_->lastMqttPayload_ += static_cast<char>(payload[i]);
}

}

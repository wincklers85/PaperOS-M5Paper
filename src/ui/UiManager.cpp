#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>

namespace paperos {
void UiManager::begin() { showHome(true); }

void UiManager::topBar() {
  auto dt = M5.Rtc.getDateTime();
  M5.Display.fillRect(0,0,M5.Display.width(),50,TFT_WHITE);
  M5.Display.drawFastHLine(0,49,M5.Display.width(),TFT_BLACK);
  M5.Display.setCursor(12,14); M5.Display.setTextSize(1);
  M5.Display.printf("%02d:%02d  %02d/%02d/%04d",dt.time.hours,dt.time.minutes,dt.date.date,dt.date.month,dt.date.year);
  String right = String(power_.batteryPercent()) + "%  " + (wifi_.isConnected()?"WiFi":"AP") + "  " + (storage_.available()?"SD":"SD!");
  M5.Display.setTextDatum(top_right); M5.Display.drawString(right,M5.Display.width()-12,14); M5.Display.setTextDatum(top_left);
}

void UiManager::drawButton(int x,int y,int w,int h,const String& label,bool comingSoon) {
  M5.Display.drawRoundRect(x,y,w,h,8,TFT_BLACK);
  M5.Display.setTextDatum(middle_center); M5.Display.setTextSize(1);
  M5.Display.drawString(label,x+w/2,y+h/2-(comingSoon?8:0));
  if (comingSoon) { M5.Display.setTextSize(0.75); M5.Display.drawString("Coming Soon",x+w/2,y+h/2+14); }
  M5.Display.setTextDatum(top_left);
}

void UiManager::showHome(bool full) {
  page_=Page::Home;
  if (full) M5.Display.fillScreen(TFT_WHITE);
  topBar();
  M5.Display.setCursor(22,78); M5.Display.setTextSize(2); M5.Display.println(config_.get().deviceName);
  M5.Display.setTextSize(1);
  M5.Display.setCursor(22,128); M5.Display.println("PaperOS v" PAPEROS_VERSION);
  M5.Display.setCursor(22,164); M5.Display.printf("Web: http://%s.local\n", HOSTNAME);
  M5.Display.setCursor(22,190); M5.Display.printf("IP: %s\n", wifi_.ip().toString().c_str());
  drawButton(22,260,235,84,"Apps"); drawButton(283,260,235,84,"System");
  drawButton(22,368,235,84,"Notes"); drawButton(283,368,235,84,"Settings");
  M5.Display.setCursor(22,510); M5.Display.setTextSize(1); M5.Display.println("Touch a card. Hold power button for sleep.");
  if (full) display_.fullRefresh(); else display_.partialRefresh(0,0,M5.Display.width(),560);
  lastFull_=millis();
}

void UiManager::showApps() {
  page_=Page::Apps; M5.Display.fillScreen(TFT_WHITE); topBar();
  M5.Display.setCursor(18,70); M5.Display.setTextSize(2); M5.Display.println("Apps");
  const char* names[] = {"Dashboard","Notes","Tasks","Calendar","Files","PDF Reader","Ebook","Bluetooth","Wi-Fi","Smart Home","Solar","MQTT","GPIO","Serial","System","Settings"};
  for (int i=0;i<16;i++) { int col=i%2,row=i/2; bool cs = !(i==0||i==1||i==4||i==8||i==14||i==15); drawButton(18+col*260,120+row*92,244,72,names[i],cs); }
  display_.fullRefresh(); lastFull_=millis();
}

void UiManager::showSystem() {
  page_=Page::System; M5.Display.fillScreen(TFT_WHITE); topBar();
  M5.Display.setCursor(18,72); M5.Display.setTextSize(2); M5.Display.println("System Monitor");
  M5.Display.setTextSize(1); M5.Display.setCursor(18,125);
  M5.Display.printf("Firmware: %s\n",VERSION);
  M5.Display.printf("Uptime: %lus\n",millis()/1000UL);
  M5.Display.printf("Heap free: %u\n",ESP.getFreeHeap());
  M5.Display.printf("Heap min: %u\n",ESP.getMinFreeHeap());
  M5.Display.printf("PSRAM total: %u\n",ESP.getPsramSize());
  M5.Display.printf("PSRAM free: %u\n",ESP.getFreePsram());
  M5.Display.printf("Battery: %d%% / %dmV\n",power_.batteryPercent(),power_.batteryMillivolts());
  M5.Display.printf("Wi-Fi: %s  RSSI %d\n",wifi_.isConnected()?"connected":"setup",wifi_.isConnected()?WiFi.RSSI():0);
  M5.Display.printf("IP: %s\n",wifi_.ip().toString().c_str());
  M5.Display.printf("SD: %s\n",storage_.available()?"mounted":"not mounted");
  if (storage_.available()) M5.Display.printf("SD free: %.1f MB\n",storage_.freeBytes()/1048576.0);
  drawButton(18,760,504,70,"Home"); display_.fullRefresh(); lastFull_=millis();
}

void UiManager::showSettings() {
  page_=Page::Settings; M5.Display.fillScreen(TFT_WHITE); topBar();
  M5.Display.setCursor(18,72); M5.Display.setTextSize(2); M5.Display.println("Settings");
  M5.Display.setTextSize(1); M5.Display.setCursor(18,125);
  M5.Display.println("Full settings are available in the Web UI.");
  M5.Display.printf("Device: %s\n",config_.get().deviceName.c_str());
  M5.Display.printf("Language: %s\n",config_.get().language.c_str());
  M5.Display.printf("Timezone: %s\n",config_.get().timezone.c_str());
  M5.Display.printf("Sleep: %lu min\n",static_cast<unsigned long>(config_.get().sleepMinutes));
  drawButton(18,760,504,70,"Home"); display_.fullRefresh(); lastFull_=millis();
}

void UiManager::loop() {
  M5.update();
  if (M5.BtnPWR.wasHold()) power_.sleepNow();
  if (M5.BtnA.wasClicked()) { power_.markActivity(); showHome(); return; }
  if (M5.BtnB.wasClicked()) { power_.markActivity(); showApps(); return; }
  auto e=touch_.poll();
  if (e.clicked) {
    power_.markActivity();
    if (page_==Page::Home) {
      if (e.y>=260 && e.y<344) { if(e.x<270) showApps(); else showSystem(); }
      else if (e.y>=368 && e.y<452) { if(e.x>=270) showSettings(); }
    } else if ((page_==Page::System || page_==Page::Settings) && e.y>=740) showHome();
  }
  if (page_==Page::Home && millis()-lastClock_>60000) { lastClock_=millis(); topBar(); display_.partialRefresh(0,0,M5.Display.width(),52); }
  if (millis()-lastFull_>30UL*60UL*1000UL) showHome(true);
}
}

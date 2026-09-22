#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>

namespace paperos {

void UiManager::begin() {
  showHome(true);
}

void UiManager::statusBar() {
  const int w = M5.Display.width();
  auto dt = M5.Rtc.getDateTime();

  M5.Display.fillRect(0, 0, w, UiTheme::StatusH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::StatusH - 1, w, TFT_LIGHTGREY);

  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextSize(1.4f);
  M5.Display.drawString(clockText, 16, 12);

  char dateText[16];
  snprintf(dateText, sizeof(dateText), "%02d/%02d/%02d", dt.date.date, dt.date.month, dt.date.year % 100);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_WHITE);
  M5.Display.drawString(dateText, 92, 17);

  const int iconY = 17;
  UiTheme::wifiIcon(345, iconY, wifi_.isConnected());
  UiTheme::bluetoothIcon(377, iconY - 1, false);
  UiTheme::sdIcon(409, iconY - 3, storage_.available());
  UiTheme::batteryIcon(466, iconY + 1, power_.batteryPercent());

  M5.Display.setTextDatum(top_right);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(String(power_.batteryPercent()) + "%", 458, 17);
  M5.Display.setTextDatum(top_left);
}

void UiManager::bottomNav(int active) {
  M5.Display.fillRect(0, UiTheme::NavY, UiTheme::ScreenW, UiTheme::NavH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::NavY, UiTheme::ScreenW, TFT_LIGHTGREY);
  const char* labels[5] = {"Home", "Notes", "Files", "Tools", "Apps"};
  for (int i = 0; i < 5; ++i) UiTheme::navItem(i, labels[i], i == active);
}

void UiManager::homeCard(int x, int y, int w, int h, const String& title, const String& mainValue, const String& detail, bool withChevron) {
  UiTheme::card(x, y, w, h);
  UiTheme::label(title, x + 16, y + 15);
  if (withChevron) UiTheme::chevron(x + w - 27, y + 17);
  UiTheme::value(mainValue, x + 16, y + 48, 1.65f);
  UiTheme::muted(detail, x + 16, y + h - 31);
}

void UiManager::showHome(bool full) {
  page_ = Page::Home;
  UiTheme::beginFrame();
  statusBar();

  UiTheme::value("PaperOS", 18, 72, 2.3f);
  UiTheme::muted(config_.get().deviceName + "  /  " + String(VERSION), 20, 111);

  UiTheme::card(18, 136, 504, 132);
  UiTheme::label("NETWORK", 34, 152);
  UiTheme::pill(wifi_.isConnected() ? "ONLINE" : "SETUP AP", 402, 149, wifi_.isConnected());
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 184, 1.65f);
  UiTheme::muted(wifi_.ip().toString(), 34, 230);
  UiTheme::chevron(493, 190);

  const int half = 246;
  homeCard(18, 282, half, 132, "BATTERY", String(power_.batteryPercent()) + "%", String(power_.batteryMillivolts()) + " mV", true);
  String storageValue = storage_.available() ? String(storage_.freeBytes() / 1048576.0, 0) + " MB" : "No SD";
  homeCard(276, 282, half, 132, "STORAGE", storageValue, storage_.available() ? "Free space" : "Insert microSD", true);

  homeCard(18, 428, half, 132, "NOTES", "Web + SD", "Native editor planned", true);
  homeCard(276, 428, half, 132, "SOLAR", "Coming soon", "MQTT / inverter module", true);

  UiTheme::card(18, 574, 504, 126);
  UiTheme::label("SYSTEM HEALTH", 34, 590);
  UiTheme::value(String(ESP.getFreeHeap() / 1024) + " KB heap free", 34, 623, 1.45f);
  UiTheme::muted("Tap for memory, PSRAM, power and storage", 34, 665);
  UiTheme::chevron(493, 625);

  UiTheme::card(18, 714, 504, 138);
  UiTheme::label("BROWSER CONSOLE", 34, 730);
  UiTheme::value("paperos.local", 34, 763, 1.55f);
  UiTheme::muted("Full control from phone, tablet or computer", 34, 807);
  UiTheme::pill("WEB", 439, 730, true);

  bottomNav(0);

  if (full) display_.fullRefresh();
  else display_.partialRefresh(0, 0, M5.Display.width(), UiTheme::NavY);
  lastFull_ = millis();
}

void UiManager::showApps() {
  page_ = Page::Apps;
  UiTheme::beginFrame();
  statusBar();

  UiTheme::value("Apps", 18, 72, 2.2f);
  UiTheme::muted("Your PaperOS tools", 20, 111);

  const char* names[12] = {
    "Dashboard", "Notes", "Tasks",
    "Calendar", "Files", "PDF",
    "Wi-Fi", "Bluetooth", "MQTT",
    "GPIO", "Settings", "System"
  };
  const char* glyphs[12] = {
    "DB", "NT", "TK",
    "CL", "FL", "PDF",
    "WF", "BT", "MQ",
    "IO", "ST", "SYS"
  };
  const bool ready[12] = {
    true, false, false,
    false, false, false,
    true, false, false,
    false, true, true
  };

  const int tileW = 160;
  const int tileH = 122;
  const int startY = 139;
  for (int i = 0; i < 12; ++i) {
    int col = i % 3;
    int row = i / 3;
    int x = 18 + col * (tileW + 12);
    int y = startY + row * (tileH + 12);
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], !ready[i]);
  }

  UiTheme::card(18, 688, 504, 150);
  UiTheme::label("TIP", 34, 705);
  UiTheme::value("Use the Web UI for full controls", 34, 740, 1.35f);
  UiTheme::muted("Files, Notes, Wi-Fi, OTA and device controls are live.", 34, 786);

  bottomNav(4);
  display_.fullRefresh();
  lastFull_ = millis();
}

void UiManager::settingRow(int y, const String& title, const String& detail) {
  UiTheme::card(18, y, 504, 86);
  UiTheme::value(title, 34, y + 16, 1.25f);
  UiTheme::muted(detail, 34, y + 52);
  UiTheme::chevron(493, y + 34);
}

void UiManager::showSettings() {
  page_ = Page::Settings;
  UiTheme::beginFrame();
  statusBar();

  UiTheme::value("Settings", 18, 72, 2.2f);
  UiTheme::muted("Device essentials", 20, 111);

  settingRow(139, "Device", config_.get().deviceName + "  /  " + config_.get().language);
  settingRow(237, "Wi-Fi", wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup"));
  settingRow(335, "Power", String("Sleep after ") + config_.get().sleepMinutes + " min");
  settingRow(433, "Storage", storage_.available() ? "microSD mounted" : "microSD unavailable");
  settingRow(531, "Web console", String("http://") + HOSTNAME + ".local");
  settingRow(629, "About & System", String("PaperOS ") + VERSION);

  UiTheme::card(18, 739, 504, 110);
  UiTheme::label("ADVANCED SETTINGS", 34, 755);
  UiTheme::muted("Open paperos.local to edit settings and manage files.", 34, 792);
  UiTheme::muted("Admin authentication protects device controls.", 34, 817);

  bottomNav(4);
  display_.fullRefresh();
  lastFull_ = millis();
}

void UiManager::showSystem() {
  page_ = Page::System;
  UiTheme::beginFrame();
  statusBar();

  UiTheme::value("System", 18, 72, 2.2f);
  UiTheme::muted(String("PaperOS ") + VERSION, 20, 111);

  homeCard(18, 139, 246, 132, "HEAP", String(ESP.getFreeHeap() / 1024) + " KB", String("Min ") + ESP.getMinFreeHeap() / 1024 + " KB", false);
  homeCard(276, 139, 246, 132, "PSRAM", String(ESP.getFreePsram() / 1048576.0, 1) + " MB", String(ESP.getPsramSize() / 1048576.0, 1) + " MB total", false);
  homeCard(18, 285, 246, 132, "BATTERY", String(power_.batteryPercent()) + "%", String(power_.batteryMillivolts()) + " mV", false);
  homeCard(276, 285, 246, 132, "UPTIME", String(millis() / 60000UL) + " min", "Since last boot", false);

  UiTheme::card(18, 431, 504, 142);
  UiTheme::label("NETWORK", 34, 447);
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 480, 1.45f);
  UiTheme::muted(wifi_.ip().toString(), 34, 525);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "AP MODE", 393, 448, false);

  UiTheme::card(18, 587, 504, 142);
  UiTheme::label("STORAGE", 34, 603);
  UiTheme::value(storage_.available() ? "microSD mounted" : "microSD unavailable", 34, 636, 1.35f);
  if (storage_.available()) UiTheme::muted(String(storage_.freeBytes() / 1048576.0, 1) + " MB free", 34, 681);
  else UiTheme::muted("SD-backed apps are temporarily disabled", 34, 681);

  UiTheme::card(18, 743, 504, 106);
  UiTheme::label("WEB ADMIN", 34, 759);
  UiTheme::value("paperos.local", 34, 790, 1.35f);
  UiTheme::muted("Diagnostics, OTA and remote controls", 34, 821);

  bottomNav(3);
  display_.fullRefresh();
  lastFull_ = millis();
}

void UiManager::showComingSoon(const String& title, int activeNav) {
  page_ = Page::ComingSoon;
  comingTitle_ = title;
  comingNav_ = activeNav;

  UiTheme::beginFrame();
  statusBar();
  UiTheme::value(title, 18, 72, 2.2f);
  UiTheme::muted("PaperOS module", 20, 111);

  UiTheme::card(18, 160, 504, 420);
  M5.Display.drawRoundRect(204, 215, 132, 132, 24, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(2.2f);
  M5.Display.drawString("...", 270, 281);
  M5.Display.setTextDatum(top_left);

  UiTheme::value("Coming Soon", 170, 382, 1.8f);
  UiTheme::muted("This screen is intentionally not simulated.", 103, 438);
  UiTheme::muted("The module will appear here when its backend is real.", 66, 469);

  UiTheme::card(18, 602, 504, 150);
  UiTheme::label("AVAILABLE NOW", 34, 620);
  UiTheme::value("Open paperos.local", 34, 654, 1.45f);
  UiTheme::muted("Files, browser Notes, Wi-Fi, settings and OTA are live.", 34, 700);

  bottomNav(activeNav);
  display_.fullRefresh();
  lastFull_ = millis();
}

void UiManager::handleBottomNav(int x) {
  int index = constrain(x / (UiTheme::ScreenW / 5), 0, 4);
  if (index == 0) showHome();
  else if (index == 1) showComingSoon("Notes", 1);
  else if (index == 2) showComingSoon("Files", 2);
  else if (index == 3) showSystem();
  else showApps();
}

void UiManager::openAppIndex(int index) {
  const char* names[12] = {
    "Dashboard", "Notes", "Tasks", "Calendar", "Files", "PDF Reader",
    "Wi-Fi", "Bluetooth", "MQTT", "GPIO", "Settings", "System"
  };
  if (index == 0) showHome();
  else if (index == 6 || index == 10) showSettings();
  else if (index == 11) showSystem();
  else if (index >= 0 && index < 12) showComingSoon(names[index], 4);
}

void UiManager::loop() {
  M5.update();

  if (M5.BtnC.wasHold()) {
    power_.sleepNow();
    return;
  }
  if (M5.BtnA.wasClicked()) {
    power_.markActivity();
    showHome();
    return;
  }
  if (M5.BtnB.wasClicked()) {
    power_.markActivity();
    showApps();
    return;
  }

  auto e = touch_.poll();
  if (e.clicked) {
    power_.markActivity();

    if (e.y >= UiTheme::NavY) {
      handleBottomNav(e.x);
      return;
    }

    if (page_ == Page::Home) {
      if (e.y >= 136 && e.y < 268) showSettings();
      else if (e.y >= 282 && e.y < 414) showSystem();
      else if (e.y >= 428 && e.y < 560) {
        if (e.x < 270) showComingSoon("Notes", 1);
        else showComingSoon("Solar", 4);
      }
      else if (e.y >= 574 && e.y < 700) showSystem();
      else if (e.y >= 714 && e.y < 852) showSettings();
      return;
    }

    if (page_ == Page::Apps) {
      const int startY = 139;
      const int tileH = 122;
      const int pitchY = tileH + 12;
      if (e.y >= startY && e.y < startY + 4 * pitchY) {
        int row = (e.y - startY) / pitchY;
        int col = (e.x - 18) / 172;
        if (e.x >= 18 && col >= 0 && col < 3) {
          int localX = (e.x - 18) % 172;
          int localY = (e.y - startY) % pitchY;
          if (localX < 160 && localY < 122) openAppIndex(row * 3 + col);
        }
      }
      return;
    }

    if (page_ == Page::Settings && e.y >= 629 && e.y < 715) {
      showSystem();
      return;
    }
  }

  if (millis() - lastClock_ > 60000UL) {
    lastClock_ = millis();
    statusBar();
    display_.partialRefresh(0, 0, M5.Display.width(), UiTheme::StatusH);
  }

  if (millis() - lastFull_ > 30UL * 60UL * 1000UL) {
    if (page_ == Page::Home) showHome(true);
    else if (page_ == Page::Apps) showApps();
    else if (page_ == Page::Settings) showSettings();
    else if (page_ == Page::System) showSystem();
    else showComingSoon(comingTitle_, comingNav_);
  }
}

}

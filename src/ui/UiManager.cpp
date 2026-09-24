#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>

namespace paperos {

void UiManager::preparePage(bool forceClean) {
  // Full quality wipes are intentionally rare. Normal navigation uses the
  // fast waveform and regional updates are used for dynamic UI.
  if (forceClean || display_.pageChangesSinceClean() >= 20) {
    display_.cleanRefresh();
    lastDeepClean_ = millis();
  }
  UiTheme::beginFrame();
}

void UiManager::commitPage() {
  display_.pageRefresh();
}

void UiManager::showAppLoading(const String& titleText, const String& detailText, int percent) {
  UiTheme::beginFrame();
  statusBar();

  UiTheme::title(titleText, 18, 92);
  UiTheme::detail(detailText, 20, 136);

  UiTheme::card(42, 270, 456, 250, true);
  UiTheme::label("LOADING", 66, 300);
  UiTheme::value("Please wait", 66, 344, false);
  UiTheme::detail("PaperOS is preparing the app.", 66, 392);

  const int barX = 66, barY = 444, barW = 408, barH = 18;
  M5.Display.drawRoundRect(barX, barY, barW, barH, 9, TFT_BLACK);
  int fill = constrain((barW - 6) * constrain(percent, 0, 100) / 100, 0, barW - 6);
  if (fill > 0) M5.Display.fillRoundRect(barX + 3, barY + 3, fill, barH - 6, 6, TFT_BLACK);

  UiTheme::detail(String(constrain(percent, 0, 100)) + "%", 430, 478);
  display_.pageRefresh();
}

void UiManager::drawBatteryLiveArea() {
  const int pct = constrain(power_.batteryPercent(), 0, 100);
  const int mv = power_.batteryMillivolts();

  UiTheme::card(18, 145, 504, 178, true);
  UiTheme::label("BATTERY", 34, 162);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(String(pct) + "%", 34, 201);
  UiTheme::resetFont();
  UiTheme::detail(String(mv) + " mV", 36, 260);

  const int bx = 238, by = 194, bw = 246, bh = 46;
  M5.Display.drawRoundRect(bx, by, bw, bh, 10, TFT_BLACK);
  M5.Display.fillRect(bx + bw, by + 13, 8, 20, TFT_BLACK);
  const int fill = (bw - 10) * pct / 100;
  if (fill > 0) M5.Display.fillRoundRect(bx + 5, by + 5, fill, bh - 10, 6, TFT_BLACK);

  UiTheme::detail(String("Voltage trend: ") + power_.batteryTrendLabel(), 238, 258);
  if (power_.chargingLikely()) {
    UiTheme::pill("CHARGE LIKELY", 336, 279, true);
  } else if (power_.batteryTrend() == BatteryTrend::Falling) {
    UiTheme::pill("ON BATTERY", 360, 279, false);
  } else {
    UiTheme::pill("MONITORING", 362, 279, false);
  }

  UiTheme::card(18, 338, 504, 132);
  UiTheme::label("LIVE POWER STATUS", 34, 355);
  UiTheme::value(power_.chargingLikely() ? "Probabile ricarica" : "Stato non misurabile", 34, 392, false);
  UiTheme::detail(String("Trend ") + String(power_.batteryTrendDeltaMv()) + " mV / finestra campioni", 34, 430);
  UiTheme::detail("USB/charger state is not exposed by M5Paper V1 hardware.", 34, 453);
}

void UiManager::begin() {
  // The boot splash already refreshed the complete panel; avoid a second
  // quality wipe during startup.
  showHome(false);
}

void UiManager::statusBar() {
  const int w = M5.Display.width();
  auto dt = M5.Rtc.getDateTime();

  M5.Display.fillRect(0, 0, w, UiTheme::StatusH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::StatusH - 1, w, TFT_BLACK);

  char clockText[8];
  snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(clockText, 14, 15);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  String dateText;
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    char b[14];
    snprintf(b, sizeof(b), "%02d/%02d/%02d", dt.date.date, dt.date.month, dt.date.year % 100);
    dateText = b;
  } else {
    dateText = "RTC SET";
  }
  M5.Display.drawString(dateText, 92, 20);

  const int iconY = 19;
  UiTheme::wifiIcon(350, iconY, wifi_.isConnected());
  UiTheme::bluetoothIcon(382, iconY - 1, bluetoothActive_);
  UiTheme::sdIcon(416, iconY - 3, storage_.available());
  UiTheme::batteryIcon(493, iconY + 1, power_.batteryPercent());

  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(top_right);
  M5.Display.drawString(String(power_.batteryPercent()) + "%", 486, 20);
  UiTheme::resetFont();
}

void UiManager::bottomNav(int active) {
  M5.Display.fillRect(0, UiTheme::NavY, UiTheme::ScreenW, UiTheme::NavH, TFT_WHITE);
  M5.Display.drawFastHLine(0, UiTheme::NavY, UiTheme::ScreenW, TFT_BLACK);
  const char* labels[5] = {"Home", "Notes", "Files", "Tools", "Apps"};
  for (int i = 0; i < 5; ++i) UiTheme::navItem(i, labels[i], i == active);
}

void UiManager::homeCard(int x, int y, int w, int h, const String& titleText, const String& mainValue, const String& detailText, bool withChevron) {
  UiTheme::card(x, y, w, h);
  UiTheme::label(titleText, x + 16, y + 14);
  if (withChevron) UiTheme::chevron(x + w - 28, y + 18);
  UiTheme::value(mainValue, x + 16, y + 47, false);
  UiTheme::detail(detailText, x + 16, y + h - 32);
}

void UiManager::showHome(bool forceClean) {
  page_ = Page::Home;
  preparePage(forceClean);
  statusBar();

  UiTheme::title("PaperOS", 18, 76);
  UiTheme::detail(config_.get().deviceName + "  /  " + VERSION, 20, 116);
  UiTheme::pill("ALPHA", 436, 79, true);

  UiTheme::card(18, 145, 504, 128, true);
  UiTheme::label("CONNECTION", 34, 160);
  String netName = wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup");
  if (netName.length() > 22) netName = netName.substring(0, 22);
  UiTheme::value(netName, 34, 194, false);
  UiTheme::detail(wifi_.isConnected() ? wifi_.ip().toString() : String("192.168.4.1 / setup"), 34, 232);

  M5.Display.drawFastVLine(366, 160, 94, TFT_BLACK);
  UiTheme::label("BATTERY", 386, 160);
  UiTheme::value(String(power_.batteryPercent()) + "%", 386, 194, false);
  UiTheme::detail(String(power_.batteryMillivolts()) + " mV", 386, 232);
  UiTheme::chevron(496, 190);

  UiTheme::label("QUICK ACCESS", 20, 294);
  homeCard(18, 322, 246, 108, "NOTES", storage_.available() ? "Open notes" : "Needs SD", "Notes on microSD", true);
  homeCard(276, 322, 246, 108, "FILES", storage_.available() ? "Browse files" : "Needs SD", "/PaperOS", true);
  homeCard(18, 442, 246, 108, "SETTINGS", "Configure", "Wi-Fi / display / power", true);
  homeCard(276, 442, 246, 108, "SYSTEM", String(ESP.getFreeHeap() / 1024) + " KB free", "Diagnostics", true);

  UiTheme::label("MODULES", 20, 574);
  homeCard(18, 604, 246, 102, "TERMO", "Coming Soon", "Heating dashboard", true);
  homeCard(276, 604, 246, 102, "SOLAR", "Coming Soon", "Energy dashboard", true);

  UiTheme::card(18, 720, 504, 130);
  UiTheme::label("WEB CONSOLE", 34, 737);
  UiTheme::value("paperos.local", 34, 770, false);
  UiTheme::detail("Files / Notes / Wi-Fi / Settings / OTA", 34, 809);
  UiTheme::pill("WEB", 440, 735, true);

  bottomNav(0);
  commitPage();
}

void UiManager::showApps() {
  page_ = Page::Apps;
  preparePage();
  statusBar();

  UiTheme::title("Apps", 18, 76);
  UiTheme::detail("Buffered launcher  /  one-pass e-paper render", 20, 116);

  const char* names[15] = {
    "Home", "Notes", "Files",
    "Calculator", "Wi-Fi", "Bluetooth",
    "Termo", "Solar", "Battery",
    "Clock", "Focus", "Settings",
    "System", "Labs", "Fun"
  };
  const char* glyphs[15] = {
    "HM", "NT", "FL",
    "CAL", "WF", "BT",
    "TH", "SL", "BAT",
    "CK", "25", "ST",
    "SYS", "LAB", "FUN"
  };
  const bool ready[15] = {
    true, true, true,
    true, true, true,
    false, false, true,
    true, true, true,
    true, true, true
  };

  const int tileW = 160;
  const int tileH = 108;
  const int startY = 154;
  const int pitchY = 116;
  for (int i = 0; i < 15; ++i) {
    int col = i % 3;
    int row = i / 3;
    int x = 18 + col * 172;
    int y = startY + row * pitchY;
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], !ready[i]);
  }

  UiTheme::card(18, 746, 504, 104);
  UiTheme::label("BUFFERED UI", 34, 762);
  UiTheme::detail("The complete page is composed off-screen before one refresh.", 34, 797);
  UiTheme::detail("Clock / Focus / Fun are lightweight native e-paper apps.", 34, 826);

  bottomNav(4);
  commitPage();
}

void UiManager::settingRow(int y, const String& titleText, const String& detailText) {
  UiTheme::card(18, y, 504, 88);
  UiTheme::value(titleText, 34, y + 17, false);
  UiTheme::detail(detailText, 34, y + 56);
  UiTheme::chevron(491, y + 34);
}


void UiManager::settingsRow(int y, const String& glyph, const String& titleText, const String& detailText, bool withChevron) {
  // Mobile-style grouped row: no nested rounded border per item.
  M5.Display.fillRect(22, y, 496, 78, TFT_WHITE);
  M5.Display.fillRoundRect(34, y + 15, 46, 46, 9, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(glyph, 57, y + 38);
  UiTheme::resetFont();

  UiTheme::value(titleText, 96, y + 11, false);
  if (detailText.length()) UiTheme::detail(detailText, 96, y + 47);
  M5.Display.drawFastHLine(96, y + 77, 404, TFT_BLACK);
  if (withChevron) UiTheme::chevron(490, y + 29);
}

void UiManager::showSettings() {
  page_ = Page::Settings;
  preparePage();
  statusBar();

  UiTheme::title("Impostazioni", 18, 76);
  UiTheme::detail("PaperOS system settings", 20, 116);

  UiTheme::card(18, 145, 504, 96, true);
  M5.Display.fillCircle(66, 193, 29, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("P", 66, 193);
  UiTheme::resetFont();
  UiTheme::value(config_.get().deviceName, 112, 164, false);
  UiTheme::detail(String("PaperOS ") + VERSION + " / WinLabs Solutions", 112, 202);
  UiTheme::chevron(490, 183);

  UiTheme::card(18, 257, 504, 250);
  settingsRow(260, "GN", "Generali", "Info dispositivo, firmware e memoria");
  settingsRow(342, "WF", "Wi-Fi", wifi_.isConnected() ? WiFi.SSID() : String("Non connesso"));
  settingsRow(424, "BT", "Bluetooth", bluetoothActive_ ? "Pronto / BLE inizializzato" : "Pronto");

  UiTheme::card(18, 522, 504, 250);
  settingsRow(525, "PW", "Batteria e Sleep", String(power_.batteryPercent()) + "%  /  " + String(power_.batteryMillivolts()) + " mV");
  settingsRow(607, "DS", "Display", "540x960 / fastest UI / anti-ghost");
  settingsRow(689, "LB", "Labs", "Funzioni beta WinLabs");

  bottomNav(4);
  commitPage();
}



void UiManager::showGeneral() {
  page_ = Page::General;
  preparePage();
  statusBar();

  UiTheme::title("Generali", 18, 78);
  UiTheme::detail("Info dispositivo", 20, 119);

  settingsRow(150, "IN", "Info", String("PaperOS ") + VERSION, false);
  settingsRow(240, "HW", "Modello", "M5Stack M5Paper original", false);
  settingsRow(330, "CPU", "Processore", "ESP32-D0WDQ6-V3 / 240 MHz", false);
  settingsRow(420, "RM", "Memoria", String(ESP.getPsramSize() / 1048576.0, 0) + " MB PSRAM", false);
  settingsRow(510, "FL", "Flash", "16 MB", false);
  settingsRow(600, "DP", "Display", "540 x 960 e-paper / 4.7 inch", false);
  settingsRow(690, "WL", "Software", "Created by WinLabs Solutions", false);

  UiTheme::card(18, 790, 504, 60);
  UiTheme::detail(String("Build: ") + VERSION + "  /  " + VENDOR, 34, 811);

  bottomNav(4);
  commitPage();
}

void UiManager::showWiFi() {
  page_ = Page::WiFi;
  preparePage();
  statusBar();

  UiTheme::title("Wi-Fi", 18, 76);
  UiTheme::detail(wifi_.isConnected() ? WiFi.SSID() : String("Non connesso"), 20, 116);

  UiTheme::card(18, 145, 504, 112, true);
  UiTheme::label("CONNECTION", 34, 161);
  String current = wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup");
  if (current.length() > 24) current = current.substring(0, 24);
  UiTheme::value(current, 34, 197, false);
  UiTheme::detail(wifi_.isConnected() ? wifi_.ip().toString() : String("Setup AP active when needed"), 34, 232);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "AP", 402, 160, false);

  UiTheme::label("AVAILABLE NETWORKS", 20, 282);
  UiTheme::card(18, 306, 504, 470);
  UiTheme::value("Scanning Wi-Fi...", 44, 350, false);
  UiTheme::detail("Network scan runs in background.", 44, 392);
  M5.Display.drawRoundRect(44, 442, 438, 18, 9, TFT_BLACK);
  M5.Display.fillRoundRect(47, 445, 260, 12, 6, TFT_BLACK);
  UiTheme::detail("Results replace this panel automatically.", 44, 486);

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail("Passwords and saved networks: paperos.local > Wi-Fi", 34, 811);

  bottomNav(4);
  commitPage();

  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
}

void UiManager::showBluetooth() {
  page_ = Page::Bluetooth;
  preparePage();
  statusBar();

  UiTheme::title("Bluetooth", 18, 76);
  UiTheme::detail("BLE explorer", 20, 116);

  if (!bluetoothActive_) {
    BLEDevice::init("PaperOS");
    bluetoothActive_ = true;
  }

  UiTheme::card(18, 145, 504, 112, true);
  UiTheme::label("BLUETOOTH LE", 34, 161);
  UiTheme::value("Scanner ready", 34, 197, false);
  UiTheme::detail("Tap this card to scan nearby BLE devices.", 34, 232);
  UiTheme::pill("SCAN", 430, 160, true);

  UiTheme::label("DEVICES", 20, 282);
  UiTheme::card(18, 306, 504, 470);
  UiTheme::value("Ready", 44, 350, false);
  UiTheme::detail("A loading panel appears while the radio scans.", 44, 392);
  UiTheme::detail("The final list is then drawn in one regional refresh.", 44, 426);

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail("BLE scan is a diagnostic feature in this alpha.", 34, 811);

  bottomNav(4);
  commitPage();
}


void UiManager::showBattery() {
  page_ = Page::Battery;
  preparePage();
  statusBar();

  UiTheme::title("Batteria", 18, 76);
  UiTheme::detail("Power Center  /  M5Paper original", 20, 116);

  drawBatteryLiveArea();

  UiTheme::card(18, 486, 246, 120);
  UiTheme::label("CURRENT", 34, 503);
  UiTheme::value("N/D", 34, 540, false);
  UiTheme::detail("No current sensor", 34, 575);

  UiTheme::card(276, 486, 246, 120);
  UiTheme::label("CAPACITY", 292, 503);
  UiTheme::value(String(power_.nominalBatteryCapacityMah()) + " mAh", 292, 540, false);
  UiTheme::detail("Nominal battery", 292, 575);

  UiTheme::card(18, 620, 504, 112);
  UiTheme::label("USB / CHARGING", 34, 637);
  UiTheme::value("5 V / 500 mA max input", 34, 672, false);
  UiTheme::detail("Actual charge mA and charger state are not measurable on M5Paper V1.", 34, 708);

  UiTheme::card(18, 746, 504, 104);
  UiTheme::label("SLEEP & WAKE", 34, 763);
  UiTheme::detail(String("Auto sleep: ") + config_.get().sleepMinutes + " min  /  Touch wake: " +
                  (config_.get().touchWakeEnabled ? "ON" : "OFF"), 34, 798);
  UiTheme::detail(String("Last boot/wake: ") + power_.wakeReason(), 34, 828);

  bottomNav(3);
  commitPage();
  lastBatteryUiRefresh_ = millis();
}


void UiManager::drawClockLiveArea() {
  auto dt = M5.Rtc.getDateTime();
  M5.Display.fillRect(18, 145, 504, 410, TFT_WHITE);
  UiTheme::card(18, 145, 504, 410, true);

  char timeText[8];
  char dateText[32];
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    snprintf(timeText, sizeof(timeText), "%02d:%02d", dt.time.hours, dt.time.minutes);
    snprintf(dateText, sizeof(dateText), "%02d/%02d/%04d", dt.date.date, dt.date.month, dt.date.year);
  } else {
    snprintf(timeText, sizeof(timeText), "--:--");
    snprintf(dateText, sizeof(dateText), "RTC not set");
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(timeText, 270, 285);
  UiTheme::resetFont();

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(dateText, 270, 365);
  UiTheme::resetFont();

  UiTheme::detail(String("Battery ") + power_.batteryPercent() + "%  /  " + power_.batteryMillivolts() + " mV", 168, 440);
  UiTheme::pill(wifi_.isConnected() ? "ONLINE" : "OFFLINE", 205, 480, wifi_.isConnected());
}

void UiManager::showClock() {
  page_ = Page::Clock;
  preparePage();
  statusBar();
  UiTheme::title("Clock", 18, 76);
  UiTheme::detail("Desk clock  /  minute refresh", 20, 116);
  drawClockLiveArea();

  UiTheme::card(18, 575, 504, 160);
  UiTheme::label("E-PAPER MODE", 34, 593);
  UiTheme::value("Quiet clock", 34, 630, false);
  UiTheme::detail("Only the clock panel refreshes once per minute.", 34, 675);
  UiTheme::detail("Ideal as a desk display without constant screen activity.", 34, 708);

  bottomNav(4);
  commitPage();
  lastClockPageRefresh_ = millis();
}

void UiManager::drawFocusLiveArea() {
  uint32_t remaining = focusRemainingSec_;
  if (focusRunning_) {
    int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
    remaining = diff > 0 ? static_cast<uint32_t>(diff) / 1000UL : 0;
  }

  const uint32_t mins = remaining / 60UL;
  const uint32_t secs = remaining % 60UL;
  char timerText[12];
  snprintf(timerText, sizeof(timerText), "%02lu:%02lu",
           static_cast<unsigned long>(mins),
           static_cast<unsigned long>(secs));

  M5.Display.fillRect(18, 145, 504, 350, TFT_WHITE);
  UiTheme::card(18, 145, 504, 350, true);
  UiTheme::label("FOCUS SESSION", 34, 164);
  UiTheme::pill(focusRunning_ ? "RUNNING" : (remaining == 0 ? "DONE" : "READY"), 382, 158, focusRunning_);

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(timerText, 270, 292);
  UiTheme::resetFont();

  UiTheme::detail("25 minute focus timer designed for low-refresh e-paper.", 73, 385);
  UiTheme::detail(focusRunning_ ? "PaperOS updates this timer roughly once per minute." : "Tap START to begin.", 79, 425);
}

void UiManager::showFocus() {
  page_ = Page::Focus;
  preparePage();
  statusBar();
  UiTheme::title("Focus", 18, 76);
  UiTheme::detail("25 minute concentration timer", 20, 116);

  drawFocusLiveArea();

  UiTheme::card(18, 515, 246, 112, true);
  UiTheme::value(focusRunning_ ? "PAUSE" : "START", 79, 553, false);
  UiTheme::detail("Tap", 121, 592);

  UiTheme::card(276, 515, 246, 112);
  UiTheme::value("RESET", 345, 553, false);
  UiTheme::detail("25:00", 379, 592);

  UiTheme::card(18, 646, 504, 118);
  UiTheme::label("TIP", 34, 664);
  UiTheme::detail("Use the full 25 minutes on one task, then take a short break.", 34, 703);
  UiTheme::detail("The timer keeps running while you open other PaperOS apps.", 34, 735);

  bottomNav(4);
  commitPage();
  lastFocusUiRefresh_ = millis();
}

void UiManager::drawFunResult() {
  M5.Display.fillRect(18, 145, 504, 230, TFT_WHITE);
  UiTheme::card(18, 145, 504, 230, true);
  UiTheme::label("RESULT", 34, 164);

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(funResult_, 270, 270);
  UiTheme::resetFont();
}

void UiManager::showFun() {
  page_ = Page::Fun;
  preparePage();
  statusBar();
  UiTheme::title("Fun", 18, 76);
  UiTheme::detail("Tiny random tools", 20, 116);

  drawFunResult();

  UiTheme::card(18, 400, 160, 118, true);
  UiTheme::value("D6", 74, 438, false);
  UiTheme::detail("Roll dice", 54, 480);

  UiTheme::card(190, 400, 160, 118, true);
  UiTheme::value("COIN", 232, 438, false);
  UiTheme::detail("Heads / tails", 214, 480);

  UiTheme::card(362, 400, 160, 118, true);
  UiTheme::value("1-100", 397, 438, false);
  UiTheme::detail("Random", 408, 480);

  UiTheme::card(18, 545, 504, 150);
  UiTheme::label("QUICK DECIDER", 34, 564);
  UiTheme::detail("Useful when nobody wants to choose.", 34, 606);
  UiTheme::detail("Uses the ESP32 hardware random source.", 34, 641);
  UiTheme::detail("Only the result card refreshes after a tap.", 34, 675);

  bottomNav(4);
  commitPage();
}

void UiManager::showLabs() {
  page_ = Page::Labs;
  preparePage();
  statusBar();

  UiTheme::title("Labs", 18, 78);
  UiTheme::detail("WinLabs Solutions beta features", 20, 119);

  UiTheme::card(18, 150, 504, 84, true);
  UiTheme::pill("LABS", 34, 172, true);
  UiTheme::detail("Funzioni sperimentali. Possono cambiare tra versioni.", 130, 180);

  settingsRow(252, "USB", "USB HID / BadUSB Lab", "Script library / external HID required");
  settingsRow(342, "BLE", "BLE Explorer", "Scan advertising / diagnostics");
  settingsRow(432, "IO", "GPIO Lab", "Pin tools - beta");
  settingsRow(522, "SER", "Serial Lab", "UART console - beta");
  settingsRow(612, "EPD", "Display Test", "Refresh / ghosting diagnostics");
  settingsRow(702, "DEV", "Developer", "Logs / heap / experimental tools");

  bottomNav(4);
  commitPage();
}

void UiManager::showHidLab() {
  page_ = Page::HidLab;
  preparePage();
  statusBar();

  UiTheme::title("USB HID Lab", 18, 78);
  UiTheme::detail("Labs / script library", 20, 119);

  UiTheme::card(18, 150, 504, 150, true);
  UiTheme::label("HARDWARE LIMIT", 34, 168);
  UiTheme::value("External HID adapter required", 34, 204, false);
  UiTheme::detail("M5Paper USB-C is USB-to-serial, not native HID.", 34, 246);
  UiTheme::detail("Scripts can be stored here but are not executed over USB-C.", 34, 274);

  UiTheme::label("SCRIPT LIBRARY", 20, 326);
  int row = 0;
  if (storage_.available()) {
    File root = SD.open("/PaperOS/Labs/HID");
    for (File file = root.openNextFile(); file && row < 5; file = root.openNextFile()) {
      if (!file.isDirectory()) {
        String n = String(file.name());
        int slash = n.lastIndexOf('/');
        if (slash >= 0) n = n.substring(slash + 1);
        settingsRow(356 + row * 82, "SC", n, String((uint32_t)file.size()) + " bytes", false);
        ++row;
      }
      file.close();
    }
    root.close();
  }
  if (row == 0) {
    UiTheme::card(18, 356, 504, 118);
    UiTheme::value("Nessuno script", 34, 390, false);
    UiTheme::detail("Carica file in /PaperOS/Labs/HID dal File Manager web.", 34, 432);
  }

  UiTheme::card(18, 786, 504, 64);
  UiTheme::detail("Execution on internal USB-C: unavailable on this hardware", 34, 808);

  bottomNav(4);
  commitPage();
}

void UiManager::showCalculator() {
  page_ = Page::Calculator;
  preparePage();
  statusBar();

  UiTheme::title("Calculator", 18, 78);
  UiTheme::detail("PaperOS utility", 20, 119);

  UiTheme::card(18, 145, 504, 100, true);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_right);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(calcDisplay_, 500, 195);
  UiTheme::resetFont();

  const char* keys[20] = {
    "C", "+/-", "%", "/",
    "7", "8", "9", "*",
    "4", "5", "6", "-",
    "1", "2", "3", "+",
    "0", ".", "=", "BS"
  };
  const int sx = 18, sy = 266, kw = 117, kh = 92, gap = 12;
  for (int i = 0; i < 20; ++i) {
    int col = i % 4, row = i / 4;
    int x = sx + col * (kw + gap);
    int y = sy + row * (kh + gap);
    UiTheme::card(x, y, kw, kh, keys[i][0] == '=');
    M5.Display.setFont(&fonts::FreeSansBold12pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.drawString(keys[i], x + kw/2, y + kh/2);
    UiTheme::resetFont();
  }

  bottomNav(4);
  commitPage();
}

void UiManager::calcKey(const String& key) {
  if (key == "C") {
    calcDisplay_ = "0"; calcAccumulator_ = 0; calcPendingOp_ = 0; calcResetInput_ = true; return;
  }
  if (key == "BS") {
    if (calcDisplay_.length() > 1) calcDisplay_.remove(calcDisplay_.length()-1);
    else calcDisplay_ = "0";
    return;
  }
  if (key == "+/-") {
    if (calcDisplay_.startsWith("-")) calcDisplay_.remove(0,1);
    else if (calcDisplay_ != "0") calcDisplay_ = "-" + calcDisplay_;
    return;
  }
  if (key == "%") {
    double v = calcDisplay_.toDouble() / 100.0;
    calcDisplay_ = String(v, 6);
    while (calcDisplay_.endsWith("0")) calcDisplay_.remove(calcDisplay_.length()-1);
    if (calcDisplay_.endsWith(".")) calcDisplay_.remove(calcDisplay_.length()-1);
    return;
  }
  if (key == "+" || key == "-" || key == "*" || key == "/" || key == "=") {
    double current = calcDisplay_.toDouble();
    if (calcPendingOp_) {
      if (calcPendingOp_ == '+') calcAccumulator_ += current;
      else if (calcPendingOp_ == '-') calcAccumulator_ -= current;
      else if (calcPendingOp_ == '*') calcAccumulator_ *= current;
      else if (calcPendingOp_ == '/' && current != 0) calcAccumulator_ /= current;
    } else calcAccumulator_ = current;
    calcDisplay_ = String(calcAccumulator_, 6);
    while (calcDisplay_.endsWith("0")) calcDisplay_.remove(calcDisplay_.length()-1);
    if (calcDisplay_.endsWith(".")) calcDisplay_.remove(calcDisplay_.length()-1);
    calcPendingOp_ = key == "=" ? 0 : key[0];
    calcResetInput_ = true;
    return;
  }

  if (calcResetInput_) { calcDisplay_ = "0"; calcResetInput_ = false; }
  if (key == "." && calcDisplay_.indexOf('.') >= 0) return;
  if (calcDisplay_ == "0" && key != ".") calcDisplay_ = key;
  else if (calcDisplay_.length() < 14) calcDisplay_ += key;
}

void UiManager::showSystem() {
  page_ = Page::System;
  preparePage();
  statusBar();

  UiTheme::title("System", 18, 78);
  UiTheme::detail(String("PaperOS ") + VERSION, 20, 119);

  homeCard(18, 150, 246, 126, "HEAP", String(ESP.getFreeHeap() / 1024) + " KB", String("Min ") + ESP.getMinFreeHeap() / 1024 + " KB", false);
  homeCard(276, 150, 246, 126, "PSRAM", String(ESP.getFreePsram() / 1048576.0, 1) + " MB", String(ESP.getPsramSize() / 1048576.0, 1) + " MB total", false);
  homeCard(18, 290, 246, 126, "BATTERY", String(power_.batteryPercent()) + "%", String(power_.batteryMillivolts()) + " mV", false);
  homeCard(276, 290, 246, 126, "UPTIME", String(millis() / 60000UL) + " min", "Since boot", false);

  UiTheme::card(18, 430, 504, 126);
  UiTheme::label("NETWORK", 34, 447);
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 480, false);
  UiTheme::detail(wifi_.ip().toString(), 34, 523);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "AP MODE", 392, 442, false);

  UiTheme::card(18, 570, 504, 126);
  UiTheme::label("STORAGE", 34, 587);
  UiTheme::value(storage_.available() ? "microSD mounted" : "microSD unavailable", 34, 620, false);
  UiTheme::detail(storage_.available() ? String(storage_.freeBytes() / 1048576.0, 1) + " MB free" : "Insert a microSD for Files and Notes", 34, 663);

  UiTheme::card(18, 710, 504, 132);
  UiTheme::label("DISPLAY", 34, 727);
  UiTheme::value(String(display_.pageChangesSinceClean()) + " page changes", 34, 760, false);
  UiTheme::detail("Tools > Clean Display performs an anti-ghosting wipe.", 34, 802);

  bottomNav(3);
  commitPage();
}

void UiManager::showNotes() {
  page_ = Page::Notes;
  noteIds_.clear();
  noteTitles_.clear();
  preparePage();
  statusBar();

  UiTheme::title("Notes", 18, 78);
  UiTheme::detail("Stored on microSD", 20, 119);

  // Show the complete app shell immediately; SD enumeration happens after.
  UiTheme::card(18, 150, 504, 690);
  UiTheme::label("LOADING NOTES", 40, 180);
  UiTheme::value("Reading microSD...", 40, 220, false);
  M5.Display.drawRoundRect(40, 282, 460, 18, 9, TFT_BLACK);
  M5.Display.fillRoundRect(43, 285, 240, 12, 6, TFT_BLACK);
  bottomNav(1);
  commitPage();

  M5.Display.fillRect(18, 150, 504, 690, TFT_WHITE);

  if (!storage_.available()) {
    UiTheme::card(18, 160, 504, 260, true);
    UiTheme::value("microSD required", 40, 205, true);
    UiTheme::detail("Insert a card, then reopen Notes.", 40, 270);
    UiTheme::detail("PaperOS will create /PaperOS/Notes automatically.", 40, 304);
  } else {
    File root = SD.open("/PaperOS/Notes");
    int row = 0;
    for (File f = root.openNextFile(); f && row < 8; f = root.openNextFile()) {
      if (!f.isDirectory()) {
        DynamicJsonDocument doc(4096);
        if (!deserializeJson(doc, f)) {
          String id = doc["id"] | String(f.name());
          String title = doc["title"] | "Untitled";
          noteIds_.push_back(id);
          noteTitles_.push_back(title);
          int y = 150 + row * 82;
          UiTheme::card(18, y, 504, 70);
          UiTheme::value(title, 34, y + 13, false);
          String cat = doc["category"] | "";
          UiTheme::detail(cat.length() ? cat : "Note", 34, y + 46);
          UiTheme::chevron(491, y + 25);
          ++row;
        }
      }
      f.close();
    }
    root.close();

    if (noteTitles_.empty()) {
      UiTheme::card(18, 160, 504, 220);
      UiTheme::value("No notes yet", 40, 205, true);
      UiTheme::detail("Create your first note from paperos.local.", 40, 270);
    }
  }

  display_.partialRefresh(18, 150, 504, 690);
}

void UiManager::showNote(size_t index) {
  if (index >= noteIds_.size() || !storage_.available()) return;
  page_ = Page::NoteView;
  preparePage();
  statusBar();

  String id = noteIds_[index];
  String path = "/PaperOS/Notes/" + id + ".json";
  File f = SD.open(path, FILE_READ);

  String titleText = noteTitles_[index];
  String bodyText;
  String categoryText;

  if (f) {
    DynamicJsonDocument doc(8192);
    if (!deserializeJson(doc, f)) {
      titleText = String((const char*)(doc["title"] | "Untitled"));
      bodyText = String((const char*)(doc["body"] | ""));
      categoryText = String((const char*)(doc["category"] | ""));
    }
    f.close();
  }

  UiTheme::title(titleText, 18, 78);
  UiTheme::detail(categoryText.length() ? categoryText : "Note", 20, 119);

  UiTheme::card(18, 150, 504, 690);
  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextWrap(true, true);
  M5.Display.setCursor(36, 180);
  String clipped = bodyText.substring(0, 1400);
  M5.Display.print(clipped);
  M5.Display.setTextWrap(false);
  UiTheme::resetFont();

  bottomNav(1);
  commitPage();
}

String UiManager::parentPath(const String& path) const {
  if (path == "/PaperOS") return "/PaperOS";
  int pos = path.lastIndexOf('/');
  if (pos <= 0) return "/PaperOS";
  String p = path.substring(0, pos);
  return p.startsWith("/PaperOS") ? p : String("/PaperOS");
}

String UiManager::joinPath(const String& base, const String& name) const {
  return base + (base.endsWith("/") ? "" : "/") + name;
}

bool UiManager::isTextFile(const String& name) const {
  String n = name;
  n.toLowerCase();
  return n.endsWith(".txt") || n.endsWith(".md") || n.endsWith(".json") || n.endsWith(".log") || n.endsWith(".csv");
}

void UiManager::showFiles(const String& path) {
  page_ = Page::Files;
  currentFilePath_ = storage_.validPath(path) ? path : String("/PaperOS");
  fileEntries_.clear();

  preparePage();
  statusBar();
  UiTheme::title("Files", 18, 78);
  UiTheme::detail(currentFilePath_, 20, 119);

  UiTheme::card(18, 150, 504, 690);
  UiTheme::label("LOADING FILES", 40, 180);
  UiTheme::value("Reading microSD...", 40, 220, false);
  M5.Display.drawRoundRect(40, 282, 460, 18, 9, TFT_BLACK);
  M5.Display.fillRoundRect(43, 285, 280, 12, 6, TFT_BLACK);
  bottomNav(2);
  commitPage();

  M5.Display.fillRect(18, 150, 504, 690, TFT_WHITE);

  if (!storage_.available()) {
    UiTheme::card(18, 160, 504, 260, true);
    UiTheme::value("microSD required", 40, 205, true);
    UiTheme::detail("Insert a card to browse local files.", 40, 270);
    UiTheme::detail("Browser storage functions also depend on microSD.", 40, 304);
  } else {
    int row = 0;
    if (currentFilePath_ != "/PaperOS") {
      FileEntry up;
      up.name = "..";
      up.directory = true;
      fileEntries_.push_back(up);
      UiTheme::card(18, 150, 504, 70);
      UiTheme::value("..", 34, 164, false);
      UiTheme::detail("Parent folder", 34, 197);
      UiTheme::chevron(491, 174);
      ++row;
    }

    File root = SD.open(currentFilePath_);
    for (File f = root.openNextFile(); f && row < 8; f = root.openNextFile()) {
      FileEntry e;
      e.name = String(f.name());
      int slash = e.name.lastIndexOf('/');
      if (slash >= 0) e.name = e.name.substring(slash + 1);
      e.directory = f.isDirectory();
      e.size = f.size();
      fileEntries_.push_back(e);

      int y = 150 + row * 82;
      UiTheme::card(18, y, 504, 70);
      UiTheme::value((e.directory ? "[DIR] " : "") + e.name, 34, y + 13, false);
      UiTheme::detail(e.directory ? "Folder" : String((uint32_t)e.size) + " bytes", 34, y + 46);
      UiTheme::chevron(491, y + 25);
      ++row;
      f.close();
    }
    root.close();

    if (fileEntries_.empty()) {
      UiTheme::card(18, 160, 504, 220);
      UiTheme::value("Empty folder", 40, 205, true);
      UiTheme::detail("Upload files from paperos.local.", 40, 270);
    }
  }

  display_.partialRefresh(18, 150, 504, 690);
}

void UiManager::showFilePreview(const String& fullPath, const String& name, uint64_t size) {
  page_ = Page::FileView;
  preparePage();
  statusBar();

  UiTheme::title(name, 18, 78);
  UiTheme::detail(String((uint32_t)size) + " bytes", 20, 119);

  UiTheme::card(18, 150, 504, 690);

  if (!isTextFile(name)) {
    UiTheme::value("Preview unavailable", 40, 205, true);
    UiTheme::detail("Binary/media files can be downloaded from paperos.local.", 40, 270);
  } else {
    File f = SD.open(fullPath, FILE_READ);
    String content;
    if (f) {
      while (f.available() && content.length() < 1800) content += (char)f.read();
      f.close();
    }
    M5.Display.setFont(&fonts::FreeMono9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(34, 178);
    M5.Display.print(content);
    M5.Display.setTextWrap(false);
    UiTheme::resetFont();
  }

  bottomNav(2);
  commitPage();
}

void UiManager::showTools() {
  page_ = Page::Tools;
  preparePage();
  statusBar();

  UiTheme::title("Tools", 18, 78);
  UiTheme::detail("Display & power controls", 20, 119);

  settingRow(150, "Clean Display", "Quality white wipe + redraw");
  settingRow(242, "System Monitor", "Memory / battery / network");
  settingRow(334, "Settings", "Display / power / Wi-Fi / storage");
  settingRow(426, "Sleep Now", "Deep sleep until touch/button");
  settingRow(518, "Sleep 15 min", "Touch wake or timer wake");
  settingRow(610, "Browser Console", String("http://") + HOSTNAME + ".local");

  UiTheme::card(18, 714, 504, 128);
  UiTheme::label("REFRESH ENGINE", 34, 730);
  UiTheme::detail("Page changes: fast text mode", 34, 765);
  UiTheme::detail("Automatic anti-ghost clean after repeated navigation", 34, 798);
  UiTheme::detail("Manual Clean Display is always available here.", 34, 828);

  bottomNav(3);
  commitPage();
}

void UiManager::showThermo() {
  page_ = Page::ComingSoon;
  comingTitle_ = "Termo";
  comingNav_ = 4;

  preparePage();
  statusBar();
  UiTheme::title("Termo", 18, 78);
  UiTheme::detail("Heating & thermal dashboard", 20, 119);

  UiTheme::card(18, 150, 504, 118, true);
  UiTheme::label("STATUS", 34, 166);
  UiTheme::pill("COMING SOON", 374, 160, false);
  UiTheme::value("Thermal control center", 34, 203, false);
  UiTheme::detail("No simulated temperatures are shown.", 34, 238);

  homeCard(18, 282, 246, 120, "ROOM", "--.- C", "SHT30 / remote sensor", false);
  homeCard(276, 282, 246, 120, "HUMIDITY", "-- %", "Local / remote sensor", false);
  homeCard(18, 416, 246, 120, "PUFFER TOP", "--.- C", "Planned telemetry", false);
  homeCard(276, 416, 246, 120, "PUFFER BOTTOM", "--.- C", "Planned telemetry", false);

  UiTheme::card(18, 550, 504, 154);
  UiTheme::label("PLANNED CONNECTIONS", 34, 568);
  UiTheme::detail("ESP32 / MQTT / HTTP / serial bridge", 34, 606);
  UiTheme::detail("Pellet boiler state + puffer resistances", 34, 640);
  UiTheme::detail("Automations will activate only after real sensor binding.", 34, 674);

  UiTheme::card(18, 718, 504, 124);
  UiTheme::label("SAFETY", 34, 736);
  UiTheme::detail("Control outputs remain disabled in this alpha.", 34, 774);
  UiTheme::detail("Placeholders show -- instead of invented measurements.", 34, 808);

  bottomNav(4);
  commitPage();
}

void UiManager::showSolar() {
  page_ = Page::ComingSoon;
  comingTitle_ = "Solar";
  comingNav_ = 4;

  preparePage();
  statusBar();
  UiTheme::title("Solar", 18, 78);
  UiTheme::detail("Energy dashboard", 20, 119);

  UiTheme::card(18, 150, 504, 118, true);
  UiTheme::label("STATUS", 34, 166);
  UiTheme::pill("COMING SOON", 374, 160, false);
  UiTheme::value("PV energy center", 34, 203, false);
  UiTheme::detail("Waiting for a real inverter data source.", 34, 238);

  homeCard(18, 282, 246, 120, "PV POWER", "--.- kW", "Planned live value", false);
  homeCard(276, 282, 246, 120, "HOME", "--.- kW", "Planned consumption", false);
  homeCard(18, 416, 246, 120, "BATTERY SOC", "-- %", "Planned inverter / BMS", false);
  homeCard(276, 416, 246, 120, "GRID", "--.- kW", "Import / export", false);

  UiTheme::card(18, 550, 504, 154);
  UiTheme::label("PLANNED DETAIL", 34, 568);
  UiTheme::detail("MPPT1 / MPPT2 / battery V+A / daily yield", 34, 606);
  UiTheme::detail("MQTT / HTTP / serial sources supported", 34, 640);
  UiTheme::detail("No fake production numbers are displayed.", 34, 674);

  UiTheme::card(18, 718, 504, 124);
  UiTheme::label("AUTOMATION READY", 34, 736);
  UiTheme::detail("Future surplus rules can drive puffer heating.", 34, 774);
  UiTheme::detail("Rules stay disabled until telemetry is connected.", 34, 808);

  bottomNav(4);
  commitPage();
}

void UiManager::showComingSoon(const String& titleText, int activeNav) {
  page_ = Page::ComingSoon;
  comingTitle_ = titleText;
  comingNav_ = activeNav;

  preparePage();
  statusBar();
  UiTheme::title(titleText, 18, 78);
  UiTheme::detail("PaperOS module", 20, 119);

  UiTheme::card(18, 170, 504, 340, true);
  UiTheme::value("Coming Soon", 150, 255, true);
  UiTheme::detail("This feature is not simulated.", 140, 325);
  UiTheme::detail("It will be enabled when its backend is real.", 92, 365);

  UiTheme::card(18, 530, 504, 180);
  UiTheme::label("AVAILABLE NOW", 34, 550);
  UiTheme::detail("Notes / Files / Wi-Fi / Settings / System / Tools", 34, 590);
  UiTheme::detail("Browser console also provides OTA and file management.", 34, 628);

  bottomNav(activeNav);
  commitPage();
}

void UiManager::handleBottomNav(int x) {
  int index = constrain(x / (UiTheme::ScreenW / 5), 0, 4);
  if (index == 0) showHome();
  else if (index == 1) showNotes();
  else if (index == 2) showFiles();
  else if (index == 3) showTools();
  else showApps();
}

void UiManager::openAppIndex(int index) {
  if (index == 0) showHome();
  else if (index == 1) showNotes();
  else if (index == 2) showFiles();
  else if (index == 3) showCalculator();
  else if (index == 4) showWiFi();
  else if (index == 5) showBluetooth();
  else if (index == 6) showThermo();
  else if (index == 7) showSolar();
  else if (index == 8) showBattery();
  else if (index == 9) showClock();
  else if (index == 10) showFocus();
  else if (index == 11) showSettings();
  else if (index == 12) showSystem();
  else if (index == 13) showLabs();
  else if (index == 14) showFun();
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

    // Battery indicator in the status bar is always a direct shortcut.
    if (e.y < UiTheme::StatusH && e.x >= 430) {
      showBattery();
      return;
    }

    if (e.y >= UiTheme::NavY) {
      handleBottomNav(e.x);
      return;
    }

    if (page_ == Page::Home) {
      if (e.y >= 145 && e.y < 273) {
        if (e.x >= 366) showBattery();
        else showWiFi();
      } else if (e.y >= 322 && e.y < 430) {
        if (e.x < 270) showNotes();
        else showFiles();
      } else if (e.y >= 442 && e.y < 550) {
        if (e.x < 270) showSettings();
        else showSystem();
      } else if (e.y >= 604 && e.y < 706) {
        if (e.x < 270) showThermo();
        else showSolar();
      } else if (e.y >= 720 && e.y < 850) showSettings();
      return;
    }

    if (page_ == Page::Apps) {
      const int startY = 154;
      const int pitchY = 116;
      if (e.y >= startY && e.y < startY + 5 * pitchY && e.x >= 18) {
        int row = (e.y - startY) / pitchY;
        int col = (e.x - 18) / 172;
        int localX = (e.x - 18) % 172;
        int localY = (e.y - startY) % pitchY;
        if (col >= 0 && col < 3 && localX < 160 && localY < 108) openAppIndex(row * 3 + col);
      }
      return;
    }

    if (page_ == Page::Notes) {
      if (e.y >= 150 && e.y < 150 + 8 * 82) {
        size_t idx = (e.y - 150) / 82;
        if (idx < noteIds_.size()) showNote(idx);
      }
      return;
    }

    if (page_ == Page::Files) {
      if (e.y >= 150 && e.y < 150 + 8 * 82) {
        size_t idx = (e.y - 150) / 82;
        if (idx < fileEntries_.size()) {
          const FileEntry& entry = fileEntries_[idx];
          if (entry.name == "..") {
            showFiles(parentPath(currentFilePath_));
          } else {
            String full = joinPath(currentFilePath_, entry.name);
            if (entry.directory) showFiles(full);
            else showFilePreview(full, entry.name, entry.size);
          }
        }
      }
      return;
    }

    if (page_ == Page::Tools) {
      if (e.y >= 150 && e.y < 238) {
        display_.cleanRefresh();
        showTools();
      } else if (e.y >= 242 && e.y < 330) {
        showSystem();
      } else if (e.y >= 334 && e.y < 422) {
        showSettings();
      } else if (e.y >= 426 && e.y < 514) {
        power_.sleepNow();
      } else if (e.y >= 518 && e.y < 606) {
        power_.sleepForMinutes(15);
      } else if (e.y >= 610 && e.y < 698) {
        showSettings();
      }
      return;
    }

    if (page_ == Page::Settings) {
      if (e.y >= 145 && e.y < 241) showGeneral();
      else if (e.y >= 260 && e.y < 338) showGeneral();
      else if (e.y >= 342 && e.y < 420) showWiFi();
      else if (e.y >= 424 && e.y < 502) showBluetooth();
      else if (e.y >= 525 && e.y < 603) showBattery();
      else if (e.y >= 607 && e.y < 685) showTools();
      else if (e.y >= 689 && e.y < 767) showLabs();
      return;
    }

    if (page_ == Page::WiFi) {
      showWiFi();
      return;
    }

    if (page_ == Page::Bluetooth) {
      if (e.y >= 145 && e.y < 257) {
        M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
        UiTheme::card(18, 306, 504, 470);
        UiTheme::value("Scanning BLE...", 44, 350, false);
        UiTheme::detail("Radio scan in progress.", 44, 392);
        M5.Display.drawRoundRect(44, 442, 438, 18, 9, TFT_BLACK);
        M5.Display.fillRoundRect(47, 445, 292, 12, 6, TFT_BLACK);
        UiTheme::detail("Results appear when the scan completes.", 44, 486);
        display_.partialRefresh(18, 306, 504, 470);

        BLEScan* scanner = BLEDevice::getScan();
        scanner->setActiveScan(true);
        BLEScanResults results = scanner->start(2, false);
        int shown = min(results.getCount(), 6);

        M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
        UiTheme::card(18, 306, 504, 470);
        if (shown == 0) {
          UiTheme::value("No BLE devices found", 44, 350, false);
          UiTheme::detail("Tap SCAN to try again.", 44, 392);
        } else {
          for (int i = 0; i < shown; ++i) {
            BLEAdvertisedDevice d = results.getDevice(i);
            String name = d.haveName() ? String(d.getName().c_str()) : String(d.getAddress().toString().c_str());
            if (name.length() > 24) name = name.substring(0, 24);
            settingsRow(307 + i * 78, "BT", name, String(d.getRSSI()) + " dBm", false);
          }
        }
        scanner->clearResults();
        display_.partialRefresh(18, 306, 504, 470);
      }
      return;
    }

    if (page_ == Page::Labs) {
      if (e.y >= 252 && e.y < 330) showHidLab();
      else if (e.y >= 342 && e.y < 420) showBluetooth();
      else if (e.y >= 432 && e.y < 510) showComingSoon("GPIO Lab", 4);
      else if (e.y >= 522 && e.y < 600) showComingSoon("Serial Lab", 4);
      else if (e.y >= 612 && e.y < 690) { display_.cleanRefresh(); showLabs(); }
      else if (e.y >= 702 && e.y < 780) showSystem();
      return;
    }

    if (page_ == Page::Calculator) {
      const int sx = 18, sy = 266, kw = 117, kh = 92, gap = 12;
      if (e.x >= sx && e.y >= sy) {
        int col = (e.x - sx) / (kw + gap);
        int row = (e.y - sy) / (kh + gap);
        int lx = (e.x - sx) % (kw + gap);
        int ly = (e.y - sy) % (kh + gap);
        if (col >= 0 && col < 4 && row >= 0 && row < 5 && lx < kw && ly < kh) {
          const char* keys[20] = {
            "C", "+/-", "%", "/",
            "7", "8", "9", "*",
            "4", "5", "6", "-",
            "1", "2", "3", "+",
            "0", ".", "=", "BS"
          };
          calcKey(keys[row * 4 + col]);

          M5.Display.fillRect(18, 145, 504, 100, TFT_WHITE);
          UiTheme::card(18, 145, 504, 100, true);
          M5.Display.setFont(&fonts::FreeSansBold24pt7b);
          M5.Display.setTextDatum(middle_right);
          M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
          M5.Display.drawString(calcDisplay_, 500, 195);
          UiTheme::resetFont();
          display_.partialRefresh(18, 145, 504, 100);
        }
      }
      return;
    }
  }

  // Complete asynchronous Wi-Fi scanning without blocking page entry.
  if (page_ == Page::WiFi) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      int shown = min(n, 6);
      M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
      UiTheme::card(18, 306, 504, 470);

      if (shown == 0) {
        UiTheme::value("No networks found", 44, 350, false);
        UiTheme::detail("Tap this page to scan again.", 44, 392);
      } else {
        for (int i = 0; i < shown; ++i) {
          String ssid = WiFi.SSID(i);
          if (ssid.length() > 24) ssid = ssid.substring(0, 24);
          settingsRow(307 + i * 78, "WF", ssid.length() ? ssid : String("<hidden>"), String(WiFi.RSSI(i)) + " dBm", false);
        }
      }

      display_.partialRefresh(18, 306, 504, 470);
      WiFi.scanDelete();
    }
  }

  // Battery screen is live, but e-paper is updated slowly enough to avoid
  // pointless refresh churn. Voltage/trend samples are collected in PowerManager.
  if (page_ == Page::Battery && millis() - lastBatteryUiRefresh_ > 30000UL) {
    lastBatteryUiRefresh_ = millis();
    M5.Display.fillRect(18, 145, 504, 325, TFT_WHITE);
    drawBatteryLiveArea();
    display_.partialRefresh(18, 145, 504, 325);
  }

  if (millis() - lastClock_ > 60000UL) {
    lastClock_ = millis();
    statusBar();
    display_.partialRefresh(0, 0, M5.Display.width(), UiTheme::StatusH);
  }

  if (millis() - lastDeepClean_ > 25UL * 60UL * 1000UL && page_ == Page::Home) {
    showHome(true);
  }
}

}

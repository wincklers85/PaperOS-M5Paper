#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <esp_system.h>
#include <time.h>

namespace paperos {

void UiManager::preparePage(bool forceClean) {
  // The user explicitly prefers a clean anti-ghost transition on every
  // full page change. Auto-display remains disabled, so after this white
  // cleanup the next page is still composed off-screen and appears at once.
  if (!firstPageFrame_ || forceClean) {
    display_.cleanRefresh();
    lastDeepClean_ = millis();
  }
  firstPageFrame_ = false;
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
  UiTheme::detail("PaperOS tools  /  buffered one-pass UI", 20, 116);

  const char* names[15] = {
    "Notes", "Files", "Calculator",
    "Wi-Fi", "Bluetooth", "Browser",
    "Battery", "Clock", "Focus",
    "OTP", "Network", "Settings",
    "System", "Labs", "Fun"
  };
  const char* glyphs[15] = {
    "NT", "FL", "CAL",
    "WF", "BT", "WEB",
    "BAT", "CK", "25",
    "OTP", "NET", "ST",
    "SYS", "LAB", "FUN"
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
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], false);
  }

  UiTheme::card(18, 746, 504, 104);
  UiTheme::label("NETWORK + SECURITY", 34, 762);
  UiTheme::detail("Wi-Fi, BLE, Web Reader, TOTP and Network Toolkit.", 34, 797);
  UiTheme::detail("Termo / Solar remain accessible from the Home modules.", 34, 826);

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
  settingsRow(607, "DS", "Display", "540x960 / clean transitions / buffered UI");
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
  wifiScan_.clear();
  preparePage();
  statusBar();

  UiTheme::title("Wi-Fi Analyzer", 18, 76);
  UiTheme::detail(wifi_.isConnected() ? WiFi.SSID() : String("Not connected"), 20, 116);

  UiTheme::card(18, 145, 504, 118, true);
  UiTheme::label("CURRENT NETWORK", 34, 161);
  String current = wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup");
  if (current.length() > 23) current = current.substring(0, 23);
  UiTheme::value(current, 34, 197, false);
  String networkDetail = wifi_.isConnected()
    ? wifi_.ip().toString() + "  /  CH " + String(WiFi.channel())
    : String("Tap a network below to connect");
  UiTheme::detail(networkDetail, 34, 235);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "OFFLINE", 392, 160, false);

  UiTheme::label("NEARBY NETWORKS", 20, 286);
  UiTheme::card(18, 310, 504, 466);
  UiTheme::value("Scanning...", 44, 354, false);
  UiTheme::detail("SSID / signal / channel / security", 44, 398);
  M5.Display.drawRoundRect(44, 448, 438, 18, 9, TFT_BLACK);
  M5.Display.fillRoundRect(47, 451, 250, 12, 6, TFT_BLACK);
  UiTheme::detail("Tap a result to join it.", 44, 493);

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail(wifiStatusMessage_.length() ? wifiStatusMessage_ : String("Saved networks reconnect automatically."), 34, 811);

  bottomNav(4);
  commitPage();

  WiFi.scanDelete();
  WiFi.scanNetworks(true, true);
}

void UiManager::showBluetooth() {
  page_ = Page::Bluetooth;
  bleScan_.clear();
  preparePage();
  statusBar();

  UiTheme::title("BLE Inspector", 18, 76);
  UiTheme::detail("Bluetooth Low Energy scanner", 20, 116);

  if (!bluetoothActive_) {
    BLEDevice::init("PaperOS");
    bluetoothActive_ = true;
  }

  UiTheme::card(18, 145, 504, 112, true);
  UiTheme::label("SCANNER", 34, 161);
  UiTheme::value("Ready", 34, 197, false);
  UiTheme::detail("Tap SCAN, then tap a device for technical details.", 34, 232);
  UiTheme::pill("SCAN", 430, 160, true);

  UiTheme::label("NEARBY BLE DEVICES", 20, 282);
  UiTheme::card(18, 306, 504, 470);
  UiTheme::value("No scan yet", 44, 350, false);
  UiTheme::detail("Shows name/address, RSSI, UUID and manufacturer bytes.", 44, 392);

  UiTheme::card(18, 792, 504, 58);
  UiTheme::detail("BLE Inspector is passive: it does not pair or write to devices.", 34, 811);

  bottomNav(4);
  commitPage();
}



String UiManager::bleManufacturerHex(const String& bytes) const {
  static const char* hex = "0123456789ABCDEF";
  String out;
  size_t limit = bytes.length() > 18 ? 18 : bytes.length();
  out.reserve(limit * 3);
  for (size_t i = 0; i < limit; ++i) {
    uint8_t b = static_cast<uint8_t>(bytes[i]);
    if (i) out += ' ';
    out += hex[(b >> 4) & 0x0F];
    out += hex[b & 0x0F];
  }
  if (bytes.length() > limit) out += " ...";
  return out;
}

void UiManager::showBleDetail(size_t index) {
  if (index >= bleScan_.size()) return;
  page_ = Page::BleDetail;
  preparePage();
  statusBar();

  const BleScanEntry& d = bleScan_[index];
  UiTheme::title("BLE Device", 18, 76);
  UiTheme::detail(d.name.length() ? d.name : d.address, 20, 116);

  settingRow(150, "Address", d.address);
  settingRow(242, "Signal", String(d.rssi) + " dBm");
  settingRow(334, "TX Power", d.hasTxPower ? String(d.txPower) + " dBm" : String("Not advertised"));
  settingRow(426, "Service UUID", d.serviceUuid.length() ? d.serviceUuid : String("Not advertised"));
  settingRow(518, "Manufacturer", d.manufacturerHex.length() ? d.manufacturerHex : String("No manufacturer data"));

  UiTheme::card(18, 630, 504, 150);
  UiTheme::label("USEFUL FOR", 34, 649);
  UiTheme::detail("Beacon checks / RSSI positioning / identifying BLE sensors.", 34, 687);
  UiTheme::detail("PaperOS only reads advertising data on this screen.", 34, 722);
  UiTheme::detail("No pairing, connection or writes are performed.", 34, 757);

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


void UiManager::showKeyboard(InputTarget target, const String& prompt, const String& initial, bool masked) {
  inputTarget_ = target;
  inputPrompt_ = prompt;
  inputValue_ = initial;
  inputMasked_ = masked;
  keyboardShift_ = false;
  keyboardSymbols_ = false;
  page_ = Page::Keyboard;
  preparePage();
  drawKeyboard();
  commitPage();
}

void UiManager::drawKeyboard() {
  statusBar();
  UiTheme::title("Keyboard", 18, 76);
  UiTheme::detail(inputPrompt_, 20, 116);

  UiTheme::card(18, 145, 504, 95, true);
  String shown = inputValue_;
  if (inputMasked_) {
    shown = "";
    int n = inputValue_.length() < 28 ? inputValue_.length() : 28;
    for (int i = 0; i < n; ++i) shown += '*';
  } else if (shown.length() > 44) {
    shown = "..." + shown.substring(shown.length() - 41);
  }
  UiTheme::value(shown.length() ? shown : String("_"), 34, 178, false);
  UiTheme::detail(String(inputValue_.length()) + " chars", 420, 213);

  const int x0 = 15, keyW = 47, gap = 4, keyH = 58;
  const int ys[4] = {260, 330, 400, 470};
  String rows[4];
  rows[0] = "1234567890";
  if (!keyboardSymbols_) {
    rows[1] = "qwertyuiop";
    rows[2] = "asdfghjkl-";
    rows[3] = "zxcvbnm./_";
  } else {
    rows[1] = "!@#$%^&*()";
    rows[2] = "[]{}<>+=?;";
    rows[3] = "_-./:;,'\"?";
  }

  for (int row = 0; row < 4; ++row) {
    for (int col = 0; col < 10; ++col) {
      int x = x0 + col * (keyW + gap);
      UiTheme::card(x, ys[row], keyW, keyH, false);
      String key = String(rows[row][col]);
      if (!keyboardSymbols_ && keyboardShift_ && row > 0 && key[0] >= 'a' && key[0] <= 'z') {
        key[0] = static_cast<char>(key[0] - 'a' + 'A');
      }
      M5.Display.setFont(&fonts::FreeSansBold9pt7b);
      M5.Display.setTextDatum(middle_center);
      M5.Display.drawString(key, x + keyW / 2, ys[row] + keyH / 2);
      UiTheme::resetFont();
    }
  }

  const int ay = 550, ah = 70;
  UiTheme::card(18, ay, 90, ah, keyboardShift_);
  UiTheme::value("SHIFT", 34, ay + 22, false);
  UiTheme::card(116, ay, 88, ah, keyboardSymbols_);
  UiTheme::value("SYM", 137, ay + 22, false);
  UiTheme::card(212, ay, 124, ah);
  UiTheme::value("SPACE", 235, ay + 22, false);
  UiTheme::card(344, ay, 80, ah);
  UiTheme::value("BS", 366, ay + 22, false);
  UiTheme::card(432, ay, 90, ah, true);
  UiTheme::value("OK", 455, ay + 22, false);

  UiTheme::card(18, 642, 504, 72);
  UiTheme::value("CANCEL", 210, 664, false);

  UiTheme::card(18, 730, 504, 112);
  UiTheme::label("INPUT", 34, 747);
  UiTheme::detail("SHIFT changes letter case. SYM opens common password/URL symbols.", 34, 782);
  UiTheme::detail("Hardware Home/App buttons can also leave this screen.", 34, 816);
}

void UiManager::handleKeyboardTap(int x, int y) {
  const int x0 = 15, keyW = 47, gap = 4, keyH = 58;
  const int ys[4] = {260, 330, 400, 470};
  String rows[4];
  rows[0] = "1234567890";
  if (!keyboardSymbols_) {
    rows[1] = "qwertyuiop";
    rows[2] = "asdfghjkl-";
    rows[3] = "zxcvbnm./_";
  } else {
    rows[1] = "!@#$%^&*()";
    rows[2] = "[]{}<>+=?;";
    rows[3] = "_-./:;,'\"?";
  }

  bool changed = false;
  for (int row = 0; row < 4 && !changed; ++row) {
    if (y < ys[row] || y >= ys[row] + keyH) continue;
    int col = (x - x0) / (keyW + gap);
    int lx = (x - x0) % (keyW + gap);
    if (col >= 0 && col < 10 && lx >= 0 && lx < keyW) {
      char c = rows[row][col];
      if (!keyboardSymbols_ && keyboardShift_ && row > 0 && c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
      if (inputValue_.length() < 96) inputValue_ += c;
      changed = true;
    }
  }

  if (y >= 550 && y < 620) {
    if (x >= 18 && x < 108) {
      keyboardShift_ = !keyboardShift_;
      UiTheme::beginFrame();
      drawKeyboard();
      display_.pageRefresh();
      return;
    }
    if (x >= 116 && x < 204) {
      keyboardSymbols_ = !keyboardSymbols_;
      UiTheme::beginFrame();
      drawKeyboard();
      display_.pageRefresh();
      return;
    }
    if (x >= 212 && x < 336) {
      if (inputValue_.length() < 96) inputValue_ += ' ';
      changed = true;
    } else if (x >= 344 && x < 424) {
      if (inputValue_.length()) inputValue_.remove(inputValue_.length() - 1);
      changed = true;
    } else if (x >= 432 && x < 522) {
      finishKeyboard();
      return;
    }
  }

  if (y >= 642 && y < 714) {
    InputTarget previous = inputTarget_;
    inputTarget_ = InputTarget::None;
    if (previous == InputTarget::WiFiPassword) showWiFi();
    else if (previous == InputTarget::BrowserUrl) showBrowser();
    else if (previous == InputTarget::OtpSecret) showOtp();
    else showApps();
    return;
  }

  if (changed) {
    M5.Display.fillRect(18, 145, 504, 95, TFT_WHITE);
    UiTheme::card(18, 145, 504, 95, true);
    String shown = inputValue_;
    if (inputMasked_) {
      shown = "";
      int n = min<int>(inputValue_.length(), 28);
      for (int i = 0; i < n; ++i) shown += '*';
    } else if (shown.length() > 44) {
      shown = "..." + shown.substring(shown.length() - 41);
    }
    UiTheme::value(shown.length() ? shown : String("_"), 34, 178, false);
    UiTheme::detail(String(inputValue_.length()) + " chars", 420, 213);
    display_.partialRefresh(18, 145, 504, 95);
  }
}

void UiManager::finishKeyboard() {
  InputTarget target = inputTarget_;
  inputTarget_ = InputTarget::None;

  if (target == InputTarget::WiFiPassword) {
    String ssid = selectedWifiSsid_;
    String password = inputValue_;
    showAppLoading("Wi-Fi", "Connecting to " + ssid, 65);
    bool ok = wifi_.connect(ssid, password, true);
    wifiStatusMessage_ = ok ? String("Connected to ") + ssid : String("Connection failed: ") + ssid;
    showWiFi();
    return;
  }

  if (target == InputTarget::BrowserUrl) {
    browserUrl_ = inputValue_;
    fetchBrowserUrl(browserUrl_);
    return;
  }

  if (target == InputTarget::OtpSecret) {
    String normalized = OtpService::normalizeSecret(inputValue_);
    if (OtpService::validSecret(normalized)) {
      otpSecret_ = normalized;
      otpCode_ = "";
    } else {
      otpSecret_ = "";
      otpCode_ = "INVALID";
    }
    showOtp();
    return;
  }

  showApps();
}

void UiManager::fetchBrowserUrl(const String& url) {
  browserUrl_ = url;
  if (!wifi_.isConnected()) {
    browserPage_ = BrowserPage();
    browserPage_.error = "Connect Wi-Fi first";
    showBrowser();
    return;
  }

  showAppLoading("Web Reader", "Fetching page...", 55);
  browserPage_ = browserService_.fetch(browserUrl_);
  if (browserPage_.finalUrl.length()) browserUrl_ = browserPage_.finalUrl;
  showBrowser();
}

void UiManager::showBrowser() {
  page_ = Page::Browser;
  preparePage();
  statusBar();

  UiTheme::title("Web Reader", 18, 76);
  UiTheme::detail("HTTP / HTTPS reader mode", 20, 116);

  UiTheme::card(18, 145, 504, 108, true);
  UiTheme::label("ADDRESS", 34, 161);
  String u = browserUrl_.length() ? browserUrl_ : String("https://");
  if (u.length() > 52) u = "..." + u.substring(u.length() - 49);
  UiTheme::value(u, 34, 197, false);
  UiTheme::pill("EDIT", 438, 160, true);
  UiTheme::detail(wifi_.isConnected() ? String("Tap to enter URL") : String("Wi-Fi required"), 34, 232);

  if (!browserPage_.ok) {
    UiTheme::card(18, 274, 504, 420);
    UiTheme::value(browserPage_.error.length() ? browserPage_.error : String("Enter a URL"), 40, 326, false);
    UiTheme::detail("Reader mode extracts page title, readable text and absolute links.", 40, 380);
    UiTheme::detail("JavaScript, video, downloads and complex CSS are not executed.", 40, 416);
    UiTheme::detail("HTTPS works in lightweight mode; certificate validation is not available.", 40, 452);
  } else {
    UiTheme::card(18, 274, 504, 88, true);
    String title = browserPage_.title;
    if (title.length() > 54) title = title.substring(0, 51) + "...";
    UiTheme::value(title, 34, 299, false);
    UiTheme::detail(String("HTTP ") + browserPage_.status, 34, 335);

    UiTheme::card(18, 376, 504, 330);
    M5.Display.setFont(&fonts::FreeSans9pt7b);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
    M5.Display.setTextWrap(true, true);
    M5.Display.setCursor(34, 400);
    String excerpt = browserPage_.text.substring(0, 1800);
    M5.Display.print(excerpt);
    M5.Display.setTextWrap(false);
    UiTheme::resetFont();

    UiTheme::label("LINKS", 20, 720);
    for (size_t i = 0; i < browserPage_.links.size() && i < 2; ++i) {
      int y = 744 + static_cast<int>(i) * 54;
      UiTheme::card(18, y, 504, 48);
      String link = browserPage_.links[i];
      if (link.length() > 58) link = link.substring(0, 55) + "...";
      UiTheme::detail(String(i + 1) + ". " + link, 30, y + 14);
    }
  }

  bottomNav(4);
  commitPage();
}

void UiManager::drawOtpCode() {
  M5.Display.fillRect(18, 145, 504, 250, TFT_WHITE);
  UiTheme::card(18, 145, 504, 250, true);
  UiTheme::label("TOTP", 34, 163);

  time_t now = time(nullptr);
  String value;
  String detail;

  if (!otpSecret_.length()) {
    value = otpCode_ == "INVALID" ? "INVALID" : "SET SECRET";
    detail = "Base32 secret is kept only in RAM.";
  } else if (now < 1700000000) {
    value = "SYNC TIME";
    detail = "Connect Wi-Fi once after boot to synchronize NTP.";
  } else {
    value = OtpService::generate(otpSecret_, static_cast<uint64_t>(now), 6, 30);
    uint32_t remaining = 30U - static_cast<uint32_t>(now % 30);
    detail = String("RFC6238 / SHA1 / 6 digits / valid ~") + remaining + " sec";
    otpCode_ = value;
    lastOtpStep_ = static_cast<uint64_t>(now) / 30ULL;
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(value, 270, 255);
  UiTheme::resetFont();

  UiTheme::detail(detail, 64, 332);
}

void UiManager::showOtp() {
  page_ = Page::Otp;
  preparePage();
  statusBar();

  UiTheme::title("OTP Authenticator", 18, 76);
  UiTheme::detail("TOTP calculator / RFC6238", 20, 116);

  drawOtpCode();

  UiTheme::card(18, 414, 246, 112, true);
  UiTheme::value("SET SECRET", 58, 452, false);
  UiTheme::detail("Base32 key", 82, 491);

  UiTheme::card(276, 414, 246, 112);
  UiTheme::value("CLEAR", 347, 452, false);
  UiTheme::detail("Forget from RAM", 330, 491);

  UiTheme::card(18, 546, 504, 166);
  UiTheme::label("SECURITY", 34, 564);
  UiTheme::detail("The OTP secret is not written to LittleFS or microSD.", 34, 604);
  UiTheme::detail("It disappears on reboot/deep power loss.", 34, 638);
  UiTheme::detail("Time is synchronized from NTP after a Wi-Fi connection.", 34, 672);

  UiTheme::card(18, 728, 504, 114);
  UiTheme::label("STATUS", 34, 746);
  UiTheme::detail(wifi_.timeSynced() ? "System clock synchronized" : "NTP not synchronized in this boot", 34, 783);
  UiTheme::detail(otpSecret_.length() ? "Secret loaded in volatile memory" : "No secret loaded", 34, 816);

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

  // Auto-display is disabled: microSD is read while the previous physical
  // frame remains visible, then this complete page is committed once.
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

  bottomNav(1);
  commitPage();
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

  // Build the entire directory page off-screen and commit it once.
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

  bottomNav(2);
  commitPage();
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
  UiTheme::detail("Useful native utilities", 20, 119);

  settingRow(150, "Wi-Fi Analyzer", "Networks / RSSI / channel / connect");
  settingRow(242, "BLE Inspector", "Nearby devices / UUID / manufacturer data");
  settingRow(334, "Web Reader", "HTTP / HTTPS reader mode");
  settingRow(426, "OTP Authenticator", "RFC6238 TOTP / 6 digits / 30 sec");
  settingRow(518, "Clean Display", "Quality white anti-ghost wipe");
  settingRow(610, "System Monitor", "Memory / battery / network");

  UiTheme::card(18, 714, 504, 128);
  UiTheme::label("PAGE TRANSITIONS", 34, 730);
  UiTheme::detail("Every full page change now performs a clean white refresh.", 34, 765);
  UiTheme::detail("The complete next page is then committed in one buffered pass.", 34, 798);
  UiTheme::detail("Dynamic widgets still use fast regional updates.", 34, 828);

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
  if (index == 0) showNotes();
  else if (index == 1) showFiles();
  else if (index == 2) showCalculator();
  else if (index == 3) showWiFi();
  else if (index == 4) showBluetooth();
  else if (index == 5) showBrowser();
  else if (index == 6) showBattery();
  else if (index == 7) showClock();
  else if (index == 8) showFocus();
  else if (index == 9) showOtp();
  else if (index == 10) showNetworkTools();
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

    if (page_ != Page::Keyboard && e.y < UiTheme::StatusH && e.x >= 430) {
      showBattery();
      return;
    }

    if (page_ != Page::Keyboard && e.y >= UiTheme::NavY) {
      handleBottomNav(e.x);
      return;
    }

    if (page_ == Page::Keyboard) {
      handleKeyboardTap(e.x, e.y);
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
      } else if (e.y >= 720 && e.y < 850) {
        showSettings();
      }
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
      if (e.y >= 150 && e.y < 238) showWiFi();
      else if (e.y >= 242 && e.y < 330) showBluetooth();
      else if (e.y >= 334 && e.y < 422) showBrowser();
      else if (e.y >= 426 && e.y < 514) showOtp();
      else if (e.y >= 518 && e.y < 606) showTools();
      else if (e.y >= 610 && e.y < 698) showSystem();
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
      if (e.y >= 145 && e.y < 263) {
        showWiFi();
        return;
      }
      if (e.y >= 310 && e.y < 776) {
        int idx = (e.y - 310) / 88;
        if (idx >= 0 && idx < static_cast<int>(wifiScan_.size())) {
          const WiFiScanEntry entry = wifiScan_[idx];
          if (!entry.ssid.length()) return;
          selectedWifiSsid_ = entry.ssid;
          if (entry.encrypted) {
            showKeyboard(InputTarget::WiFiPassword, String("Password for ") + entry.ssid, "", true);
          } else {
            showAppLoading("Wi-Fi", String("Connecting to ") + entry.ssid, 65);
            bool ok = wifi_.connect(entry.ssid, "", true);
            wifiStatusMessage_ = ok ? String("Connected to ") + entry.ssid : String("Connection failed: ") + entry.ssid;
            showWiFi();
          }
        }
      }
      return;
    }

    if (page_ == Page::Bluetooth) {
      if (e.y >= 145 && e.y < 257) {
        bleScan_.clear();

        M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
        UiTheme::card(18, 306, 504, 470);
        UiTheme::value("Scanning BLE...", 44, 350, false);
        UiTheme::detail("Passive advertising scan in progress.", 44, 392);
        M5.Display.drawRoundRect(44, 442, 438, 18, 9, TFT_BLACK);
        M5.Display.fillRoundRect(47, 445, 292, 12, 6, TFT_BLACK);
        display_.partialRefresh(18, 306, 504, 470);

        BLEScan* scanner = BLEDevice::getScan();
        scanner->setActiveScan(true);
        BLEScanResults results = scanner->start(2, false);
        int shown = min(results.getCount(), 5);

        M5.Display.fillRect(18, 306, 504, 470, TFT_WHITE);
        UiTheme::card(18, 306, 504, 470);

        if (shown == 0) {
          UiTheme::value("No BLE devices found", 44, 350, false);
          UiTheme::detail("Tap SCAN to try again.", 44, 392);
        } else {
          for (int i = 0; i < shown; ++i) {
            BLEAdvertisedDevice d = results.getDevice(i);
            BleScanEntry entry;
            entry.address = String(d.getAddress().toString().c_str());
            entry.name = d.haveName() ? String(d.getName().c_str()) : entry.address;
            entry.rssi = d.getRSSI();
            entry.hasTxPower = d.haveTXPower();
            entry.txPower = entry.hasTxPower ? d.getTXPower() : 0;
            entry.serviceUuid = d.haveServiceUUID() ? String(d.getServiceUUID().toString().c_str()) : String();
            entry.manufacturerHex = d.haveManufacturerData() ? bleManufacturerHex(String(d.getManufacturerData().c_str())) : String();
            bleScan_.push_back(entry);

            String title = entry.name;
            if (title.length() > 24) title = title.substring(0, 24);
            String detail = String(entry.rssi) + " dBm";
            if (entry.serviceUuid.length()) detail += "  /  UUID";
            settingsRow(307 + i * 88, "BT", title, detail, true);
          }
        }

        scanner->clearResults();
        display_.partialRefresh(18, 306, 504, 470);
        return;
      }

      if (e.y >= 306 && e.y < 776) {
        int idx = (e.y - 306) / 88;
        if (idx >= 0 && idx < static_cast<int>(bleScan_.size())) showBleDetail(static_cast<size_t>(idx));
      }
      return;
    }

    if (page_ == Page::Browser) {
      if (e.y >= 145 && e.y < 253) {
        showKeyboard(InputTarget::BrowserUrl, "Web address", browserUrl_.length() ? browserUrl_ : String("https://"), false);
        return;
      }
      if (browserPage_.ok && e.y >= 744 && e.y < 852) {
        int idx = (e.y - 744) / 54;
        int ly = (e.y - 744) % 54;
        if (idx >= 0 && idx < static_cast<int>(browserPage_.links.size()) && idx < 2 && ly < 48) {
          fetchBrowserUrl(browserPage_.links[idx]);
        }
      }
      return;
    }

    if (page_ == Page::Otp) {
      if (e.y >= 414 && e.y < 526) {
        if (e.x < 270) {
          showKeyboard(InputTarget::OtpSecret, "Base32 OTP secret", otpSecret_, false);
        } else {
          otpSecret_ = "";
          otpCode_ = "";
          lastOtpStep_ = 0;
          drawOtpCode();
          display_.partialRefresh(18, 145, 504, 250);
        }
      }
      return;
    }

    if (page_ == Page::Focus) {
      if (e.y >= 515 && e.y < 627) {
        if (e.x < 270) {
          if (focusRunning_) {
            int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
            focusRemainingSec_ = diff > 0 ? static_cast<uint32_t>(diff) / 1000UL : 0;
            focusRunning_ = false;
          } else {
            if (focusRemainingSec_ == 0) focusRemainingSec_ = 25UL * 60UL;
            focusEndMs_ = millis() + focusRemainingSec_ * 1000UL;
            focusRunning_ = true;
          }
        } else {
          focusRunning_ = false;
          focusRemainingSec_ = 25UL * 60UL;
        }

        drawFocusLiveArea();
        M5.Display.fillRect(18, 515, 504, 112, TFT_WHITE);
        UiTheme::card(18, 515, 246, 112, true);
        UiTheme::value(focusRunning_ ? "PAUSE" : "START", 79, 553, false);
        UiTheme::detail("Tap", 121, 592);
        UiTheme::card(276, 515, 246, 112);
        UiTheme::value("RESET", 345, 553, false);
        UiTheme::detail("25:00", 379, 592);
        display_.partialRefresh(18, 145, 504, 482);
      }
      return;
    }

    if (page_ == Page::Fun) {
      if (e.y >= 400 && e.y < 518) {
        if (e.x < 178) funResult_ = String("D6: ") + String((esp_random() % 6U) + 1U);
        else if (e.x < 350) funResult_ = (esp_random() & 1U) ? "HEADS" : "TAILS";
        else funResult_ = String("Number: ") + String((esp_random() % 100U) + 1U);
        drawFunResult();
        display_.partialRefresh(18, 145, 504, 230);
      }
      return;
    }

    if (page_ == Page::Labs) {
      if (e.y >= 252 && e.y < 330) showHidLab();
      else if (e.y >= 342 && e.y < 420) showBluetooth();
      else if (e.y >= 432 && e.y < 510) showComingSoon("GPIO Lab", 4);
      else if (e.y >= 522 && e.y < 600) showComingSoon("Serial Lab", 4);
      else if (e.y >= 612 && e.y < 690) showLabs();
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

  if (focusRunning_) {
    int32_t diff = static_cast<int32_t>(focusEndMs_ - millis());
    if (diff <= 0) {
      focusRunning_ = false;
      focusRemainingSec_ = 0;
      if (page_ == Page::Focus) {
        drawFocusLiveArea();
        display_.partialRefresh(18, 145, 504, 350);
      }
    }
  }

  if (page_ == Page::WiFi) {
    int n = WiFi.scanComplete();
    if (n >= 0) {
      wifiScan_.clear();
      int shown = min(n, 5);

      M5.Display.fillRect(18, 310, 504, 466, TFT_WHITE);
      UiTheme::card(18, 310, 504, 466);

      if (shown == 0) {
        UiTheme::value("No networks found", 44, 354, false);
        UiTheme::detail("Tap CURRENT NETWORK to rescan.", 44, 396);
      } else {
        for (int i = 0; i < shown; ++i) {
          WiFiScanEntry entry;
          entry.ssid = WiFi.SSID(i);
          entry.rssi = WiFi.RSSI(i);
          entry.channel = WiFi.channel(i);
          entry.encrypted = WiFi.encryptionType(i) != WIFI_AUTH_OPEN;
          wifiScan_.push_back(entry);

          String title = entry.ssid.length() ? entry.ssid : String("<hidden>");
          if (title.length() > 24) title = title.substring(0, 24);
          String detail = String(entry.rssi) + " dBm  /  CH " + String(entry.channel) + "  /  " +
                          (entry.encrypted ? "SECURE" : "OPEN");
          settingsRow(311 + i * 88, "WF", title, detail, true);
        }
      }

      display_.partialRefresh(18, 310, 504, 466);
      WiFi.scanDelete();
    }
  }

  if (page_ == Page::Battery && millis() - lastBatteryUiRefresh_ > 30000UL) {
    lastBatteryUiRefresh_ = millis();
    M5.Display.fillRect(18, 145, 504, 325, TFT_WHITE);
    drawBatteryLiveArea();
    display_.partialRefresh(18, 145, 504, 325);
  }

  if (page_ == Page::Clock && millis() - lastClockPageRefresh_ > 60000UL) {
    lastClockPageRefresh_ = millis();
    drawClockLiveArea();
    display_.partialRefresh(18, 145, 504, 410);
  }

  if (page_ == Page::Focus && focusRunning_ && millis() - lastFocusUiRefresh_ > 60000UL) {
    lastFocusUiRefresh_ = millis();
    drawFocusLiveArea();
    display_.partialRefresh(18, 145, 504, 350);
  }

  if (page_ == Page::Otp && otpSecret_.length()) {
    time_t now = time(nullptr);
    if (now >= 1700000000) {
      uint64_t step = static_cast<uint64_t>(now) / 30ULL;
      if (step != lastOtpStep_) {
        drawOtpCode();
        display_.partialRefresh(18, 145, 504, 250);
      }
    }
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

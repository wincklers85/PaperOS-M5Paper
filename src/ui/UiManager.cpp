#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>

namespace paperos {

void UiManager::preparePage(bool forceClean) {
  if (forceClean || display_.pageChangesSinceClean() >= 4) {
    display_.cleanRefresh();
    lastDeepClean_ = millis();
  }
  UiTheme::beginFrame();
}

void UiManager::commitPage() {
  display_.pageRefresh();
}

void UiManager::begin() {
  showHome(true);
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

  UiTheme::title("PaperOS", 18, 78);
  UiTheme::detail(String(VERSION) + "  /  540x960 native UI", 20, 119);

  UiTheme::card(18, 146, 504, 108, true);
  UiTheme::label("NETWORK", 34, 160);
  UiTheme::pill(wifi_.isConnected() ? "ONLINE" : "SETUP AP", 392, 156, wifi_.isConnected());
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 194, false);
  UiTheme::detail(wifi_.ip().toString(), 34, 228);

  UiTheme::card(18, 266, 504, 106);
  UiTheme::label("SOLAR", 34, 280);
  UiTheme::pill("COMING SOON", 382, 276, false);
  UiTheme::value("Energy dashboard", 34, 314, false);
  UiTheme::detail("PV / home / battery / grid / MPPT", 34, 346);
  UiTheme::chevron(491, 307);

  homeCard(18, 384, 246, 116, "TERMO", "Coming Soon", "Puffer / boiler / room", true);
  homeCard(276, 384, 246, 116, "NOTES", storage_.available() ? "Open notes" : "Needs SD", "Read notes on-device", true);

  homeCard(18, 512, 246, 116, "FILES", storage_.available() ? "Browse files" : "Needs SD", "/PaperOS", true);
  homeCard(276, 512, 246, 116, "SYSTEM", String(ESP.getFreeHeap() / 1024) + " KB free", "Status & diagnostics", true);

  UiTheme::card(18, 640, 504, 112);
  UiTheme::label("BROWSER CONSOLE", 34, 654);
  UiTheme::value("paperos.local", 34, 688, false);
  UiTheme::detail("Files / Notes / Wi-Fi / Settings / OTA", 34, 724);
  UiTheme::pill("WEB", 438, 650, true);

  UiTheme::card(18, 764, 504, 86);
  UiTheme::label("POWER", 34, 779);
  UiTheme::detail(String("Auto sleep: ") + config_.get().sleepMinutes + " min  /  Touch wake", 34, 814);
  UiTheme::chevron(491, 791);

  bottomNav(0);
  commitPage();
}

void UiManager::showApps() {
  page_ = Page::Apps;
  preparePage();
  statusBar();

  UiTheme::title("Apps", 18, 78);
  UiTheme::detail("PaperOS native launcher", 20, 119);

  const char* names[15] = {
    "Home", "Notes", "Files",
    "Calculator", "Wi-Fi", "Bluetooth",
    "Termo", "Solar", "Tasks",
    "Calendar", "MQTT", "Settings",
    "System", "Labs", "Reader"
  };
  const char* glyphs[15] = {
    "HM", "NT", "FL",
    "CAL", "WF", "BT",
    "TH", "SL", "TK",
    "CL", "MQ", "ST",
    "SYS", "LAB", "RD"
  };
  const bool ready[15] = {
    true, true, true,
    true, true, true,
    false, false, false,
    false, false, true,
    true, true, false
  };

  const int tileW = 160;
  const int tileH = 116;
  const int startY = 146;
  for (int i = 0; i < 15; ++i) {
    int col = i % 3;
    int row = i / 3;
    int x = 18 + col * 172;
    int y = startY + row * 128;
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], !ready[i]);
  }

  UiTheme::card(18, 796, 504, 54);
  UiTheme::detail("Labs = funzioni beta WinLabs Solutions", 34, 813);

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
  M5.Display.fillRoundRect(18, y, 504, 78, 14, TFT_WHITE);
  M5.Display.drawRoundRect(18, y, 504, 78, 14, TFT_BLACK);
  M5.Display.fillRoundRect(32, y + 14, 48, 48, 10, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(glyph, 56, y + 38);
  UiTheme::resetFont();
  UiTheme::value(titleText, 96, y + 13, false);
  if (detailText.length()) UiTheme::detail(detailText, 96, y + 47);
  if (withChevron) UiTheme::chevron(490, y + 29);
}

void UiManager::showSettings() {
  page_ = Page::Settings;
  preparePage();
  statusBar();

  UiTheme::title("Impostazioni", 154, 78);
  UiTheme::detail("PaperOS", 239, 118);

  UiTheme::card(18, 145, 504, 104, true);
  M5.Display.fillCircle(66, 197, 30, TFT_BLACK);
  M5.Display.setTextColor(TFT_WHITE, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString("P", 66, 197);
  UiTheme::resetFont();
  UiTheme::value(config_.get().deviceName, 112, 166, false);
  UiTheme::detail(String("PaperOS ") + VERSION, 112, 204);
  UiTheme::chevron(490, 183);

  settingsRow(267, "GN", "Generali", "Info dispositivo, firmware e memoria");
  settingsRow(357, "WF", "Wi-Fi", wifi_.isConnected() ? WiFi.SSID() : String("Non connesso"));
  settingsRow(447, "BT", "Bluetooth", bluetoothActive_ ? "Attivo" : "Pronto");
  settingsRow(537, "PW", "Batteria e Sleep", String(power_.batteryPercent()) + "%  /  sleep " + config_.get().sleepMinutes + " min");
  settingsRow(627, "DS", "Display", "540x960 / anti-ghosting");
  settingsRow(717, "LB", "Labs", "Funzioni beta WinLabs");

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

  UiTheme::title("Wi-Fi", 18, 78);
  UiTheme::detail(wifi_.isConnected() ? WiFi.SSID() : String("Non connesso"), 20, 119);

  UiTheme::card(18, 150, 504, 112, true);
  UiTheme::label("CONNESSIONE", 34, 166);
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup"), 34, 201, false);
  UiTheme::detail(wifi_.ip().toString(), 34, 235);
  UiTheme::pill(wifi_.isConnected() ? String(WiFi.RSSI()) + " dBm" : "AP", 395, 163, false);

  UiTheme::label("RETI DISPONIBILI", 20, 286);
  int n = WiFi.scanNetworks(false, true);
  int shown = min(n, 6);
  for (int i = 0; i < shown; ++i) {
    int y = 318 + i * 82;
    settingsRow(y, "WF", WiFi.SSID(i), String(WiFi.RSSI(i)) + " dBm", false);
  }
  if (shown == 0) {
    UiTheme::card(18, 318, 504, 120);
    UiTheme::value("Nessuna rete trovata", 34, 350, false);
    UiTheme::detail("Riprova aprendo nuovamente Wi-Fi.", 34, 392);
  }
  WiFi.scanDelete();

  UiTheme::card(18, 816, 504, 42);
  UiTheme::detail("Per inserire password: paperos.local > Wi-Fi", 34, 827);

  bottomNav(4);
  commitPage();
}

void UiManager::showBluetooth() {
  page_ = Page::Bluetooth;
  preparePage();
  statusBar();

  UiTheme::title("Bluetooth", 18, 78);
  UiTheme::detail("BLE scanner", 20, 119);

  if (!bluetoothActive_) {
    BLEDevice::init("PaperOS");
    bluetoothActive_ = true;
  }

  UiTheme::card(18, 150, 504, 94, true);
  UiTheme::label("BLUETOOTH LE", 34, 166);
  UiTheme::value("Scanner attivo", 34, 199, false);
  UiTheme::pill("BETA", 433, 161, true);

  BLEScan* scanner = BLEDevice::getScan();
  scanner->setActiveScan(true);
  BLEScanResults results = scanner->start(3, false);
  int shown = min(results.getCount(), 6);

  UiTheme::label("DISPOSITIVI VICINI", 20, 268);
  for (int i = 0; i < shown; ++i) {
    BLEAdvertisedDevice d = results.getDevice(i);
    String name = d.haveName() ? String(d.getName().c_str()) : String(d.getAddress().toString().c_str());
    if (name.length() > 24) name = name.substring(0, 24);
    int y = 300 + i * 82;
    settingsRow(y, "BT", name, String(d.getRSSI()) + " dBm", false);
  }
  if (shown == 0) {
    UiTheme::card(18, 300, 504, 120);
    UiTheme::value("Nessun BLE trovato", 34, 334, false);
    UiTheme::detail("Riapri Bluetooth per eseguire una nuova scansione.", 34, 374);
  }
  scanner->clearResults();

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
  else if (index == 8) showComingSoon("Tasks", 4);
  else if (index == 9) showComingSoon("Calendar", 4);
  else if (index == 10) showComingSoon("MQTT", 4);
  else if (index == 11) showSettings();
  else if (index == 12) showSystem();
  else if (index == 13) showLabs();
  else if (index == 14) showComingSoon("Reader", 4);
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
      if (e.y >= 146 && e.y < 254) showSettings();
      else if (e.y >= 266 && e.y < 372) showSolar();
      else if (e.y >= 384 && e.y < 500) {
        if (e.x < 270) showThermo();
        else showNotes();
      } else if (e.y >= 512 && e.y < 628) {
        if (e.x < 270) showFiles();
        else showSystem();
      } else if (e.y >= 640 && e.y < 752) showSettings();
      else if (e.y >= 764 && e.y < 850) showTools();
      return;
    }

    if (page_ == Page::Apps) {
      const int startY = 146;
      const int pitchY = 128;
      if (e.y >= startY && e.y < startY + 5 * pitchY && e.x >= 18) {
        int row = (e.y - startY) / pitchY;
        int col = (e.x - 18) / 172;
        int localX = (e.x - 18) % 172;
        int localY = (e.y - startY) % pitchY;
        if (col >= 0 && col < 3 && localX < 160 && localY < 116) openAppIndex(row * 3 + col);
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
      if (e.y >= 145 && e.y < 249) showGeneral();
      else if (e.y >= 267 && e.y < 345) showGeneral();
      else if (e.y >= 357 && e.y < 435) showWiFi();
      else if (e.y >= 447 && e.y < 525) showBluetooth();
      else if (e.y >= 537 && e.y < 615) showTools();
      else if (e.y >= 627 && e.y < 705) showTools();
      else if (e.y >= 717 && e.y < 795) showLabs();
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
          showCalculator();
        }
      }
      return;
    }
  }

  if (millis() - lastClock_ > 60000UL) {
    lastClock_ = millis();
    statusBar();
    display_.partialRefresh(0, 0, M5.Display.width(), UiTheme::StatusH);
  }

  // Periodic quality refresh is deliberately rare; normal navigation uses epd_text.
  if (millis() - lastDeepClean_ > 20UL * 60UL * 1000UL && page_ == Page::Home) {
    showHome(true);
  }
}

}

#include "UiManager.h"
#include "PaperOS.h"
#include <WiFi.h>
#include <SD.h>
#include <ArduinoJson.h>

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
  UiTheme::bluetoothIcon(382, iconY - 1, false);
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

  const char* names[12] = {
    "Home", "Notes", "Files",
    "Termo", "Solar", "Wi-Fi",
    "Tasks", "Calendar", "Bluetooth",
    "MQTT", "Settings", "System"
  };
  const char* glyphs[12] = {
    "HM", "NT", "FL",
    "TH", "SL", "WF",
    "TK", "CL", "BT",
    "MQ", "ST", "SYS"
  };
  const bool ready[12] = {
    true, true, true,
    false, false, true,
    false, false, false,
    false, true, true
  };

  const int tileW = 160;
  const int tileH = 126;
  const int startY = 148;
  for (int i = 0; i < 12; ++i) {
    int col = i % 3;
    int row = i / 3;
    int x = 18 + col * 172;
    int y = startY + row * 138;
    UiTheme::appTile(x, y, tileW, tileH, glyphs[i], names[i], !ready[i]);
  }

  UiTheme::card(18, 712, 504, 128);
  UiTheme::label("STATUS", 34, 730);
  UiTheme::detail("Native now: Home / Notes / Files / Wi-Fi / Settings / System", 34, 765);
  UiTheme::detail("Termo and Solar have final UI shells but no fake telemetry.", 34, 799);

  bottomNav(4);
  commitPage();
}

void UiManager::settingRow(int y, const String& titleText, const String& detailText) {
  UiTheme::card(18, y, 504, 88);
  UiTheme::value(titleText, 34, y + 17, false);
  UiTheme::detail(detailText, 34, y + 56);
  UiTheme::chevron(491, y + 34);
}

void UiManager::showSettings() {
  page_ = Page::Settings;
  preparePage();
  statusBar();

  UiTheme::title("Settings", 18, 78);
  UiTheme::detail("PaperOS preferences", 20, 119);

  settingRow(150, "Device", config_.get().deviceName + " / " + config_.get().language);
  settingRow(246, "Wi-Fi", wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup"));
  settingRow(342, "Display", "540x960 / high contrast / anti-ghost");
  settingRow(438, "Power", String("Auto sleep ") + config_.get().sleepMinutes + " min");
  settingRow(534, "Storage", storage_.available() ? "microSD mounted" : "microSD unavailable");
  settingRow(630, "Browser", String("http://") + HOSTNAME + ".local");

  UiTheme::card(18, 738, 504, 104);
  UiTheme::label("SLEEP & WAKE", 34, 754);
  UiTheme::detail(config_.get().touchWakeEnabled ? "Touch wake enabled" : "Touch wake disabled", 34, 788);
  UiTheme::detail("Timed wake is optional and separate from auto sleep.", 34, 818);

  bottomNav(4);
  commitPage();
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
  else if (index == 3) showThermo();
  else if (index == 4) showSolar();
  else if (index == 5 || index == 10) showSettings();
  else if (index == 11) showSystem();
  else if (index == 6) showComingSoon("Tasks", 4);
  else if (index == 7) showComingSoon("Calendar", 4);
  else if (index == 8) showComingSoon("Bluetooth", 4);
  else if (index == 9) showComingSoon("MQTT", 4);
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
      const int startY = 148;
      const int pitchY = 138;
      if (e.y >= startY && e.y < startY + 4 * pitchY && e.x >= 18) {
        int row = (e.y - startY) / pitchY;
        int col = (e.x - 18) / 172;
        int localX = (e.x - 18) % 172;
        int localY = (e.y - startY) % pitchY;
        if (col >= 0 && col < 3 && localX < 160 && localY < 126) openAppIndex(row * 3 + col);
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

    if (page_ == Page::Settings && e.y >= 650 && e.y < 738) {
      showSystem();
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

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
  UiTheme::detail(String(VERSION) + "  /  " + config_.get().deviceName, 20, 119);

  UiTheme::card(18, 148, 504, 124, true);
  UiTheme::label("NETWORK", 34, 164);
  UiTheme::pill(wifi_.isConnected() ? "ONLINE" : "SETUP AP", 392, 160, wifi_.isConnected());
  UiTheme::value(wifi_.isConnected() ? WiFi.SSID() : String(SETUP_AP), 34, 202, false);
  UiTheme::detail(wifi_.ip().toString(), 34, 244);

  homeCard(18, 286, 246, 126, "BATTERY", String(power_.batteryPercent()) + "%", String(power_.batteryMillivolts()) + " mV");
  String storageValue = storage_.available() ? String(storage_.freeBytes() / 1048576.0, 0) + " MB free" : "No SD";
  homeCard(276, 286, 246, 126, "STORAGE", storageValue, storage_.available() ? "microSD ready" : "Insert microSD");

  homeCard(18, 426, 246, 126, "NOTES", storage_.available() ? "Open notes" : "Needs SD", "Read notes on-device");
  homeCard(276, 426, 246, 126, "FILES", storage_.available() ? "Browse files" : "Needs SD", "/PaperOS");

  UiTheme::card(18, 566, 504, 112);
  UiTheme::label("SYSTEM", 34, 582);
  UiTheme::value(String(ESP.getFreeHeap() / 1024) + " KB free heap", 34, 618, false);
  UiTheme::detail("Tap for memory, PSRAM, Wi-Fi and power", 34, 654);
  UiTheme::chevron(492, 611);

  UiTheme::card(18, 692, 504, 150);
  UiTheme::label("QUICK ACCESS", 34, 708);
  UiTheme::value("paperos.local", 34, 744, false);
  UiTheme::detail("Browser console + OTA + full file management", 34, 780);
  UiTheme::pill("TOOLS", 416, 704, true);
  UiTheme::detail("Touch Tools below for display cleanup and power.", 34, 813);

  bottomNav(0);
  commitPage();
}

void UiManager::showApps() {
  page_ = Page::Apps;
  preparePage();
  statusBar();

  UiTheme::title("Apps", 18, 78);
  UiTheme::detail("Native PaperOS tools", 20, 119);

  const char* names[12] = {
    "Home", "Notes", "Tasks",
    "Calendar", "Files", "PDF",
    "Wi-Fi", "Bluetooth", "MQTT",
    "GPIO", "Settings", "System"
  };
  const char* glyphs[12] = {
    "HM", "NT", "TK",
    "CL", "FL", "PDF",
    "WF", "BT", "MQ",
    "IO", "ST", "SYS"
  };
  const bool ready[12] = {
    true, true, false,
    false, true, false,
    true, false, false,
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
  UiTheme::label("AVAILABLE NOW", 34, 730);
  UiTheme::detail("Home / Notes / Files / Wi-Fi / Settings / System", 34, 765);
  UiTheme::detail("Tools includes anti-ghosting refresh and sleep.", 34, 799);

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
  UiTheme::detail("Device essentials", 20, 119);

  settingRow(150, "Device", config_.get().deviceName + " / " + config_.get().language);
  settingRow(250, "Wi-Fi", wifi_.isConnected() ? WiFi.SSID() : String("PaperOS-Setup"));
  settingRow(350, "Power", String("Sleep after ") + config_.get().sleepMinutes + " min");
  settingRow(450, "Storage", storage_.available() ? "microSD mounted" : "microSD unavailable");
  settingRow(550, "Browser", String("http://") + HOSTNAME + ".local");
  settingRow(650, "About", String("PaperOS ") + VERSION);

  UiTheme::card(18, 750, 504, 92);
  UiTheme::detail("Advanced editing is available from the browser console.", 34, 775);
  UiTheme::detail("Touch Tools for clean refresh and sleep.", 34, 807);

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
  UiTheme::detail("Device maintenance", 20, 119);

  settingRow(150, "Clean Display", "White wipe + redraw to remove ghosting");
  settingRow(250, "System Monitor", "Heap / PSRAM / battery / network");
  settingRow(350, "Settings", "Wi-Fi / power / browser / storage");
  settingRow(450, "Sleep Now", "Enter low-power deep sleep");
  settingRow(550, "Browser Console", String("http://") + HOSTNAME + ".local");

  UiTheme::card(18, 670, 504, 170);
  UiTheme::label("REFRESH POLICY", 34, 690);
  UiTheme::detail("Normal page change: epd_text (fast + readable)", 34, 728);
  UiTheme::detail("Automatic cleanup: every 4 page changes", 34, 762);
  UiTheme::detail("Clean Display: immediate anti-ghosting wipe", 34, 796);

  bottomNav(3);
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
  const char* names[12] = {
    "Home", "Notes", "Tasks", "Calendar", "Files", "PDF Reader",
    "Wi-Fi", "Bluetooth", "MQTT", "GPIO", "Settings", "System"
  };
  if (index == 0) showHome();
  else if (index == 1) showNotes();
  else if (index == 4) showFiles();
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
      if (e.y >= 148 && e.y < 272) showSettings();
      else if (e.y >= 286 && e.y < 412) {
        if (e.x < 270) showSystem();
        else showFiles();
      } else if (e.y >= 426 && e.y < 552) {
        if (e.x < 270) showNotes();
        else showFiles();
      } else if (e.y >= 566 && e.y < 678) showSystem();
      else if (e.y >= 692 && e.y < 842) showTools();
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
      } else if (e.y >= 250 && e.y < 338) {
        showSystem();
      } else if (e.y >= 350 && e.y < 438) {
        showSettings();
      } else if (e.y >= 450 && e.y < 538) {
        power_.sleepNow();
      } else if (e.y >= 550 && e.y < 638) {
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

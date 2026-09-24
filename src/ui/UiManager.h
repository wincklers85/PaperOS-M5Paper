#pragma once
#include <Arduino.h>
#include <vector>
#include "UiTheme.h"
#include "../drivers/DisplayManager.h"
#include "../drivers/TouchManager.h"
#include "../network/WiFiManager.h"
#include "../storage/StorageManager.h"
#include "../services/PowerManager.h"
#include "../core/ConfigManager.h"

namespace paperos {

class UiManager {
 public:
  UiManager(DisplayManager& d, TouchManager& t, WiFiManager& w, StorageManager& s, PowerManager& p, ConfigManager& c)
   : display_(d), touch_(t), wifi_(w), storage_(s), power_(p), config_(c) {}

  void begin();
  void loop();

  void showHome(bool forceClean = false);
  void showApps();
  void showSystem();
  void showSettings();
  void showNotes();
  void showFiles(const String& path = "/PaperOS");
  void showTools();
  void showThermo();
  void showSolar();
  void showWiFi();
  void showBluetooth();
  void showGeneral();
  void showLabs();
  void showHidLab();
  void showCalculator();
  void showBattery();
  void showClock();
  void showFocus();
  void showFun();

 private:
  enum class Page {
    Home, Apps, System, Settings, Notes, NoteView,
    Files, FileView, Tools, WiFi, Bluetooth, General, Labs, HidLab, Calculator, Battery, Clock, Focus, Fun, ComingSoon
  };

  struct FileEntry {
    String name;
    bool directory = false;
    uint64_t size = 0;
  };

  Page page_ = Page::Home;
  DisplayManager& display_;
  TouchManager& touch_;
  WiFiManager& wifi_;
  StorageManager& storage_;
  PowerManager& power_;
  ConfigManager& config_;

  uint32_t lastClock_ = 0;
  uint32_t lastDeepClean_ = 0;
  uint32_t lastBatteryUiRefresh_ = 0;
  uint32_t lastClockPageRefresh_ = 0;
  uint32_t lastFocusUiRefresh_ = 0;
  int comingNav_ = 4;
  String comingTitle_;

  std::vector<FileEntry> fileEntries_;
  String currentFilePath_ = "/PaperOS";
  bool bluetoothActive_ = false;
  String calcDisplay_ = "0";
  double calcAccumulator_ = 0.0;
  char calcPendingOp_ = 0;
  bool calcResetInput_ = true;
  std::vector<String> noteIds_;
  std::vector<String> noteTitles_;
  bool focusRunning_ = false;
  uint32_t focusEndMs_ = 0;
  uint32_t focusRemainingSec_ = 25UL * 60UL;
  String funResult_ = "Tap a game";

  void preparePage(bool forceClean = false);
  void commitPage();
  void statusBar();
  void bottomNav(int active);
  void homeCard(int x, int y, int w, int h, const String& title, const String& value, const String& detail, bool chevron = true);
  void settingRow(int y, const String& title, const String& detail);
  void showComingSoon(const String& title, int activeNav = 4);
  void showNote(size_t index);
  void showFilePreview(const String& fullPath, const String& name, uint64_t size);
  void handleBottomNav(int x);
  void openAppIndex(int index);
  String parentPath(const String& path) const;
  String joinPath(const String& base, const String& name) const;
  bool isTextFile(const String& name) const;
  void settingsRow(int y, const String& glyph, const String& title, const String& detail, bool chevron = true);
  void calcKey(const String& key);
  void showAppLoading(const String& title, const String& detail, int percent = 55);
  void drawBatteryLiveArea();
  void drawClockLiveArea();
  void drawFocusLiveArea();
  void drawFunResult();
};

}

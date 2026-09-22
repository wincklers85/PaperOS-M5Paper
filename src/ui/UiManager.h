#pragma once
#include <Arduino.h>
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
  void showHome(bool full = true);
  void showApps();
  void showSystem();
  void showSettings();
 private:
  enum class Page { Home, Apps, System, Settings } page_ = Page::Home;
  DisplayManager& display_; TouchManager& touch_; WiFiManager& wifi_; StorageManager& storage_; PowerManager& power_; ConfigManager& config_;
  uint32_t lastClock_ = 0; uint32_t lastFull_ = 0;
  void topBar();
  void drawButton(int x,int y,int w,int h,const String& label,bool comingSoon=false);
};
}

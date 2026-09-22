#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "../core/ConfigManager.h"

namespace paperos {
class PowerManager {
 public:
  explicit PowerManager(ConfigManager& cfg) : cfg_(cfg) {}
  int batteryPercent() const;
  int batteryMillivolts() const;
  void markActivity();
  void loop();
  void sleepNow(uint32_t seconds = 0);
 private:
  ConfigManager& cfg_;
  uint32_t lastActivity_ = 0;
};
}

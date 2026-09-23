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

  // Deep sleep indefinitely (touch/button wake) unless wakeSeconds > 0.
  void sleepNow(uint32_t wakeSeconds = 0);

  // Convenience timed sleep with touch wake still enabled.
  void sleepForMinutes(uint32_t minutes);

  uint32_t inactivityMinutes() const;
  uint32_t secondsUntilAutoSleep() const;

 private:
  ConfigManager& cfg_;
  uint32_t lastActivity_ = 0;

  void drawSleepScreen(uint32_t wakeSeconds);
};
}

#pragma once
#include <Arduino.h>
#include <M5Unified.h>
#include "../core/ConfigManager.h"

namespace paperos {

enum class BatteryTrend : uint8_t {
  Unknown = 0,
  Rising,
  Stable,
  Falling
};

class PowerManager {
 public:
  explicit PowerManager(ConfigManager& cfg) : cfg_(cfg) {}

  int batteryPercent() const;
  int batteryMillivolts() const;

  // Original M5Paper exposes battery voltage/level but has no current-sense
  // hardware for charge/discharge current.
  bool batteryCurrentSupported() const { return false; }
  int batteryCurrentMilliamps() const { return 0; }
  uint16_t nominalBatteryCapacityMah() const { return 1150; }

  BatteryTrend batteryTrend() const;
  String batteryTrendLabel() const;
  bool chargingLikely() const;
  int batteryTrendDeltaMv() const { return trendDeltaMv_; }
  String wakeReason() const;

  void markActivity();
  void loop();

  // Deep sleep indefinitely (touch wake) unless wakeSeconds > 0.
  void sleepNow(uint32_t wakeSeconds = 0);

  // Convenience timed sleep with touch wake still enabled.
  void sleepForMinutes(uint32_t minutes);

  uint32_t inactivityMinutes() const;
  uint32_t secondsUntilAutoSleep() const;

 private:
  ConfigManager& cfg_;
  uint32_t lastActivity_ = 0;

  static constexpr uint8_t kBatterySamples = 5;
  int batterySamples_[kBatterySamples] = {0};
  uint8_t batterySampleCount_ = 0;
  uint8_t batterySampleIndex_ = 0;
  uint32_t lastBatterySampleMs_ = 0;
  int trendDeltaMv_ = 0;

  void sampleBattery();
  void drawSleepScreen(uint32_t wakeSeconds);
};

}

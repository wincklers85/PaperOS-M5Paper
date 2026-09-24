#include "PowerManager.h"
#include "PaperOS.h"
#include <esp_sleep.h>

namespace paperos {

int PowerManager::batteryPercent() const {
  return M5.Power.getBatteryLevel();
}

int PowerManager::batteryMillivolts() const {
  return M5.Power.getBatteryVoltage();
}

void PowerManager::markActivity() {
  lastActivity_ = millis();
  autoLockIssued_ = false;
}

uint32_t PowerManager::inactivityMinutes() const {
  if (!lastActivity_) return 0;
  return (millis() - lastActivity_) / 60000UL;
}

uint32_t PowerManager::secondsUntilAutoSleep() const {
  const uint32_t mins = cfg_.get().sleepMinutes;
  if (!mins) return 0;
  const uint32_t timeoutMs = mins * 60000UL;
  const uint32_t idleMs = lastActivity_ ? millis() - lastActivity_ : 0;
  if (idleMs >= timeoutMs) return 0;
  return (timeoutMs - idleMs) / 1000UL;
}

void PowerManager::sampleBattery() {
  const uint32_t now = millis();
  if (lastBatterySampleMs_ && now - lastBatterySampleMs_ < 15000UL) return;
  lastBatterySampleMs_ = now;

  const int mv = batteryMillivolts();
  if (mv <= 0) return;

  batterySamples_[batterySampleIndex_] = mv;
  batterySampleIndex_ = (batterySampleIndex_ + 1) % kBatterySamples;
  if (batterySampleCount_ < kBatterySamples) ++batterySampleCount_;

  voltageHistory_[voltageHistoryIndex_] = mv;
  voltageHistoryIndex_ = (voltageHistoryIndex_ + 1) % kVoltageHistorySamples;
  if (voltageHistoryCount_ < kVoltageHistorySamples) ++voltageHistoryCount_;

  if (batterySampleCount_ >= 3) {
    const uint8_t newestIndex = (batterySampleIndex_ + kBatterySamples - 1) % kBatterySamples;
    const uint8_t oldestIndex = batterySampleCount_ < kBatterySamples ? 0 : batterySampleIndex_;
    trendDeltaMv_ = batterySamples_[newestIndex] - batterySamples_[oldestIndex];
  }
}

BatteryTrend PowerManager::batteryTrend() const {
  if (batterySampleCount_ < 3) return BatteryTrend::Unknown;
  if (trendDeltaMv_ >= 10) return BatteryTrend::Rising;
  if (trendDeltaMv_ <= -10) return BatteryTrend::Falling;
  return BatteryTrend::Stable;
}

String PowerManager::batteryTrendLabel() const {
  switch (batteryTrend()) {
    case BatteryTrend::Rising: return "In aumento";
    case BatteryTrend::Stable: return "Stabile";
    case BatteryTrend::Falling: return "In diminuzione";
    default: return "In analisi";
  }
}

bool PowerManager::chargingLikely() const {
  // This is deliberately only a heuristic: original M5Paper cannot expose
  // real USB/charger state or battery current. A sustained voltage rise can
  // suggest charging but is not presented as a hardware measurement.
  return batteryPercent() < 100 && batteryTrend() == BatteryTrend::Rising;
}

int PowerManager::voltageHistoryMv(uint8_t chronologicalIndex) const {
  if (chronologicalIndex >= voltageHistoryCount_) return 0;
  uint8_t oldest = voltageHistoryCount_ < kVoltageHistorySamples ? 0 : voltageHistoryIndex_;
  uint8_t idx = (oldest + chronologicalIndex) % kVoltageHistorySamples;
  return voltageHistory_[idx];
}

String PowerManager::wakeReason() const {
  switch (esp_sleep_get_wakeup_cause()) {
    case ESP_SLEEP_WAKEUP_EXT0: return "Touch / wake pin";
    case ESP_SLEEP_WAKEUP_EXT1: return "External wake pin";
    case ESP_SLEEP_WAKEUP_TIMER: return "Timer";
    case ESP_SLEEP_WAKEUP_TOUCHPAD: return "Touch";
    case ESP_SLEEP_WAKEUP_ULP: return "ULP";
    case ESP_SLEEP_WAKEUP_UNDEFINED: return "Power on / reset";
    default: return "Other";
  }
}

void PowerManager::loop() {
  if (!lastActivity_) lastActivity_ = millis();
  sampleBattery();

  const uint32_t mins = cfg_.get().sleepMinutes;
  if (!mins) return;

  if (!autoLockIssued_ && millis() - lastActivity_ >= mins * 60000UL) {
    lockRequested_ = true;
    autoLockIssued_ = true;
  }
}

void PowerManager::drawSleepScreen(uint32_t wakeSeconds) {
  auto dt = M5.Rtc.getDateTime();

  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.setColorDepth(1);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(middle_center);

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.drawString("PaperOS", M5.Display.width() / 2, 132);

  char clockText[8];
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  } else {
    snprintf(clockText, sizeof(clockText), "--:--");
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.drawString(clockText, M5.Display.width() / 2, 252);

  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.drawString("Deep Sleep", M5.Display.width() / 2, 334);

  M5.Display.drawRoundRect(74, 404, 392, 166, 18, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.drawString(String("Battery  ") + batteryPercent() + "%", M5.Display.width() / 2, 450);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(String(batteryMillivolts()) + " mV", M5.Display.width() / 2, 493);

  if (cfg_.get().touchWakeEnabled || wakeSeconds == 0) {
    M5.Display.drawString("Touch the screen to wake PaperOS", M5.Display.width() / 2, 535);
  } else {
    M5.Display.drawString("Wake by timer", M5.Display.width() / 2, 535);
  }

  if (wakeSeconds) {
    M5.Display.drawString(String("Timer: ") + (wakeSeconds / 60UL) + " min", M5.Display.width() / 2, 632);
  } else {
    M5.Display.drawString("No wake timer", M5.Display.width() / 2, 632);
  }

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString("The e-paper image remains visible without refresh", M5.Display.width() / 2, 760);
  M5.Display.display();
  delay(120);
}

void PowerManager::sleepNow(uint32_t) {
  lockRequested_ = true;
  autoLockIssued_ = true;
}

bool PowerManager::consumeLockRequest() {
  if (!lockRequested_) return false;
  lockRequested_ = false;
  return true;
}

void PowerManager::deepSleepNow(uint32_t wakeSeconds) {
  drawSleepScreen(wakeSeconds);

  bool touchWake = cfg_.get().touchWakeEnabled;
  if (wakeSeconds == 0 && !touchWake) touchWake = true;

  const uint64_t wakeUs = wakeSeconds
    ? static_cast<uint64_t>(wakeSeconds) * 1000000ULL
    : ~0ULL;

  M5.update();
  delay(40);
  M5.Power.deepSleep(wakeUs, touchWake);
}

void PowerManager::sleepForMinutes(uint32_t minutes) {
  if (minutes) deepSleepNow(minutes * 60UL);
  else sleepNow(0);
}

}

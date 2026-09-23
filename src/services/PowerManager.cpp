#include "PowerManager.h"
#include "PaperOS.h"

namespace paperos {

int PowerManager::batteryPercent() const {
  return M5.Power.getBatteryLevel();
}

int PowerManager::batteryMillivolts() const {
  return M5.Power.getBatteryVoltage();
}

void PowerManager::markActivity() {
  lastActivity_ = millis();
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

void PowerManager::loop() {
  if (!lastActivity_) lastActivity_ = millis();

  const uint32_t mins = cfg_.get().sleepMinutes;
  if (!mins) return;

  // Auto sleep must remain asleep until the user wakes the device.
  // Older PaperOS builds incorrectly scheduled another wake after the same timeout.
  if (millis() - lastActivity_ >= mins * 60000UL) {
    sleepNow(0);
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
  M5.Display.drawString("PaperOS", M5.Display.width() / 2, 150);

  char clockText[8];
  if (dt.date.year >= 2020 && dt.date.year <= 2099) {
    snprintf(clockText, sizeof(clockText), "%02d:%02d", dt.time.hours, dt.time.minutes);
  } else {
    snprintf(clockText, sizeof(clockText), "--:--");
  }

  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.drawString(clockText, M5.Display.width() / 2, 275);

  M5.Display.setFont(&fonts::FreeSans12pt7b);
  M5.Display.drawString("Sleeping", M5.Display.width() / 2, 355);

  M5.Display.drawRoundRect(92, 430, 356, 132, 18, TFT_BLACK);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.drawString(String("Battery ") + batteryPercent() + "%", M5.Display.width() / 2, 470);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  if (cfg_.get().touchWakeEnabled) {
    M5.Display.drawString("Touch the screen to wake", M5.Display.width() / 2, 520);
  } else {
    M5.Display.drawString("Use the hardware button to wake", M5.Display.width() / 2, 520);
  }

  if (wakeSeconds) {
    M5.Display.drawString(String("Automatic wake in ") + (wakeSeconds / 60UL) + " min", M5.Display.width() / 2, 610);
  } else {
    M5.Display.drawString("No automatic wake timer", M5.Display.width() / 2, 610);
  }

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString("E-paper keeps this screen without power", M5.Display.width() / 2, 760);
  M5.Display.display();
  delay(150);
}

void PowerManager::sleepNow(uint32_t wakeSeconds) {
  drawSleepScreen(wakeSeconds);

  const bool touchWake = cfg_.get().touchWakeEnabled;
  const uint64_t wakeUs = static_cast<uint64_t>(wakeSeconds) * 1000000ULL;

  // M5Unified deepSleep supports optional timer + touch wake.
  M5.Power.deepSleep(wakeUs, touchWake);
}

void PowerManager::sleepForMinutes(uint32_t minutes) {
  sleepNow(minutes ? minutes * 60UL : 0);
}

}

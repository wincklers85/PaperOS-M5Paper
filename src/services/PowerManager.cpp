#include "PowerManager.h"
#include "PaperOS.h"

namespace paperos {
int PowerManager::batteryPercent() const { return M5.Power.getBatteryLevel(); }
int PowerManager::batteryMillivolts() const { return M5.Power.getBatteryVoltage(); }
void PowerManager::markActivity() { lastActivity_ = millis(); }
void PowerManager::loop() {
  if (!lastActivity_) lastActivity_ = millis();
  auto mins = cfg_.get().sleepMinutes;
  if (mins && millis() - lastActivity_ > mins * 60000UL) sleepNow(mins * 60);
}
void PowerManager::sleepNow(uint32_t seconds) {
  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK,TFT_WHITE);
  M5.Display.setCursor(28,40);
  M5.Display.setTextSize(2);
  M5.Display.println("PaperOS sleeping");
  M5.Display.setTextSize(1);
  M5.Display.printf("Battery: %d%%\n", batteryPercent());
  M5.Display.println("Touch/power button to wake");
  if (seconds) M5.Display.printf("Timer wake: %lu s\n", static_cast<unsigned long>(seconds));
  M5.Display.display();
  delay(100);
  if (seconds) M5.Power.deepSleep(static_cast<uint64_t>(seconds) * 1000000ULL, true);
  else M5.Power.deepSleep();
}
}

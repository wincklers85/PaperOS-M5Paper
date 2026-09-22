#include "DisplayManager.h"
#include "PaperOS.h"

namespace paperos {
void DisplayManager::begin() {
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5Paper;
  cfg.clear_display = true;
  M5.begin(cfg);
  M5.Display.setRotation(1);
  M5.Display.setEpdMode(m5gfx::epd_fast);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextWrap(false);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.display();
}

void DisplayManager::splash() {
  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(3);
  M5.Display.drawString("PaperOS", width()/2, height()/2 - 34);
  M5.Display.setTextSize(1);
  M5.Display.drawString(String("v") + VERSION, width()/2, height()/2 + 18);
  M5.Display.drawString("M5Paper ESP32", width()/2, height()/2 + 48);
  M5.Display.display();
  delay(900);
  M5.Display.setTextDatum(top_left);
  M5.Display.setEpdMode(m5gfx::epd_fast);
}

void DisplayManager::fullRefresh() {
  M5.Display.setEpdMode(m5gfx::epd_quality);
  M5.Display.display();
  M5.Display.setEpdMode(m5gfx::epd_fast);
}

void DisplayManager::partialRefresh(int32_t x, int32_t y, int32_t w, int32_t h) {
  M5.Display.setEpdMode(m5gfx::epd_fastest);
  M5.Display.display(x, y, w, h);
  M5.Display.setEpdMode(m5gfx::epd_fast);
}
}

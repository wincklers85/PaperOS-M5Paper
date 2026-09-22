#include "DisplayManager.h"
#include "PaperOS.h"

namespace paperos {

void DisplayManager::begin() {
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5Paper;
  cfg.clear_display = true;
  M5.begin(cfg);

  // PaperOS is designed pixel-first for the original M5Paper in portrait mode.
  if (M5.Display.width() > M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }

  M5.Display.setEpdMode(m5gfx::epd_fast);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextWrap(false);
  M5.Display.setTextDatum(top_left);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.display();
}

void DisplayManager::splash() {
  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.fillScreen(TFT_WHITE);

  const int cx = width() / 2;
  const int cy = height() / 2;

  M5.Display.drawRoundRect(cx - 48, cy - 148, 96, 96, 22, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextSize(2.2f);
  M5.Display.drawString("P", cx, cy - 100);

  M5.Display.setTextSize(3);
  M5.Display.drawString("PaperOS", cx, cy - 10);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_DARKGREY, TFT_WHITE);
  M5.Display.drawString("Smart e-paper workspace", cx, cy + 42);
  M5.Display.drawString(String("M5Paper  /  ") + VERSION, cx, cy + 72);

  const int barX = 76;
  const int barY = cy + 130;
  const int barW = width() - 152;
  M5.Display.drawRoundRect(barX, barY, barW, 14, 7, TFT_BLACK);
  M5.Display.fillRoundRect(barX + 3, barY + 3, barW - 6, 8, 4, TFT_BLACK);

  M5.Display.display();
  delay(800);

  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
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

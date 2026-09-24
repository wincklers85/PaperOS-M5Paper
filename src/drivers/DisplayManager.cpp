#include "DisplayManager.h"
#include "PaperOS.h"

namespace paperos {

void DisplayManager::begin() {
  auto cfg = M5.config();
  cfg.fallback_board = m5::board_t::board_M5Paper;
  cfg.clear_display = true;
  M5.begin(cfg);

  // Critical for e-paper UX: M5GFX enables auto-display on framebuffer
  // panels by default. Disable it so an entire PaperOS screen is composed
  // off-screen first and becomes visible only on the explicit display()
  // call in commitPage()/partialRefresh().
  M5.Display.setAutoDisplay(false);

  if (M5.Display.width() > M5.Display.height()) {
    M5.Display.setRotation(M5.Display.getRotation() ^ 1);
  }

  // PaperOS 0.1.2-alpha is intentionally high-contrast.
  // 1-bit drawing avoids faint antialiased grays on the original M5Paper.
  M5.Display.setColorDepth(1);
  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextWrap(false);
  M5.Display.setTextDatum(top_left);
  M5.Display.fillScreen(TFT_WHITE);
  // Do not force an extra panel update here. splash() performs the first
  // visible refresh and avoids a redundant boot-time flash.
}

void DisplayManager::splash() {
  M5.Display.setEpdMode(m5gfx::epd_text);
  M5.Display.fillScreen(TFT_WHITE);

  const int cx = width() / 2;
  const int cy = height() / 2;

  M5.Display.drawRoundRect(cx - 50, cy - 150, 100, 100, 18, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeSansBold24pt7b);
  M5.Display.drawString("P", cx, cy - 98);

  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.drawString("PaperOS", cx, cy - 6);

  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString("Smart e-paper workspace", cx, cy + 42);
  M5.Display.drawString(String("M5Paper  /  ") + VERSION, cx, cy + 72);

  const int barX = 74;
  const int barY = cy + 128;
  const int barW = width() - 148;
  M5.Display.drawRoundRect(barX, barY, barW, 14, 7, TFT_BLACK);
  M5.Display.fillRoundRect(barX + 3, barY + 3, barW - 6, 8, 4, TFT_BLACK);

  M5.Display.display();
  delay(120);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(top_left);
}

void DisplayManager::pageRefresh() {
  // Fast waveform for normal page-to-page navigation. Ghosting is handled
  // by the less frequent quality clean in UiManager and the manual tool.
  M5.Display.setEpdMode(m5gfx::epd_fastest);
  M5.Display.display();
  M5.Display.setEpdMode(m5gfx::epd_text);
  ++pageChangesSinceClean_;
}

void DisplayManager::qualityRefresh() {
  M5.Display.setEpdMode(m5gfx::epd_quality);
  M5.Display.display();
  M5.Display.setEpdMode(m5gfx::epd_text);
  pageChangesSinceClean_ = 0;
}

void DisplayManager::cleanRefresh() {
  // A quality white pass is intentionally slower, but it is used only
  // periodically/on demand to reset visible e-paper ghosting.
  M5.Display.setEpdMode(m5gfx::epd_quality);
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.display();
  delay(100);
  M5.Display.setEpdMode(m5gfx::epd_text);
  pageChangesSinceClean_ = 0;
}

void DisplayManager::partialRefresh(int32_t x, int32_t y, int32_t w, int32_t h) {
  M5.Display.setEpdMode(m5gfx::epd_fastest);
  M5.Display.display(x, y, w, h);
  M5.Display.setEpdMode(m5gfx::epd_text);
}

void DisplayManager::notePageChange() {
  ++pageChangesSinceClean_;
}

}

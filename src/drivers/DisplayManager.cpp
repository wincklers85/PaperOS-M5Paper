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
  M5.Display.drawRoundRect(barX, barY, barW, 24, 8, TFT_BLACK);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString("Avvio in corso", barX, barY + 44);

  M5.Display.display();
  delay(120);
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextDatum(top_left);
  splashProgress(0, "Preparazione display");
}

void DisplayManager::splashProgress(uint8_t percent, const String& phase) {
  percent = percent > 100 ? 100 : percent;
  const int cx = width() / 2;
  const int cy = height() / 2;
  const int barX = 74;
  const int barY = cy + 128;
  const int barW = width() - 148;
  M5.Display.fillRect(barX + 3, barY + 3, barW - 6, 18, TFT_WHITE);
  const int fillW = (barW - 6) * percent / 100;
  // One-bit display: a checker pattern creates a stable mid-gray impression.
  for (int y = 0; y < 18; ++y) {
    int x = (y & 1) ? 1 : 0;
    while (x < fillW) {
      M5.Display.drawPixel(barX + 3 + x, barY + 3 + y, TFT_BLACK);
      x += 2;
    }
  }
  M5.Display.fillRect(74, barY + 38, width() - 148, 30, TFT_WHITE);
  M5.Display.setTextDatum(middle_left);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(phase, 74, barY + 51);
  M5.Display.setTextDatum(middle_right);
  M5.Display.drawString(String(percent) + "%", width() - 74, barY + 51);
  M5.Display.display(barX - 4, barY - 2, barW + 8, 74);
  M5.Display.setTextDatum(top_left);
  M5.Display.setFont(&fonts::Font2);
}

void DisplayManager::pageRefresh() {
  if (profile_ == 0) M5.Display.setEpdMode(m5gfx::epd_fastest);
  else if (profile_ == 1) M5.Display.setEpdMode(m5gfx::epd_fast);
  else M5.Display.setEpdMode(m5gfx::epd_quality);
  M5.Display.display();
  M5.Display.setEpdMode(m5gfx::epd_text);
  ++pageChangesSinceClean_;
}

String DisplayManager::profileLabel() const {
  if (profile_ == 0) return "FAST";
  if (profile_ == 2) return "CLEAN";
  return "BALANCED";
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

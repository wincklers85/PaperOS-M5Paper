#include "UiTheme.h"

namespace paperos {

void UiTheme::resetFont() {
  M5.Display.setFont(&fonts::Font2);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
}

void UiTheme::beginFrame() {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextWrap(false);
  resetFont();
}

void UiTheme::card(int x, int y, int w, int h, bool heavy) {
  M5.Display.fillRoundRect(x, y, w, h, 12, TFT_WHITE);
  M5.Display.drawRoundRect(x, y, w, h, 12, TFT_BLACK);
  if (heavy) M5.Display.drawRoundRect(x + 1, y + 1, w - 2, h - 2, 11, TFT_BLACK);
}

void UiTheme::title(const String& text, int x, int y) {
  M5.Display.setFont(&fonts::FreeSansBold18pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(text, x, y);
  resetFont();
}

void UiTheme::label(const String& text, int x, int y) {
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(text, x, y);
  resetFont();
}

void UiTheme::value(const String& text, int x, int y, bool large) {
  M5.Display.setFont(large ? &fonts::FreeSansBold18pt7b : &fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(text, x, y);
  resetFont();
}

void UiTheme::detail(const String& text, int x, int y) {
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(text, x, y);
  resetFont();
}

void UiTheme::pill(const String& text, int x, int y, bool filled) {
  resetFont();
  int w = 24 + (int)text.length() * 9;
  const uint32_t bg = filled ? TFT_BLACK : TFT_WHITE;
  const uint32_t fg = filled ? TFT_WHITE : TFT_BLACK;
  M5.Display.fillRoundRect(x, y, w, 32, 16, bg);
  M5.Display.drawRoundRect(x, y, w, 32, 16, TFT_BLACK);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(text, x + w / 2, y + 16);
  resetFont();
}

void UiTheme::chevron(int x, int y) {
  M5.Display.drawLine(x, y, x + 9, y + 9, TFT_BLACK);
  M5.Display.drawLine(x + 9, y + 9, x, y + 18, TFT_BLACK);
}

void UiTheme::divider(int x, int y, int w) {
  M5.Display.drawFastHLine(x, y, w, TFT_BLACK);
}

static void drawNavGlyph(int index, int cx, int cy, uint32_t color) {
  if (index == 0) {
    M5.Display.drawLine(cx - 11, cy, cx, cy - 10, color);
    M5.Display.drawLine(cx, cy - 10, cx + 11, cy, color);
    M5.Display.drawRect(cx - 8, cy, 16, 12, color);
  } else if (index == 1) {
    M5.Display.drawRoundRect(cx - 10, cy - 11, 20, 24, 2, color);
    M5.Display.drawFastHLine(cx - 6, cy - 4, 12, color);
    M5.Display.drawFastHLine(cx - 6, cy + 2, 12, color);
    M5.Display.drawFastHLine(cx - 6, cy + 8, 9, color);
  } else if (index == 2) {
    M5.Display.drawRect(cx - 12, cy - 5, 24, 16, color);
    M5.Display.drawRect(cx - 10, cy - 10, 11, 5, color);
  } else if (index == 3) {
    M5.Display.drawCircle(cx - 5, cy - 5, 5, color);
    M5.Display.drawLine(cx - 1, cy - 1, cx + 10, cy + 10, color);
    M5.Display.drawCircle(cx + 10, cy + 10, 2, color);
  } else {
    for (int r = 0; r < 2; ++r)
      for (int c = 0; c < 2; ++c)
        M5.Display.fillRect(cx - 10 + c * 12, cy - 10 + r * 12, 8, 8, color);
  }
}

void UiTheme::navItem(int index, const String& labelText, bool active) {
  const int itemW = ScreenW / 5;
  const int x = index * itemW;
  const uint32_t bg = active ? TFT_BLACK : TFT_WHITE;
  const uint32_t fg = active ? TFT_WHITE : TFT_BLACK;

  if (active) {
    M5.Display.fillRoundRect(x + 7, NavY + 7, itemW - 14, NavH - 14, 12, bg);
  }

  const int cx = x + itemW / 2;
  drawNavGlyph(index, cx, NavY + 31, fg);
  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(fg, bg);
  M5.Display.drawString(labelText, cx, NavY + 66);
  resetFont();
}

void UiTheme::appTile(int x, int y, int w, int h, const String& glyph, const String& labelText, bool comingSoon) {
  card(x, y, w, h, !comingSoon);
  M5.Display.drawRoundRect(x + 14, y + 14, 48, 48, 9, TFT_BLACK);

  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextDatum(middle_center);
  M5.Display.drawString(glyph, x + 38, y + 38);
  resetFont();

  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.drawString(labelText, x + 14, y + 72);
  resetFont();

  if (comingSoon) detail("Soon", x + 14, y + 101);
  else label("OPEN", x + 14, y + 101);
}

void UiTheme::batteryIcon(int x, int y, int percent) {
  M5.Display.drawRect(x, y, 26, 13, TFT_BLACK);
  M5.Display.fillRect(x + 26, y + 3, 3, 7, TFT_BLACK);
  int fill = constrain(map(percent, 0, 100, 0, 22), 0, 22);
  if (fill > 0) M5.Display.fillRect(x + 2, y + 2, fill, 9, TFT_BLACK);
}

void UiTheme::wifiIcon(int x, int y, bool connected) {
  if (!connected) {
    M5.Display.drawLine(x, y, x + 18, y + 18, TFT_BLACK);
    M5.Display.drawLine(x + 18, y, x, y + 18, TFT_BLACK);
    return;
  }
  M5.Display.drawFastHLine(x + 1, y + 2, 17, TFT_BLACK);
  M5.Display.drawFastHLine(x + 4, y + 8, 11, TFT_BLACK);
  M5.Display.fillCircle(x + 9, y + 15, 2, TFT_BLACK);
}

void UiTheme::bluetoothIcon(int x, int y, bool enabled) {
  uint32_t c = TFT_BLACK;
  M5.Display.drawLine(x + 7, y, x + 7, y + 18, c);
  M5.Display.drawLine(x + 7, y, x + 14, y + 5, c);
  M5.Display.drawLine(x + 14, y + 5, x + 3, y + 13, c);
  M5.Display.drawLine(x + 3, y + 5, x + 14, y + 13, c);
  M5.Display.drawLine(x + 14, y + 13, x + 7, y + 18, c);
  if (!enabled) M5.Display.drawLine(x, y, x + 17, y + 18, c);
}

void UiTheme::sdIcon(int x, int y, bool mounted) {
  M5.Display.drawRect(x, y + 2, 16, 18, TFT_BLACK);
  M5.Display.drawLine(x + 10, y + 2, x + 16, y + 8, TFT_BLACK);
  if (!mounted) M5.Display.drawLine(x - 2, y, x + 18, y + 22, TFT_BLACK);
}

}

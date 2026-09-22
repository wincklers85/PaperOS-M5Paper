#include "UiTheme.h"

namespace paperos {

void UiTheme::beginFrame() {
  M5.Display.fillScreen(TFT_WHITE);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextWrap(false);
  M5.Display.setTextSize(1);
}

void UiTheme::card(int x, int y, int w, int h) {
  M5.Display.fillRoundRect(x, y, w, h, 14, TFT_WHITE);
  M5.Display.drawRoundRect(x, y, w, h, 14, TFT_BLACK);
}

void UiTheme::sectionTitle(const String& title, int x, int y) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(title, x, y);
}

void UiTheme::label(const String& text, int x, int y) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(text, x, y);
}

void UiTheme::value(const String& text, int x, int y, float size) {
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(size);
  M5.Display.drawString(text, x, y);
  M5.Display.setTextSize(1);
}

void UiTheme::muted(const String& text, int x, int y) {
  M5.Display.setTextColor(TFT_DARKGREY, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextSize(1);
  M5.Display.drawString(text, x, y);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

void UiTheme::pill(const String& text, int x, int y, bool filled) {
  int w = 20 + (int)text.length() * 7;
  const uint32_t bg = filled ? TFT_BLACK : TFT_WHITE;
  const uint32_t fg = filled ? TFT_WHITE : TFT_BLACK;
  M5.Display.fillRoundRect(x, y, w, 28, 14, bg);
  M5.Display.drawRoundRect(x, y, w, 28, 14, TFT_BLACK);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(text, x + w / 2, y + 14);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.setTextDatum(top_left);
}

void UiTheme::chevron(int x, int y) {
  M5.Display.drawLine(x, y, x + 7, y + 7, TFT_BLACK);
  M5.Display.drawLine(x + 7, y + 7, x, y + 14, TFT_BLACK);
}

void UiTheme::divider(int x, int y, int w) {
  M5.Display.drawFastHLine(x, y, w, TFT_LIGHTGREY);
}

static void drawNavGlyph(int index, int cx, int cy, uint32_t color) {
  if (index == 0) {
    M5.Display.drawLine(cx - 10, cy, cx, cy - 9, color);
    M5.Display.drawLine(cx, cy - 9, cx + 10, cy, color);
    M5.Display.drawRect(cx - 7, cy, 14, 11, color);
  } else if (index == 1) {
    M5.Display.drawRoundRect(cx - 9, cy - 10, 18, 22, 2, color);
    M5.Display.drawFastHLine(cx - 5, cy - 3, 10, color);
    M5.Display.drawFastHLine(cx - 5, cy + 2, 10, color);
    M5.Display.drawFastHLine(cx - 5, cy + 7, 7, color);
  } else if (index == 2) {
    M5.Display.drawRect(cx - 11, cy - 5, 22, 15, color);
    M5.Display.drawRect(cx - 9, cy - 10, 10, 5, color);
  } else if (index == 3) {
    M5.Display.drawCircle(cx - 5, cy - 5, 5, color);
    M5.Display.drawLine(cx - 1, cy - 1, cx + 10, cy + 10, color);
    M5.Display.drawCircle(cx + 10, cy + 10, 2, color);
  } else {
    for (int r = 0; r < 2; ++r)
      for (int c = 0; c < 2; ++c)
        M5.Display.fillRect(cx - 9 + c * 11, cy - 9 + r * 11, 7, 7, color);
  }
}

void UiTheme::navItem(int index, const String& labelText, bool active) {
  const int itemW = ScreenW / 5;
  const int x = index * itemW;
  const uint32_t bg = active ? TFT_BLACK : TFT_WHITE;
  const uint32_t fg = active ? TFT_WHITE : TFT_BLACK;
  if (active) M5.Display.fillRoundRect(x + 8, NavY + 8, itemW - 16, NavH - 16, 12, bg);
  const int cx = x + itemW / 2;
  drawNavGlyph(index, cx, NavY + 30, fg);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextColor(fg, bg);
  M5.Display.setTextSize(1);
  M5.Display.drawString(labelText, cx, NavY + 61);
  M5.Display.setTextDatum(top_left);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

void UiTheme::appTile(int x, int y, int w, int h, const String& glyph, const String& labelText, bool comingSoon) {
  card(x, y, w, h);
  M5.Display.drawRoundRect(x + 14, y + 15, 42, 42, 10, TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(1);
  M5.Display.drawString(glyph, x + 35, y + 36);
  M5.Display.setTextDatum(top_left);
  M5.Display.drawString(labelText, x + 14, y + 70);
  if (comingSoon) {
    M5.Display.setTextColor(TFT_DARKGREY, TFT_WHITE);
    M5.Display.drawString("Soon", x + 14, y + 94);
    M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  } else {
    M5.Display.drawString("Open", x + 14, y + 94);
  }
}

void UiTheme::batteryIcon(int x, int y, int percent) {
  M5.Display.drawRect(x, y, 24, 12, TFT_BLACK);
  M5.Display.fillRect(x + 24, y + 3, 3, 6, TFT_BLACK);
  int fill = constrain(map(percent, 0, 100, 0, 20), 0, 20);
  if (fill > 0) M5.Display.fillRect(x + 2, y + 2, fill, 8, TFT_BLACK);
}

void UiTheme::wifiIcon(int x, int y, bool connected) {
  if (!connected) {
    M5.Display.drawLine(x, y, x + 16, y + 16, TFT_BLACK);
    M5.Display.drawLine(x + 16, y, x, y + 16, TFT_BLACK);
    return;
  }
  M5.Display.drawFastHLine(x + 2, y + 3, 14, TFT_BLACK);
  M5.Display.drawFastHLine(x + 5, y + 8, 8, TFT_BLACK);
  M5.Display.fillCircle(x + 9, y + 14, 2, TFT_BLACK);
}

void UiTheme::bluetoothIcon(int x, int y, bool enabled) {
  uint32_t c = enabled ? TFT_BLACK : TFT_DARKGREY;
  M5.Display.drawLine(x + 7, y, x + 7, y + 18, c);
  M5.Display.drawLine(x + 7, y, x + 14, y + 5, c);
  M5.Display.drawLine(x + 14, y + 5, x + 3, y + 13, c);
  M5.Display.drawLine(x + 3, y + 5, x + 14, y + 13, c);
  M5.Display.drawLine(x + 14, y + 13, x + 7, y + 18, c);
}

void UiTheme::sdIcon(int x, int y, bool mounted) {
  uint32_t c = mounted ? TFT_BLACK : TFT_DARKGREY;
  M5.Display.drawRect(x, y + 2, 15, 17, c);
  M5.Display.drawLine(x + 10, y + 2, x + 15, y + 7, c);
  M5.Display.setTextColor(c, TFT_WHITE);
  M5.Display.setTextSize(1);
  M5.Display.drawString("S", x + 4, y + 6);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
}

}

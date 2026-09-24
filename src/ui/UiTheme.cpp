#include "UiTheme.h"

namespace paperos {

uint8_t UiTheme::style_ = 1;

String UiTheme::styleName() {
  if (style_ == 0) return "CLASSIC";
  if (style_ == 2) return "TECH";
  if (style_ == 3) return "MINIMAL";
  return "SOFT";
}

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
  const int radius = style_ == 0 ? 8 : (style_ == 2 ? 4 : (style_ == 3 ? 14 : 18));
  M5.Display.fillRoundRect(x, y, w, h, radius, TFT_WHITE);
  M5.Display.drawRoundRect(x, y, w, h, radius, TFT_BLACK);
  if (heavy) {
    M5.Display.drawFastHLine(x + 12, y + 3, w - 24, TFT_BLACK);
  }
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
  card(x, y, w, h, false);

  const int ix = x + 14;
  const int iy = y + 13;
  const int cx = ix + 23;
  const int cy = iy + 23;
  const uint32_t bg = comingSoon ? TFT_WHITE : TFT_BLACK;
  const uint32_t fg = comingSoon ? TFT_BLACK : TFT_WHITE;
  const int radius = style_ == 2 ? 7 : 15;

  M5.Display.fillRoundRect(ix, iy, 46, 46, radius, bg);
  M5.Display.drawRoundRect(ix, iy, 46, 46, radius, TFT_BLACK);

  if (glyph == "NT") {
    M5.Display.drawRoundRect(cx-11,cy-13,22,26,3,fg);
    M5.Display.drawFastHLine(cx-6,cy-6,12,fg);
    M5.Display.drawFastHLine(cx-6,cy,12,fg);
    M5.Display.drawFastHLine(cx-6,cy+6,9,fg);
  } else if (glyph == "FL") {
    M5.Display.drawRoundRect(cx-14,cy-7,28,19,3,fg);
    M5.Display.drawRect(cx-12,cy-12,12,6,fg);
  } else if (glyph == "CAL") {
    M5.Display.drawRoundRect(cx-13,cy-13,26,26,4,fg);
    M5.Display.drawFastHLine(cx-8,cy-5,16,fg);
    M5.Display.drawFastVLine(cx-4,cy-1,11,fg);
    M5.Display.drawFastVLine(cx+4,cy-1,11,fg);
  } else if (glyph == "WF") {
    M5.Display.fillCircle(cx,cy+9,2,fg);
    M5.Display.drawLine(cx-6,cy+3,cx,cy,fg); M5.Display.drawLine(cx,cy,cx+6,cy+3,fg);
    M5.Display.drawLine(cx-12,cy-4,cx,cy-9,fg); M5.Display.drawLine(cx,cy-9,cx+12,cy-4,fg);
  } else if (glyph == "BT") {
    M5.Display.drawLine(cx,cy-14,cx,cy+14,fg);
    M5.Display.drawLine(cx,cy-14,cx+9,cy-6,fg); M5.Display.drawLine(cx+9,cy-6,cx-8,cy+7,fg);
    M5.Display.drawLine(cx-8,cy-7,cx+9,cy+6,fg); M5.Display.drawLine(cx+9,cy+6,cx,cy+14,fg);
  } else if (glyph == "WEB") {
    M5.Display.drawCircle(cx,cy,13,fg);
    M5.Display.drawFastHLine(cx-12,cy,24,fg);
    M5.Display.drawFastVLine(cx,cy-12,24,fg);
    M5.Display.drawRoundRect(cx-6,cy-12,12,24,6,fg);
  } else if (glyph == "BAT") {
    M5.Display.drawRoundRect(cx-14,cy-8,25,16,4,fg);
    M5.Display.fillRect(cx+11,cy-4,4,8,fg);
    M5.Display.fillRoundRect(cx-10,cy-4,14,8,2,fg);
  } else if (glyph == "CK" || glyph == "25") {
    M5.Display.drawCircle(cx,cy,13,fg);
    M5.Display.drawLine(cx,cy,cx,cy-8,fg);
    M5.Display.drawLine(cx,cy,cx+7,cy+5,fg);
  } else if (glyph == "OTP") {
    M5.Display.drawRoundRect(cx-14,cy-10,28,20,5,fg);
    for (int k=-7;k<=7;k+=7) M5.Display.fillCircle(cx+k,cy,2,fg);
  } else if (glyph == "NET") {
    M5.Display.fillCircle(cx,cy-9,3,fg);
    M5.Display.fillCircle(cx-11,cy+8,3,fg);
    M5.Display.fillCircle(cx+11,cy+8,3,fg);
    M5.Display.drawLine(cx,cy-6,cx-9,cy+6,fg);
    M5.Display.drawLine(cx,cy-6,cx+9,cy+6,fg);
    M5.Display.drawFastHLine(cx-8,cy+8,16,fg);
  } else if (glyph == "ST") {
    M5.Display.drawCircle(cx,cy,11,fg);
    M5.Display.drawCircle(cx,cy,4,fg);
    M5.Display.drawFastHLine(cx-15,cy,30,fg);
    M5.Display.drawFastVLine(cx,cy-15,30,fg);
  } else if (glyph == "SYS") {
    M5.Display.drawRect(cx-14,cy-11,28,20,fg);
    M5.Display.drawLine(cx-8,cy+14,cx+8,cy+14,fg);
    M5.Display.drawFastVLine(cx,cy+9,5,fg);
  } else if (glyph == "LAB") {
    M5.Display.drawLine(cx-5,cy-14,cx-5,cy-4,fg); M5.Display.drawLine(cx+5,cy-14,cx+5,cy-4,fg);
    M5.Display.drawLine(cx-5,cy-4,cx-12,cy+12,fg); M5.Display.drawLine(cx+5,cy-4,cx+12,cy+12,fg);
    M5.Display.drawFastHLine(cx-12,cy+12,24,fg); M5.Display.drawFastHLine(cx-7,cy+5,14,fg);
  } else if (glyph == "PH") {
    M5.Display.drawRoundRect(cx-9,cy-14,18,28,5,fg);
    M5.Display.drawFastHLine(cx-4,cy+9,8,fg);
  } else {
    M5.Display.setFont(&fonts::FreeSansBold9pt7b);
    M5.Display.setTextDatum(middle_center);
    M5.Display.setTextColor(fg,bg);
    M5.Display.drawString(glyph,cx,cy);
  }
  resetFont();

  M5.Display.setFont(&fonts::FreeSansBold9pt7b);
  M5.Display.setTextColor(TFT_BLACK, TFT_WHITE);
  M5.Display.drawString(labelText, x + 14, y + 66);
  resetFont();
  detail(comingSoon ? "SOON" : "READY", x + 14, y + 91);
}

void UiTheme::batteryIcon(int x, int y, int percent) {
  M5.Display.drawRoundRect(x, y, 28, 15, 4, TFT_BLACK);
  M5.Display.fillRoundRect(x + 28, y + 4, 3, 7, 1, TFT_BLACK);
  int fill = constrain(map(percent, 0, 100, 0, 22), 0, 22);
  if (fill > 0) M5.Display.fillRoundRect(x + 3, y + 3, fill, 9, 2, TFT_BLACK);
}

void UiTheme::wifiIcon(int x, int y, bool connected) {
  const int cx = x + 10;
  if (!connected) {
    M5.Display.drawCircle(cx, y + 9, 9, TFT_BLACK);
    M5.Display.drawLine(x + 3, y + 2, x + 18, y + 17, TFT_BLACK);
    return;
  }
  M5.Display.drawLine(cx, y + 16, cx, y + 17, TFT_BLACK);
  M5.Display.fillCircle(cx, y + 16, 2, TFT_BLACK);
  M5.Display.drawLine(x + 6, y + 12, cx, y + 9, TFT_BLACK);
  M5.Display.drawLine(cx, y + 9, x + 14, y + 12, TFT_BLACK);
  M5.Display.drawLine(x + 3, y + 8, cx, y + 4, TFT_BLACK);
  M5.Display.drawLine(cx, y + 4, x + 17, y + 8, TFT_BLACK);
  M5.Display.drawLine(x, y + 4, cx, y, TFT_BLACK);
  M5.Display.drawLine(cx, y, x + 20, y + 4, TFT_BLACK);
}

void UiTheme::bluetoothIcon(int x, int y, bool enabled) {
  uint32_t c = TFT_BLACK;
  const int cx = x + 8;
  M5.Display.drawLine(cx, y, cx, y + 20, c);
  M5.Display.drawLine(cx, y, x + 15, y + 5, c);
  M5.Display.drawLine(x + 15, y + 5, x + 3, y + 14, c);
  M5.Display.drawLine(x + 3, y + 6, x + 15, y + 15, c);
  M5.Display.drawLine(x + 15, y + 15, cx, y + 20, c);
  if (!enabled) {
    M5.Display.drawLine(x, y + 1, x + 18, y + 20, c);
    M5.Display.drawLine(x + 1, y + 1, x + 19, y + 20, c);
  }
}

void UiTheme::sdIcon(int x, int y, bool mounted) {
  M5.Display.drawRoundRect(x, y + 2, 17, 20, 3, TFT_BLACK);
  M5.Display.drawLine(x + 10, y + 2, x + 17, y + 9, TFT_BLACK);
  M5.Display.drawFastVLine(x + 4, y + 5, 6, TFT_BLACK);
  M5.Display.drawFastVLine(x + 8, y + 5, 6, TFT_BLACK);
  if (!mounted) M5.Display.drawLine(x - 2, y, x + 20, y + 24, TFT_BLACK);
}


void UiTheme::shadowCard(int x,int y,int w,int h,int depth) {
  const int radius = style_ == 2 ? 5 : 18;
  M5.Display.fillRoundRect(x+depth,y+depth,w,h,radius,TFT_BLACK);
  M5.Display.fillRoundRect(x,y,w,h,radius,TFT_WHITE);
  M5.Display.drawRoundRect(x,y,w,h,radius,TFT_BLACK);
}

void UiTheme::ditherOverlay(int x,int y,int w,int h) {
  for (int yy=y; yy<y+h; yy+=4) {
    int offset=((yy/4)&1)?2:0;
    for (int xx=x+offset; xx<x+w; xx+=4) M5.Display.drawPixel(xx,yy,TFT_BLACK);
  }
}

void UiTheme::iconButton(int x,int y,int w,int h,const String& glyph,const String& labelText,bool filled) {
  uint32_t bg=filled?TFT_BLACK:TFT_WHITE;
  uint32_t fg=filled?TFT_WHITE:TFT_BLACK;
  int radius = style_ == 2 ? 6 : 18;
  M5.Display.fillRoundRect(x,y,w,h,radius,bg);
  M5.Display.drawRoundRect(x,y,w,h,radius,TFT_BLACK);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setFont(&fonts::FreeSansBold12pt7b);
  M5.Display.setTextColor(fg,bg);
  M5.Display.drawString(glyph,x+w/2,y+h/2-10);
  M5.Display.setFont(&fonts::FreeSans9pt7b);
  M5.Display.drawString(labelText,x+w/2,y+h-20);
  resetFont();
}

}

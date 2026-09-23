#pragma once
#include <Arduino.h>
#include <M5Unified.h>

namespace paperos {

class UiTheme {
 public:
  static constexpr int ScreenW = 540;
  static constexpr int ScreenH = 960;
  static constexpr int Margin = 18;
  static constexpr int Gap = 12;
  static constexpr int StatusH = 58;
  static constexpr int NavH = 92;
  static constexpr int NavY = ScreenH - NavH;

  static void beginFrame();
  static void card(int x, int y, int w, int h, bool heavy = false);
  static void title(const String& text, int x, int y);
  static void label(const String& text, int x, int y);
  static void value(const String& text, int x, int y, bool large = false);
  static void detail(const String& text, int x, int y);
  static void pill(const String& text, int x, int y, bool filled = false);
  static void chevron(int x, int y);
  static void divider(int x, int y, int w);
  static void navItem(int index, const String& label, bool active);
  static void appTile(int x, int y, int w, int h, const String& glyph, const String& label, bool comingSoon);
  static void batteryIcon(int x, int y, int percent);
  static void wifiIcon(int x, int y, bool connected);
  static void bluetoothIcon(int x, int y, bool enabled);
  static void sdIcon(int x, int y, bool mounted);
  static void resetFont();
};

}

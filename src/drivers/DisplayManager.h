#pragma once
#include <M5Unified.h>
#include <Arduino.h>

namespace paperos {
class DisplayManager {
 public:
  void begin();
  void splash();
  void fullRefresh();
  void partialRefresh(int32_t x, int32_t y, int32_t w, int32_t h);
  int width() const { return M5.Display.width(); }
  int height() const { return M5.Display.height(); }
};
}

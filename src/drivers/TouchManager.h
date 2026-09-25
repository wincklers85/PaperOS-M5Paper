#pragma once
#include <M5Unified.h>

namespace paperos {

enum class TouchGesture : uint8_t {
  None = 0,
  Tap,
  SwipeUp,
  SwipeDown,
  SwipeLeft,
  SwipeRight,
  LongPress
};

struct TouchEvent {
  bool active = false;
  bool clicked = false;
  bool released = false;
  int x = 0;
  int y = 0;
  int startX = 0;
  int startY = 0;
  int dx = 0;
  int dy = 0;
  TouchGesture gesture = TouchGesture::None;
};

class TouchManager {
 public:
  TouchEvent poll();

 private:
  bool tracking_ = false;
  int startX_ = 0;
  int startY_ = 0;
  int lastX_ = 0;
  int lastY_ = 0;
  uint32_t startedMs_ = 0;
};

}

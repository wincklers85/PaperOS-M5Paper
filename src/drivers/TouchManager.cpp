#include "TouchManager.h"
#include <cstdlib>

namespace paperos {

TouchEvent TouchManager::poll() {
  TouchEvent e;

  if (M5.Touch.getCount()) {
    auto d = M5.Touch.getDetail(0);
    e.active = true;
    e.x = d.x;
    e.y = d.y;

    if (!tracking_) {
      tracking_ = true;
      startX_ = lastX_ = d.x;
      startY_ = lastY_ = d.y;
      startedMs_ = millis();
    } else {
      lastX_ = d.x;
      lastY_ = d.y;
    }

    e.startX = startX_;
    e.startY = startY_;
    e.dx = lastX_ - startX_;
    e.dy = lastY_ - startY_;
    return e;
  }

  if (!tracking_) return e;

  tracking_ = false;
  e.released = true;
  e.x = lastX_;
  e.y = lastY_;
  e.startX = startX_;
  e.startY = startY_;
  e.dx = lastX_ - startX_;
  e.dy = lastY_ - startY_;

  const int ax = std::abs(e.dx);
  const int ay = std::abs(e.dy);
  const uint32_t elapsed = millis() - startedMs_;

  if (ax < 28 && ay < 28 && elapsed < 900) {
    e.clicked = true;
    e.gesture = TouchGesture::Tap;
  } else if (ay >= 60 && ay > ax) {
    e.gesture = e.dy > 0 ? TouchGesture::SwipeDown : TouchGesture::SwipeUp;
  } else if (ax >= 60 && ax > ay) {
    e.gesture = e.dx > 0 ? TouchGesture::SwipeRight : TouchGesture::SwipeLeft;
  }

  return e;
}

}

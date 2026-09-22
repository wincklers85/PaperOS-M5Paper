#pragma once
#include <M5Unified.h>

namespace paperos {
struct TouchEvent { bool clicked = false; int x = 0; int y = 0; };
class TouchManager {
 public:
  TouchEvent poll();
};
}

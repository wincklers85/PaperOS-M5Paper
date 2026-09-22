#include "TouchManager.h"
namespace paperos {
TouchEvent TouchManager::poll() {
  TouchEvent e;
  if (!M5.Touch.getCount()) return e;
  auto d = M5.Touch.getDetail(0);
  if (d.wasClicked()) { e.clicked = true; e.x = d.x; e.y = d.y; }
  return e;
}
}

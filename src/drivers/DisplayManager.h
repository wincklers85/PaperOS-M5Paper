#pragma once
#include <M5Unified.h>
#include <Arduino.h>

namespace paperos {
class DisplayManager {
 public:
  void begin();
  void splash();

  // Fast full-screen refresh for normal page changes.
  void pageRefresh();

  // High-quality full-screen refresh used periodically or on demand.
  void qualityRefresh();

  // White-flash cleanup followed by a text refresh. Useful when ghosting is visible.
  void cleanRefresh();

  // Small-region update for clock/status.
  void partialRefresh(int32_t x, int32_t y, int32_t w, int32_t h);
  void setProfile(uint8_t profile) { profile_ = profile > 2 ? 2 : profile; }
  uint8_t profile() const { return profile_; }
  String profileLabel() const;

  void notePageChange();
  uint32_t pageChangesSinceClean() const { return pageChangesSinceClean_; }

  int width() const { return M5.Display.width(); }
  int height() const { return M5.Display.height(); }

 private:
  uint32_t pageChangesSinceClean_ = 0;
  uint8_t profile_ = 1;
};
}

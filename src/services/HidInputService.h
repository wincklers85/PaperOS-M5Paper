#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEClient.h>
#include <BLESecurity.h>
#include <vector>

namespace paperos {

struct HidDeviceInfo {
  String name;
  String address;
  int rssi = 0;
  uint16_t appearance = 0;
  bool likelyKeyboard = false;
  bool likelyMouse = false;
};

struct HidKeyEvent {
  uint8_t keycode = 0;
  char ascii = 0;
  bool shift = false;
  bool ctrl = false;
  bool alt = false;
};

struct HidMouseEvent {
  int8_t dx = 0;
  int8_t dy = 0;
  int8_t wheel = 0;
  uint8_t buttons = 0;
};

class HidInputService {
 public:
  bool begin();
  void stop();
  bool active() const { return active_; }
  bool scan(uint8_t seconds = 4);
  const std::vector<HidDeviceInfo>& devices() const { return devices_; }
  bool connect(size_t index);
  size_t connectedCount() const;
  String statusText() const;

  bool popKey(HidKeyEvent& event);
  bool popMouse(HidMouseEvent& event);

 private:
  class ClientCallbacks;
  static HidInputService* instance_;

  bool active_ = false;
  std::vector<HidDeviceInfo> devices_;
  std::vector<BLEClient*> clients_;
  std::vector<HidKeyEvent> keys_;
  std::vector<HidMouseEvent> mice_;
  uint8_t previousKeys_[6] = {0,0,0,0,0,0};

  static void notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool isNotify);
  void handleReport(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length);
  void handleKeyboard(const uint8_t* data, size_t length);
  void handleMouse(const uint8_t* data, size_t length);
  char keycodeToAscii(uint8_t code, bool shift) const;
  bool keyWasDown(uint8_t code) const;
};

}

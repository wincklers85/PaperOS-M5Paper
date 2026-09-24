#include "HidInputService.h"
#include <BLERemoteService.h>
#include <BLERemoteCharacteristic.h>
#include <BLERemoteDescriptor.h>
#include <algorithm>

namespace paperos {

HidInputService* HidInputService::instance_ = nullptr;
static BLEUUID HID_SERVICE((uint16_t)0x1812);
static BLEUUID HID_REPORT((uint16_t)0x2A4D);
static BLEUUID BOOT_KEYBOARD_INPUT((uint16_t)0x2A22);
static BLEUUID BOOT_MOUSE_INPUT((uint16_t)0x2A33);
static BLEUUID PROTOCOL_MODE((uint16_t)0x2A4E);

class HidInputService::ClientCallbacks : public BLEClientCallbacks {
 public:
  void onConnect(BLEClient*) override {}
  void onDisconnect(BLEClient*) override {}
};

bool HidInputService::begin() {
  if (active_) return true;
  instance_ = this;

  BLESecurity* security = new BLESecurity();
  security->setCapability(ESP_IO_CAP_NONE);
  security->setAuthenticationMode(true, false, true);

  active_ = true;
  return true;
}

void HidInputService::stop() {
  for (auto* client : clients_) {
    if (client && client->isConnected()) client->disconnect();
  }
  clients_.clear();
  devices_.clear();
  keys_.clear();
  mice_.clear();
  active_ = false;
}

bool HidInputService::scan(uint8_t seconds) {
  if (!active_) begin();
  devices_.clear();

  BLEScan* scan = BLEDevice::getScan();
  scan->setActiveScan(true);
  scan->setInterval(96);
  scan->setWindow(64);
  BLEScanResults results = scan->start(seconds, false);

  for (int i = 0; i < results.getCount(); ++i) {
    BLEAdvertisedDevice d = results.getDevice(i);
    bool hid = d.haveServiceUUID() && d.isAdvertisingService(HID_SERVICE);
    uint16_t appearance = d.haveAppearance() ? d.getAppearance() : 0;

    String name = d.haveName() ? String(d.getName().c_str()) : String();
    String lower = name;
    lower.toLowerCase();
    bool keyboard = appearance == 0x03C1 || lower.indexOf("keyboard") >= 0 || lower.indexOf("keys") >= 0;
    bool mouse = appearance == 0x03C2 || lower.indexOf("mouse") >= 0;

    if (!hid && !keyboard && !mouse) continue;

    HidDeviceInfo info;
    info.name = name.length() ? name : String("BLE HID device");
    info.address = String(d.getAddress().toString().c_str());
    info.rssi = d.getRSSI();
    info.appearance = appearance;
    info.likelyKeyboard = keyboard;
    info.likelyMouse = mouse;
    devices_.push_back(info);
  }

  scan->clearResults();
  return true;
}

bool HidInputService::connect(size_t index) {
  if (index >= devices_.size()) return false;
  if (!active_) begin();

  BLEClient* client = BLEDevice::createClient();
  if (!client) return false;
  client->setClientCallbacks(new ClientCallbacks());

  BLEAddress address(devices_[index].address.c_str());
  if (!client->connect(address)) {
    delete client;
    return false;
  }

  BLERemoteService* hid = client->getService(HID_SERVICE);
  if (!hid) {
    client->disconnect();
    delete client;
    return false;
  }

  BLERemoteCharacteristic* protocol = hid->getCharacteristic(PROTOCOL_MODE);
  if (protocol && protocol->canWrite()) {
    uint8_t boot = 0;
    protocol->writeValue(&boot, 1, true);
  }

  bool subscribed = false;
  BLERemoteCharacteristic* keyboard = hid->getCharacteristic(BOOT_KEYBOARD_INPUT);
  if (keyboard && keyboard->canNotify()) {
    keyboard->registerForNotify(notifyCallback);
    subscribed = true;
  }

  BLERemoteCharacteristic* mouse = hid->getCharacteristic(BOOT_MOUSE_INPUT);
  if (mouse && mouse->canNotify()) {
    mouse->registerForNotify(notifyCallback);
    subscribed = true;
  }

  auto* chars = hid->getCharacteristics();
  if (chars) {
    for (const auto& entry : *chars) {
      BLERemoteCharacteristic* c = entry.second;
      if (!c || !c->getUUID().equals(HID_REPORT) || !c->canNotify()) continue;

      BLERemoteDescriptor* ref = c->getDescriptor(BLEUUID((uint16_t)0x2908));
      bool inputReport = true;
      if (ref) {
        String rv = ref->readValue();
        if (rv.length() >= 2) inputReport = ((uint8_t)rv[1] == 1);
      }
      if (inputReport) {
        c->registerForNotify(notifyCallback);
        subscribed = true;
      }
    }
  }

  if (!subscribed) {
    client->disconnect();
    delete client;
    return false;
  }

  clients_.push_back(client);
  return true;
}

size_t HidInputService::connectedCount() const {
  size_t n = 0;
  for (auto* c : clients_) if (c && c->isConnected()) ++n;
  return n;
}

String HidInputService::statusText() const {
  if (!active_) return "HID host stopped";
  size_t n = connectedCount();
  if (n) return String(n) + " HID device(s) connected";
  if (devices_.size()) return String(devices_.size()) + " HID device(s) found";
  return "Ready to scan for BLE HID";
}

void HidInputService::notifyCallback(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length, bool) {
  if (instance_) instance_->handleReport(characteristic, data, length);
}

void HidInputService::handleReport(BLERemoteCharacteristic* characteristic, uint8_t* data, size_t length) {
  if (!characteristic || !data || !length) return;
  BLEUUID uuid = characteristic->getUUID();

  if (uuid.equals(BOOT_KEYBOARD_INPUT)) {
    handleKeyboard(data, length);
    return;
  }
  if (uuid.equals(BOOT_MOUSE_INPUT)) {
    handleMouse(data, length);
    return;
  }

  if (length >= 8) handleKeyboard(data, length);
  else if (length >= 3 && length <= 6) handleMouse(data, length);
}

bool HidInputService::keyWasDown(uint8_t code) const {
  for (uint8_t k : previousKeys_) if (k == code) return true;
  return false;
}

void HidInputService::handleKeyboard(const uint8_t* data, size_t length) {
  if (length < 8) return;
  uint8_t mod = data[0];
  bool shift = (mod & 0x22) != 0;
  bool ctrl = (mod & 0x11) != 0;
  bool alt = (mod & 0x44) != 0;

  for (int i = 2; i < 8; ++i) {
    uint8_t code = data[i];
    if (!code || keyWasDown(code)) continue;
    HidKeyEvent e;
    e.keycode = code;
    e.ascii = keycodeToAscii(code, shift);
    e.shift = shift;
    e.ctrl = ctrl;
    e.alt = alt;
    keys_.push_back(e);
    if (keys_.size() > 32) keys_.erase(keys_.begin());
  }
  for (int i = 0; i < 6; ++i) previousKeys_[i] = data[i + 2];
}

void HidInputService::handleMouse(const uint8_t* data, size_t length) {
  if (length < 3) return;
  HidMouseEvent e;
  e.buttons = data[0];
  e.dx = static_cast<int8_t>(data[1]);
  e.dy = static_cast<int8_t>(data[2]);
  e.wheel = length >= 4 ? static_cast<int8_t>(data[3]) : 0;
  mice_.push_back(e);
  if (mice_.size() > 32) mice_.erase(mice_.begin());
}

char HidInputService::keycodeToAscii(uint8_t code, bool shift) const {
  if (code >= 0x04 && code <= 0x1D) {
    char c = 'a' + (code - 0x04);
    return shift ? (char)(c - 'a' + 'A') : c;
  }
  if (code >= 0x1E && code <= 0x27) {
    static const char normal[] = "1234567890";
    static const char shifted[] = "!@#$%^&*()";
    return shift ? shifted[code - 0x1E] : normal[code - 0x1E];
  }
  switch (code) {
    case 0x28: return '\n';
    case 0x2A: return '\b';
    case 0x2B: return '\t';
    case 0x2C: return ' ';
    case 0x2D: return shift ? '_' : '-';
    case 0x2E: return shift ? '+' : '=';
    case 0x2F: return shift ? '{' : '[';
    case 0x30: return shift ? '}' : ']';
    case 0x31: return shift ? '|' : '\\';
    case 0x33: return shift ? ':' : ';';
    case 0x34: return shift ? '"' : '\'';
    case 0x35: return shift ? '~' : '`';
    case 0x36: return shift ? '<' : ',';
    case 0x37: return shift ? '>' : '.';
    case 0x38: return shift ? '?' : '/';
    default: return 0;
  }
}

bool HidInputService::popKey(HidKeyEvent& event) {
  if (keys_.empty()) return false;
  event = keys_.front();
  keys_.erase(keys_.begin());
  return true;
}

bool HidInputService::popMouse(HidMouseEvent& event) {
  if (mice_.empty()) return false;
  event = mice_.front();
  mice_.erase(mice_.begin());
  return true;
}

}

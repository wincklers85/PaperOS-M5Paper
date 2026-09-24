#include "PhoneLinkService.h"
#include <ArduinoJson.h>
#include <BLE2902.h>

namespace paperos {

class PhoneLinkService::ServerCallbacks : public BLEServerCallbacks {
 public:
  explicit ServerCallbacks(PhoneLinkService* owner) : owner_(owner) {}
  void onConnect(BLEServer*) override { if (owner_) owner_->onConnected(true); }
  void onDisconnect(BLEServer*) override {
    if (owner_) owner_->onConnected(false);
    BLEDevice::startAdvertising();
  }
 private:
  PhoneLinkService* owner_;
};

class PhoneLinkService::NotificationCallbacks : public BLECharacteristicCallbacks {
 public:
  explicit NotificationCallbacks(PhoneLinkService* owner) : owner_(owner) {}
  void onWrite(BLECharacteristic* characteristic) override {
    if (!owner_ || !characteristic) return;
    std::string raw = characteristic->getValue();
    String payload;
    payload.reserve(raw.size());
    for (char c : raw) payload += c;
    owner_->onNotificationWrite(payload);
  }
 private:
  PhoneLinkService* owner_;
};

bool PhoneLinkService::begin() {
  if (active_) return true;

  BLEDevice::init("PaperOS Phone Link");
  server_ = BLEDevice::createServer();
  if (!server_) return false;
  server_->setCallbacks(new ServerCallbacks(this));

  service_ = server_->createService(SERVICE_UUID);
  if (!service_) return false;

  notifyIn_ = service_->createCharacteristic(
      NOTIFY_IN_UUID,
      BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
  commandOut_ = service_->createCharacteristic(
      COMMAND_OUT_UUID,
      BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY);

  if (!notifyIn_ || !commandOut_) return false;
  notifyIn_->setCallbacks(new NotificationCallbacks(this));
  commandOut_->addDescriptor(new BLE2902());
  commandOut_->setValue("ready");

  service_->start();
  BLEAdvertising* advertising = BLEDevice::getAdvertising();
  advertising->addServiceUUID(SERVICE_UUID);
  advertising->setScanResponse(true);
  advertising->start();

  active_ = true;
  return true;
}

void PhoneLinkService::stop() {
  if (!active_) return;
  BLEDevice::getAdvertising()->stop();
  connected_ = false;
  active_ = false;
}

const PhoneNotification* PhoneLinkService::notification(size_t index) const {
  if (index >= notifications_.size()) return nullptr;
  return &notifications_[index];
}

void PhoneLinkService::clearNotifications() {
  notifications_.clear();
}

void PhoneLinkService::pushNotification(const String& app, const String& title, const String& body) {
  PhoneNotification n;
  n.app = app.length() ? app : "Phone";
  n.title = title;
  n.body = body;
  n.receivedMs = millis();
  notifications_.insert(notifications_.begin(), n);
  if (notifications_.size() > 30) notifications_.resize(30);
}

void PhoneLinkService::onNotificationWrite(const String& payload) {
  DynamicJsonDocument doc(1536);
  if (!deserializeJson(doc, payload)) {
    pushNotification(
      String((const char*)(doc["app"] | "Phone")),
      String((const char*)(doc["title"] | "")),
      String((const char*)(doc["body"] | ""))
    );
    return;
  }
  pushNotification("Phone", "Incoming", payload);
}

void PhoneLinkService::onConnected(bool connected) {
  connected_ = connected;
}

bool PhoneLinkService::sendCommand(const String& command) {
  if (!active_ || !commandOut_ || !command.length()) return false;
  lastCommand_ = command;
  commandOut_->setValue(command.c_str());
  if (connected_) commandOut_->notify();
  return true;
}

String PhoneLinkService::statusText() const {
  if (!active_) return "Bridge stopped";
  return connected_ ? "Phone companion connected" : "Waiting for companion";
}

}

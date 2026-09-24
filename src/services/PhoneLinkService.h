#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <vector>

namespace paperos {

struct PhoneNotification {
  String app;
  String title;
  String body;
  uint32_t receivedMs = 0;
};

class PhoneLinkService {
 public:
  bool begin();
  void stop();
  bool active() const { return active_; }
  bool connected() const { return connected_; }

  size_t notificationCount() const { return notifications_.size(); }
  const PhoneNotification* notification(size_t index) const;
  void clearNotifications();
  void pushNotification(const String& app, const String& title, const String& body);

  bool sendCommand(const String& command);
  String lastCommand() const { return lastCommand_; }
  String statusText() const;

  static constexpr const char* SERVICE_UUID = "6f0c1000-4e55-4c42-8a8d-50415045524f";
  static constexpr const char* NOTIFY_IN_UUID = "6f0c1001-4e55-4c42-8a8d-50415045524f";
  static constexpr const char* COMMAND_OUT_UUID = "6f0c1002-4e55-4c42-8a8d-50415045524f";

 private:
  class ServerCallbacks;
  class NotificationCallbacks;

  bool active_ = false;
  bool connected_ = false;
  BLEServer* server_ = nullptr;
  BLEService* service_ = nullptr;
  BLECharacteristic* notifyIn_ = nullptr;
  BLECharacteristic* commandOut_ = nullptr;
  std::vector<PhoneNotification> notifications_;
  String lastCommand_;

  void onConnected(bool connected);
  void onNotificationWrite(const String& payload);
};

}

#pragma once
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEClient.h>
#include <BLEUtils.h>
#include <vector>

namespace paperos {

struct PhoneNotification {
  String app;
  String title;
  String body;
  uint8_t category = 0;
  uint32_t uid = 0;
  uint32_t receivedMs = 0;
  bool positiveAction = false;
  bool negativeAction = false;
};

class PhoneLinkService {
 public:
  bool begin();
  void stop();
  bool active() const { return active_; }
  bool connected() const { return connected_; }
  bool ancsReady() const { return ancsReady_; }
  bool bonded() const { return bonded_; }
  String peerAddress() const { return peerAddress_; }

  size_t notificationCount() const { return notifications_.size(); }
  const PhoneNotification* notification(size_t index) const;
  void clearNotifications();
  void pushNotification(const String& app, const String& title, const String& body);

  bool sendCommand(const String& command);
  bool performNotificationAction(uint32_t uid, bool positive);
  String lastCommand() const { return lastCommand_; }
  String statusText() const;
  String pairingHelp() const;

  static constexpr const char* SERVICE_UUID = "6f0c1000-4e55-4c42-8a8d-50415045524f";
  static constexpr const char* NOTIFY_IN_UUID = "6f0c1001-4e55-4c42-8a8d-50415045524f";
  static constexpr const char* COMMAND_OUT_UUID = "6f0c1002-4e55-4c42-8a8d-50415045524f";
  static constexpr const char* ANCS_SERVICE_UUID = "7905F431-B5CE-4E99-A40F-4B1E122D00D0";
  static constexpr const char* ANCS_NOTIFICATION_SOURCE_UUID = "9FBF120D-6301-42D9-8C58-25E699A21DBD";
  static constexpr const char* ANCS_CONTROL_POINT_UUID = "69D1D8F3-45E1-49A8-9821-9BBDFDAAD9D9";
  static constexpr const char* ANCS_DATA_SOURCE_UUID = "22EAC6E9-24D6-4BB5-BE44-B36ACE7C7BFB";

 private:
  class ServerCallbacks;
  class NotificationCallbacks;
  class SecurityCallbacks;

  bool active_ = false;
  bool connected_ = false;
  bool ancsReady_ = false;
  bool bonded_ = false;
  BLEServer* server_ = nullptr;
  BLEService* service_ = nullptr;
  BLECharacteristic* notifyIn_ = nullptr;
  BLECharacteristic* commandOut_ = nullptr;
  BLEClient* ancsClient_ = nullptr;
  BLERemoteCharacteristic* ancsControlPoint_ = nullptr;
  std::vector<PhoneNotification> notifications_;
  String lastCommand_;
  String peerAddress_;

  uint32_t pendingUid_ = 0;
  uint8_t pendingCategory_ = 0;
  uint8_t pendingFlags_ = 0;
  String ancsBuffer_;

  void onConnected(bool connected, const String& address = "");
  void onNotificationWrite(const String& payload);
  void startAncsClient(const String& address);
  static void ancsTaskThunk(void* arg);
  void ancsTask();
  bool setupAncs();
  void requestNotificationAttributes(uint32_t uid);
  void handleNotificationSource(uint8_t* data, size_t length);
  void handleDataSource(uint8_t* data, size_t length);
  void parseAncsBuffer();
  static void notificationSourceCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static void dataSourceCallback(BLERemoteCharacteristic*, uint8_t*, size_t, bool);
  static PhoneLinkService* instance_;
};

}

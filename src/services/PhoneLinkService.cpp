#include "PhoneLinkService.h"
#include <ArduinoJson.h>
#include <BLE2902.h>
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>

namespace paperos {

PhoneLinkService* PhoneLinkService::instance_ = nullptr;

static void addAncsSolicitation(BLEAdvertisementData& data) {
  BLEUUID uuid(PhoneLinkService::ANCS_SERVICE_UUID);
  char hdr[2] = {17, ESP_BLE_AD_TYPE_128SOL_SRV_UUID};
  data.addData(hdr, 2);
  esp_bt_uuid_t* native = uuid.getNative();
  data.addData(reinterpret_cast<char*>(native->uuid.uuid128), 16);
}

class PhoneLinkService::SecurityCallbacks : public BLESecurityCallbacks {
 public:
  explicit SecurityCallbacks(PhoneLinkService* owner) : owner_(owner) {}
  uint32_t onPassKeyRequest() override { return 0; }
  void onPassKeyNotify(uint32_t) override {}
  bool onSecurityRequest() override { return true; }
  bool onConfirmPIN(uint32_t) override { return true; }
  void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
    if (!owner_) return;
    owner_->bonded_ = cmpl.success;
  }
 private:
  PhoneLinkService* owner_;
};

class PhoneLinkService::ServerCallbacks : public BLEServerCallbacks {
 public:
  explicit ServerCallbacks(PhoneLinkService* owner) : owner_(owner) {}
  void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    if (!owner_) return;
    String address = BLEAddress(param->connect.remote_bda).toString().c_str();
    owner_->onConnected(true, address);
  }
  void onConnect(BLEServer*) override {
    if (owner_) owner_->onConnected(true);
  }
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
  instance_ = this;

  BLEDevice::init("PaperOS");
  BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT);
  BLEDevice::setSecurityCallbacks(new SecurityCallbacks(this));

  BLESecurity* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_BOND);
  security->setCapability(ESP_IO_CAP_NONE);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  security->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);

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

  BLEAdvertising* advertising = server_->getAdvertising();
  BLEAdvertisementData adv;
  adv.setFlags(ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT);
  addAncsSolicitation(adv);
  advertising->setAdvertisementData(adv);

  BLEAdvertisementData scan;
  scan.setName("PaperOS");
  scan.setCompleteServices(BLEUUID(SERVICE_UUID));
  advertising->setScanResponseData(scan);
  advertising->setMinPreferred(0x06);
  advertising->setMaxPreferred(0x12);
  advertising->start();

  active_ = true;
  return true;
}

void PhoneLinkService::stop() {
  if (!active_) return;
  if (ancsClient_ && ancsClient_->isConnected()) ancsClient_->disconnect();
  BLEDevice::getAdvertising()->stop();
  connected_ = false;
  ancsReady_ = false;
  active_ = false;
}

const PhoneNotification* PhoneLinkService::notification(size_t index) const {
  if (index >= notifications_.size()) return nullptr;
  return &notifications_[index];
}

void PhoneLinkService::clearNotifications() { notifications_.clear(); }

void PhoneLinkService::pushNotification(const String& app, const String& title, const String& body) {
  PhoneNotification n;
  n.app = app.length() ? app : "Phone";
  n.title = title;
  n.body = body;
  n.receivedMs = millis();
  notifications_.insert(notifications_.begin(), n);
  if (notifications_.size() > 40) notifications_.resize(40);
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

void PhoneLinkService::onConnected(bool connected, const String& address) {
  connected_ = connected;
  if (!connected) {
    ancsReady_ = false;
    return;
  }
  if (address.length()) {
    peerAddress_ = address;
    startAncsClient(address);
  }
}

void PhoneLinkService::startAncsClient(const String& address) {
  peerAddress_ = address;
  xTaskCreatePinnedToCore(ancsTaskThunk, "paperos_ancs", 12288, this, 1, nullptr, 0);
}

void PhoneLinkService::ancsTaskThunk(void* arg) {
  auto* self = static_cast<PhoneLinkService*>(arg);
  if (self) self->ancsTask();
  vTaskDelete(nullptr);
}

void PhoneLinkService::ancsTask() {
  delay(900);
  if (!peerAddress_.length()) return;

  BLEAddress address(peerAddress_.c_str());
  ancsClient_ = BLEDevice::createClient();
  if (!ancsClient_) return;
  if (!ancsClient_->connect(address)) return;

  setupAncs();
}

bool PhoneLinkService::setupAncs() {
  if (!ancsClient_ || !ancsClient_->isConnected()) return false;

  BLERemoteService* ancs = ancsClient_->getService(BLEUUID(ANCS_SERVICE_UUID));
  if (!ancs) return false;

  auto* source = ancs->getCharacteristic(BLEUUID(ANCS_NOTIFICATION_SOURCE_UUID));
  ancsControlPoint_ = ancs->getCharacteristic(BLEUUID(ANCS_CONTROL_POINT_UUID));
  auto* data = ancs->getCharacteristic(BLEUUID(ANCS_DATA_SOURCE_UUID));
  if (!source || !ancsControlPoint_ || !data) return false;

  source->registerForNotify(notificationSourceCallback);
  data->registerForNotify(dataSourceCallback);

  const uint8_t enable[] = {0x01, 0x00};
  auto* sDesc = source->getDescriptor(BLEUUID((uint16_t)0x2902));
  auto* dDesc = data->getDescriptor(BLEUUID((uint16_t)0x2902));
  if (sDesc) sDesc->writeValue((uint8_t*)enable, 2, true);
  if (dDesc) dDesc->writeValue((uint8_t*)enable, 2, true);

  ancsReady_ = true;
  bonded_ = true;
  return true;
}

void PhoneLinkService::notificationSourceCallback(BLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  if (instance_) instance_->handleNotificationSource(data, length);
}

void PhoneLinkService::dataSourceCallback(BLERemoteCharacteristic*, uint8_t* data, size_t length, bool) {
  if (instance_) instance_->handleDataSource(data, length);
}

void PhoneLinkService::handleNotificationSource(uint8_t* data, size_t length) {
  if (!data || length < 8) return;
  uint8_t eventId = data[0];
  uint8_t flags = data[1];
  uint8_t category = data[2];
  uint32_t uid = (uint32_t)data[4] | ((uint32_t)data[5] << 8) |
                 ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);

  if (eventId == 2) {
    for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
      if (it->uid == uid) { notifications_.erase(it); break; }
    }
    return;
  }

  pendingUid_ = uid;
  pendingCategory_ = category;
  pendingFlags_ = flags;
  requestNotificationAttributes(uid);
}

void PhoneLinkService::requestNotificationAttributes(uint32_t uid) {
  if (!ancsControlPoint_) return;
  uint8_t req[32];
  size_t i = 0;
  req[i++] = 0x00;
  req[i++] = uid & 0xFF;
  req[i++] = (uid >> 8) & 0xFF;
  req[i++] = (uid >> 16) & 0xFF;
  req[i++] = (uid >> 24) & 0xFF;
  req[i++] = 0x00; // AppIdentifier
  req[i++] = 0x01; req[i++] = 64; req[i++] = 0;   // Title
  req[i++] = 0x03; req[i++] = 160; req[i++] = 0;  // Message
  req[i++] = 0x06; req[i++] = 32; req[i++] = 0;   // Positive action label
  req[i++] = 0x07; req[i++] = 32; req[i++] = 0;   // Negative action label
  ancsBuffer_ = "";
  ancsControlPoint_->writeValue(req, i, true);
}

void PhoneLinkService::handleDataSource(uint8_t* data, size_t length) {
  if (!data || !length) return;
  for (size_t i = 0; i < length; ++i) ancsBuffer_ += (char)data[i];
  parseAncsBuffer();
}

void PhoneLinkService::parseAncsBuffer() {
  if (ancsBuffer_.length() < 5) return;
  const uint8_t* b = reinterpret_cast<const uint8_t*>(ancsBuffer_.c_str());
  if (b[0] != 0x00) return;
  uint32_t uid = (uint32_t)b[1] | ((uint32_t)b[2] << 8) | ((uint32_t)b[3] << 16) | ((uint32_t)b[4] << 24);
  size_t p = 5;
  String app, title, body, positive, negative;

  while (p + 3 <= ancsBuffer_.length()) {
    uint8_t attr = (uint8_t)ancsBuffer_[p++];
    uint16_t len = (uint8_t)ancsBuffer_[p] | ((uint16_t)(uint8_t)ancsBuffer_[p+1] << 8);
    p += 2;
    if (p + len > ancsBuffer_.length()) return;
    String value;
    value.reserve(len);
    for (uint16_t i = 0; i < len; ++i) value += ancsBuffer_[p+i];
    p += len;
    if (attr == 0x00) app = value;
    else if (attr == 0x01) title = value;
    else if (attr == 0x03) body = value;
    else if (attr == 0x06) positive = value;
    else if (attr == 0x07) negative = value;
  }

  if (uid != pendingUid_ || (!title.length() && !body.length() && !app.length())) return;

  PhoneNotification n;
  n.app = app.length() ? app : "iPhone";
  n.title = title;
  n.body = body;
  n.category = pendingCategory_;
  n.uid = uid;
  n.receivedMs = millis();
  n.positiveAction = (pendingFlags_ & (1 << 3)) != 0;
  n.negativeAction = (pendingFlags_ & (1 << 4)) != 0;

  for (auto it = notifications_.begin(); it != notifications_.end(); ++it) {
    if (it->uid == uid) { notifications_.erase(it); break; }
  }
  notifications_.insert(notifications_.begin(), n);
  if (notifications_.size() > 40) notifications_.resize(40);
  ancsBuffer_ = "";
}

bool PhoneLinkService::performNotificationAction(uint32_t uid, bool positive) {
  if (!ancsReady_ || !ancsControlPoint_) return false;
  uint8_t req[6] = {
    0x02,
    (uint8_t)(uid & 0xFF), (uint8_t)((uid >> 8) & 0xFF),
    (uint8_t)((uid >> 16) & 0xFF), (uint8_t)((uid >> 24) & 0xFF),
    (uint8_t)(positive ? 0x00 : 0x01)
  };
  return ancsControlPoint_->writeValue(req, sizeof(req), true);
}

bool PhoneLinkService::sendCommand(const String& command) {
  if (!active_ || !commandOut_ || !command.length()) return false;
  lastCommand_ = command;
  commandOut_->setValue(command.c_str());
  if (connected_) commandOut_->notify();
  return true;
}

String PhoneLinkService::statusText() const {
  if (!active_) return "Phone Link stopped";
  if (ancsReady_) return "iPhone ANCS connected";
  if (bonded_) return "Bonded / waiting for ANCS";
  if (connected_) return "Connected / pairing";
  return "Pairing mode / waiting for iPhone";
}

String PhoneLinkService::pairingHelp() const {
  return "Use nRF Connect on iPhone: Scan > PaperOS > Connect > Pair > Allow Notifications.";
}

}

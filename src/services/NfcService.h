#pragma once
#include <Arduino.h>
#include <PN532_HSU.h>
#include <PN532.h>

namespace paperos {

struct NfcTagInfo {
  bool found = false;
  String uid;
  String type;
  uint8_t uidLength = 0;
};

class NfcService {
 public:
  bool begin();
  bool ready() const { return ready_; }
  uint32_t firmwareVersion() const { return firmwareVersion_; }
  String firmwareText() const;
  String statusText() const;
  NfcTagInfo scan(uint16_t timeoutMs = 1500);
  void stop();

  static constexpr int RX_PIN = 18; // Port C: M5Paper RX <- PN532 TX
  static constexpr int TX_PIN = 19; // Port C: M5Paper TX -> PN532 RX

 private:
  PN532_HSU* hsu_ = nullptr;
  PN532* nfc_ = nullptr;
  bool ready_ = false;
  uint32_t firmwareVersion_ = 0;
};

}

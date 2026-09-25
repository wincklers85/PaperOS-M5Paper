#pragma once
#include <Arduino.h>
#include <PN532_HSU.h>
#include <PN532.h>
#include <vector>

namespace paperos {

enum class NfcTagKind : uint8_t {
  Unknown = 0,
  Type2,
  MifareClassicCompatible,
  Felica
};

struct NfcTagInfo {
  bool found = false;
  String uid;
  String type;
  String technology;
  String product;
  String ndefType;
  String ndefPayload;
  String memoryPreview;
  uint8_t uidLength = 0;
  uint16_t capacityBytes = 0;
  bool ndefPresent = false;
  bool writable = false;
  bool protectedData = false;
  NfcTagKind kind = NfcTagKind::Unknown;
};

class NfcService {
 public:
  bool begin();
  bool ready() const { return ready_; }
  uint32_t firmwareVersion() const { return firmwareVersion_; }
  String firmwareText() const;
  String statusText() const;

  // Auto-detects ISO14443A first, then NFC-F/FeliCa.
  NfcTagInfo scan(uint16_t timeoutMs = 1500);
  const NfcTagInfo& lastTag() const { return lastTag_; }

  // NFC Forum Type 2 / NTAG / Ultralight helpers. These touch user-memory
  // pages only; manufacturer, lock/config and protected credential areas are
  // never modified by these helpers.
  bool readType2UserMemory(String& hexDump, uint16_t maxBytes = 256);
  bool writeNdefText(const String& text, String& status);
  bool writeNdefUri(const String& uri, String& status);

  // Human-readable snapshot suitable for saving/displaying elsewhere.
  String snapshotText() const;

  void stop();

  static constexpr int RX_PIN = 18; // Port C: M5Paper RX <- PN532 TX
  static constexpr int TX_PIN = 19; // Port C: M5Paper TX -> PN532 RX

 private:
  PN532_HSU* hsu_ = nullptr;
  PN532* nfc_ = nullptr;
  bool ready_ = false;
  uint32_t firmwareVersion_ = 0;
  NfcTagInfo lastTag_;
  uint8_t lastUid_[10] = {0};
  uint8_t lastUidLength_ = 0;

  bool detectType2(uint8_t* version, uint8_t& versionLen);
  bool fillType2Info(NfcTagInfo& info);
  bool scanFelica(NfcTagInfo& info, uint16_t timeoutMs);
  bool readType2Bytes(std::vector<uint8_t>& data, uint16_t maxBytes = 0);
  bool parseNdef(const std::vector<uint8_t>& data, NfcTagInfo& info) const;
  bool writeType2NdefRecord(const std::vector<uint8_t>& record, String& status);
  static String uidToString(const uint8_t* uid, uint8_t length);
  static String hexPreview(const uint8_t* data, size_t length, size_t maxBytes = 64);
  static String type2ProductName(const uint8_t* version, uint8_t length);
};

}

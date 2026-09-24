#include "NfcService.h"

namespace paperos {

bool NfcService::begin() {
  if (ready_ && nfc_) return true;

  Serial2.end();
  delay(30);
  Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);

  if (!hsu_) hsu_ = new PN532_HSU(Serial2, RX_PIN, TX_PIN);
  if (!nfc_) nfc_ = new PN532(*hsu_);

  hsu_->begin();
  nfc_->begin();
  firmwareVersion_ = nfc_->getFirmwareVersion();
  if (!firmwareVersion_) {
    ready_ = false;
    return false;
  }

  if (!nfc_->SAMConfig()) {
    ready_ = false;
    return false;
  }
  nfc_->setPassiveActivationRetries(0x10);
  ready_ = true;
  return true;
}

void NfcService::stop() {
  ready_ = false;
  Serial2.end();
}

String NfcService::firmwareText() const {
  if (!firmwareVersion_) return "Not detected";
  uint8_t ic = (firmwareVersion_ >> 24) & 0xFF;
  uint8_t major = (firmwareVersion_ >> 16) & 0xFF;
  uint8_t minor = (firmwareVersion_ >> 8) & 0xFF;
  char out[40];
  snprintf(out, sizeof(out), "PN5%02X / FW %u.%u", ic, major, minor);
  return String(out);
}

String NfcService::statusText() const {
  if (ready_) return firmwareText();
  return "PN532 not initialized";
}

NfcTagInfo NfcService::scan(uint16_t timeoutMs) {
  NfcTagInfo info;
  if (!ready_ && !begin()) return info;

  uint8_t uid[7] = {0};
  uint8_t uidLength = 0;
  bool ok = nfc_->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, timeoutMs);
  if (!ok || !uidLength) return info;

  info.found = true;
  info.uidLength = uidLength;
  String hex;
  hex.reserve(uidLength * 3);
  for (uint8_t i = 0; i < uidLength; ++i) {
    if (i) hex += ':';
    char b[3];
    snprintf(b, sizeof(b), "%02X", uid[i]);
    hex += b;
  }
  info.uid = hex;

  if (uidLength == 4) info.type = "ISO14443A / MIFARE Classic-like";
  else if (uidLength == 7) info.type = "ISO14443A / Ultralight-NTAG-like";
  else info.type = String("ISO14443A / UID ") + uidLength + " bytes";
  return info;
}

}

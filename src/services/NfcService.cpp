#include "NfcService.h"

namespace paperos {

namespace {

String uriPrefix(uint8_t code) {
  switch (code) {
    case 0x01: return "http://www.";
    case 0x02: return "https://www.";
    case 0x03: return "http://";
    case 0x04: return "https://";
    case 0x05: return "tel:";
    case 0x06: return "mailto:";
    default: return "";
  }
}

}

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

String NfcService::uidToString(const uint8_t* uid, uint8_t length) {
  String hex;
  hex.reserve(length * 3);
  for (uint8_t i = 0; i < length; ++i) {
    if (i) hex += ':';
    char b[3];
    snprintf(b, sizeof(b), "%02X", uid[i]);
    hex += b;
  }
  return hex;
}

String NfcService::hexPreview(const uint8_t* data, size_t length, size_t maxBytes) {
  size_t show = min(length, maxBytes);
  String out;
  out.reserve(show * 3 + 8);
  for (size_t i = 0; i < show; ++i) {
    if (i) out += ' ';
    char b[3];
    snprintf(b, sizeof(b), "%02X", data[i]);
    out += b;
  }
  if (show < length) out += " ...";
  return out;
}

String NfcService::type2ProductName(const uint8_t* version, uint8_t length) {
  if (!version || length < 8) return "NFC Forum Type 2";
  const uint8_t productType = version[2];
  const uint8_t storage = version[6];

  if (productType == 0x04) {
    if (storage == 0x0F) return "NTAG213";
    if (storage == 0x11) return "NTAG215";
    if (storage == 0x13) return "NTAG216";
    return "NTAG / Type 2";
  }
  if (productType == 0x03) {
    if (storage == 0x0B) return "MIFARE Ultralight EV1 48B";
    if (storage == 0x0E) return "MIFARE Ultralight EV1 128B";
    return "MIFARE Ultralight / Type 2";
  }
  if (productType == 0x05) return "NTAG I2C / Type 2";
  return "NFC Forum Type 2";
}

bool NfcService::detectType2(uint8_t* version, uint8_t& versionLen) {
  if (!nfc_) return false;
  uint8_t cmd[1] = {0x60}; // GET_VERSION on NTAG/Ultralight EV1 family
  uint8_t response[16] = {0};
  uint8_t responseLen = sizeof(response);
  if (!nfc_->inDataExchange(cmd, sizeof(cmd), response, &responseLen)) return false;
  if (responseLen < 8) return false;

  // NXP Type 2 GET_VERSION starts with fixed header 00 04 and a Type 2
  // product family byte. Requiring this avoids confusing DESFire native
  // GET_VERSION responses with NTAG/Ultralight.
  if (response[0] != 0x00 || response[1] != 0x04) return false;
  if (!(response[2] == 0x03 || response[2] == 0x04 || response[2] == 0x05)) return false;

  versionLen = min<uint8_t>(responseLen, 8);
  memcpy(version, response, versionLen);
  return true;
}

bool NfcService::readType2Bytes(std::vector<uint8_t>& data, uint16_t maxBytes) {
  data.clear();
  if (!nfc_ || lastTag_.kind != NfcTagKind::Type2) return false;

  uint8_t ccBlock[16] = {0};
  if (!nfc_->mifareultralight_ReadPage(3, ccBlock)) return false;
  if (ccBlock[0] != 0xE1) return false;

  uint16_t capacity = static_cast<uint16_t>(ccBlock[2]) * 8U;
  if (!capacity) return false;
  if (maxBytes && capacity > maxBytes) capacity = maxBytes;

  data.reserve(capacity);
  for (uint16_t offset = 0; offset < capacity; offset += 16) {
    uint8_t block[16] = {0};
    uint8_t page = static_cast<uint8_t>(4 + (offset / 4));
    if (!nfc_->mifareultralight_ReadPage(page, block)) break;
    size_t take = min<size_t>(16, capacity - offset);
    data.insert(data.end(), block, block + take);
  }
  return !data.empty();
}

bool NfcService::parseNdef(const std::vector<uint8_t>& data, NfcTagInfo& info) const {
  if (data.empty()) return false;

  size_t p = 0;
  size_t ndefStart = 0;
  size_t ndefLen = 0;
  bool found = false;

  while (p < data.size()) {
    uint8_t tlv = data[p++];
    if (tlv == 0x00) continue; // NULL TLV
    if (tlv == 0xFE) break;    // Terminator TLV
    if (p >= data.size()) break;

    size_t len = data[p++];
    if (len == 0xFF) {
      if (p + 1 >= data.size()) break;
      len = (static_cast<size_t>(data[p]) << 8) | data[p + 1];
      p += 2;
    }
    if (p + len > data.size()) break;

    if (tlv == 0x03) {
      ndefStart = p;
      ndefLen = len;
      found = true;
      break;
    }
    p += len;
  }

  if (!found || ndefLen < 3) return false;
  info.ndefPresent = true;

  const uint8_t* r = data.data() + ndefStart;
  size_t remaining = ndefLen;
  uint8_t header = r[0];
  bool shortRecord = header & 0x10;
  bool hasId = header & 0x08;
  if (remaining < (shortRecord ? 3U : 6U)) return true;

  size_t idx = 1;
  uint8_t typeLen = r[idx++];
  uint32_t payloadLen = 0;
  if (shortRecord) {
    payloadLen = r[idx++];
  } else {
    payloadLen = (static_cast<uint32_t>(r[idx]) << 24) |
                 (static_cast<uint32_t>(r[idx + 1]) << 16) |
                 (static_cast<uint32_t>(r[idx + 2]) << 8) |
                 static_cast<uint32_t>(r[idx + 3]);
    idx += 4;
  }

  uint8_t idLen = 0;
  if (hasId) {
    if (idx >= remaining) return true;
    idLen = r[idx++];
  }
  if (idx + typeLen + idLen + payloadLen > remaining) return true;

  String recordType;
  for (uint8_t i = 0; i < typeLen; ++i) recordType += static_cast<char>(r[idx + i]);
  idx += typeLen + idLen;
  const uint8_t* payload = r + idx;

  if (recordType == "T" && payloadLen >= 1) {
    info.ndefType = "Text";
    uint8_t status = payload[0];
    uint8_t langLen = status & 0x3F;
    bool utf16 = status & 0x80;
    if (!utf16 && payloadLen > static_cast<uint32_t>(1 + langLen)) {
      for (uint32_t i = 1 + langLen; i < payloadLen; ++i) {
        char c = static_cast<char>(payload[i]);
        if (c >= 0x20 || c == '\t') info.ndefPayload += c;
      }
    } else if (utf16) {
      info.ndefPayload = "UTF-16 text record";
    }
  } else if (recordType == "U" && payloadLen >= 1) {
    info.ndefType = "URI";
    info.ndefPayload = uriPrefix(payload[0]);
    for (uint32_t i = 1; i < payloadLen; ++i) info.ndefPayload += static_cast<char>(payload[i]);
  } else {
    info.ndefType = recordType.length() ? recordType : "NDEF";
    info.ndefPayload = hexPreview(payload, payloadLen, 48);
  }

  if (info.ndefPayload.length() > 180) info.ndefPayload = info.ndefPayload.substring(0, 177) + "...";
  return true;
}

bool NfcService::fillType2Info(NfcTagInfo& info) {
  uint8_t version[8] = {0};
  uint8_t versionLen = 0;
  if (!detectType2(version, versionLen)) return false;

  info.kind = NfcTagKind::Type2;
  info.technology = "NFC-A / NFC Forum Type 2";
  info.product = type2ProductName(version, versionLen);
  info.type = info.product + " / Type 2";

  uint8_t ccBlock[16] = {0};
  if (nfc_->mifareultralight_ReadPage(3, ccBlock) && ccBlock[0] == 0xE1) {
    info.capacityBytes = static_cast<uint16_t>(ccBlock[2]) * 8U;
    info.writable = (ccBlock[3] & 0x0F) == 0x00;
  }

  std::vector<uint8_t> memory;
  if (readType2Bytes(memory, min<uint16_t>(info.capacityBytes ? info.capacityBytes : 256, 256))) {
    info.memoryPreview = hexPreview(memory.data(), memory.size(), 64);
    parseNdef(memory, info);
  }
  return true;
}

bool NfcService::scanFelica(NfcTagInfo& info, uint16_t timeoutMs) {
  if (!nfc_) return false;
  uint8_t idm[8] = {0};
  uint8_t pmm[8] = {0};
  uint16_t systemCode = 0;
  int8_t result = nfc_->felica_Polling(0xFFFF, 0x01, idm, pmm, &systemCode, timeoutMs);
  if (result <= 0) return false;

  info.found = true;
  info.kind = NfcTagKind::Felica;
  info.uidLength = 8;
  info.uid = uidToString(idm, 8);
  info.technology = "NFC-F / FeliCa";
  info.product = String("FeliCa system 0x") + String(systemCode, HEX);
  info.type = "NFC-F / FeliCa";
  info.protectedData = true;
  info.writable = false;
  memcpy(lastUid_, idm, 8);
  lastUidLength_ = 8;
  return true;
}

NfcTagInfo NfcService::scan(uint16_t timeoutMs) {
  NfcTagInfo info;
  lastTag_ = info;
  lastUidLength_ = 0;
  memset(lastUid_, 0, sizeof(lastUid_));

  if (!ready_ && !begin()) return info;

  uint8_t uid[10] = {0};
  uint8_t uidLength = 0;
  bool ok = nfc_->readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, timeoutMs);
  if (!ok || !uidLength) {
    if (scanFelica(info, timeoutMs)) {
      lastTag_ = info;
      return info;
    }
    return info;
  }

  info.found = true;
  info.uidLength = uidLength;
  info.uid = uidToString(uid, uidLength);
  info.technology = "NFC-A / ISO14443A";
  memcpy(lastUid_, uid, min<size_t>(uidLength, sizeof(lastUid_)));
  lastUidLength_ = uidLength;

  if (!fillType2Info(info)) {
    if (uidLength == 4) {
      info.kind = NfcTagKind::MifareClassicCompatible;
      info.type = "ISO14443A / MIFARE Classic-compatible";
      info.product = "Classic-compatible / protected sectors possible";
      info.protectedData = true;
      info.writable = false;
    } else {
      info.kind = NfcTagKind::Unknown;
      info.type = String("ISO14443A / UID ") + uidLength + " bytes";
      info.product = "ISO14443A tag / secure or unsupported family";
      info.protectedData = true;
      info.writable = false;
    }
  }

  lastTag_ = info;
  return info;
}

bool NfcService::readType2UserMemory(String& hexDump, uint16_t maxBytes) {
  std::vector<uint8_t> data;
  if (!readType2Bytes(data, maxBytes)) return false;
  hexDump = hexPreview(data.data(), data.size(), data.size());
  return true;
}

bool NfcService::writeType2NdefRecord(const std::vector<uint8_t>& record, String& status) {
  if (!nfc_ || lastTag_.kind != NfcTagKind::Type2) {
    status = "Scan an NTAG/Ultralight Type 2 tag first";
    return false;
  }

  uint8_t ccBlock[16] = {0};
  if (!nfc_->mifareultralight_ReadPage(3, ccBlock) || ccBlock[0] != 0xE1) {
    status = "Tag has no readable Type 2 capability container";
    return false;
  }

  const uint16_t capacity = static_cast<uint16_t>(ccBlock[2]) * 8U;
  if ((ccBlock[3] & 0x0F) != 0x00) {
    status = "Tag capability container reports read-only/protected write access";
    return false;
  }

  std::vector<uint8_t> tlv;
  tlv.push_back(0x03);
  if (record.size() < 0xFF) {
    tlv.push_back(static_cast<uint8_t>(record.size()));
  } else {
    tlv.push_back(0xFF);
    tlv.push_back(static_cast<uint8_t>((record.size() >> 8) & 0xFF));
    tlv.push_back(static_cast<uint8_t>(record.size() & 0xFF));
  }
  tlv.insert(tlv.end(), record.begin(), record.end());
  tlv.push_back(0xFE);

  if (tlv.size() > capacity) {
    status = String("NDEF payload too large for tag: ") + tlv.size() + "/" + capacity + " bytes";
    return false;
  }

  while (tlv.size() % 4) tlv.push_back(0x00);
  for (size_t offset = 0; offset < tlv.size(); offset += 4) {
    uint8_t pageData[4] = {
      tlv[offset], tlv[offset + 1], tlv[offset + 2], tlv[offset + 3]
    };
    uint8_t page = static_cast<uint8_t>(4 + (offset / 4));
    if (!nfc_->mifareultralight_WritePage(page, pageData)) {
      status = String("Write failed at user page ") + page + " (tag may be locked/protected)";
      return false;
    }
  }

  status = String("NDEF written: ") + record.size() + " bytes";
  return true;
}

bool NfcService::writeNdefText(const String& text, String& status) {
  if (!text.length()) {
    status = "Text is empty";
    return false;
  }
  if (text.length() > 220) {
    status = "Text too long for safe alpha writer (max 220 bytes)";
    return false;
  }

  const uint8_t langLen = 2;
  const uint32_t payloadLen = 1 + langLen + text.length();
  if (payloadLen > 255) {
    status = "Text record exceeds short-record size";
    return false;
  }

  std::vector<uint8_t> record;
  record.reserve(payloadLen + 4);
  record.push_back(0xD1); // MB | ME | SR | TNF well-known
  record.push_back(0x01);
  record.push_back(static_cast<uint8_t>(payloadLen));
  record.push_back('T');
  record.push_back(langLen); // UTF-8 + language length 2
  record.push_back('e');
  record.push_back('n');
  for (size_t i = 0; i < text.length(); ++i) record.push_back(static_cast<uint8_t>(text[i]));

  return writeType2NdefRecord(record, status);
}

bool NfcService::writeNdefUri(const String& uri, String& status) {
  if (!uri.length()) {
    status = "URI is empty";
    return false;
  }
  if (uri.length() > 230) {
    status = "URI too long for safe alpha writer (max 230 bytes)";
    return false;
  }

  const uint32_t payloadLen = 1 + uri.length();
  if (payloadLen > 255) {
    status = "URI record exceeds short-record size";
    return false;
  }

  std::vector<uint8_t> record;
  record.reserve(payloadLen + 4);
  record.push_back(0xD1);
  record.push_back(0x01);
  record.push_back(static_cast<uint8_t>(payloadLen));
  record.push_back('U');
  record.push_back(0x00); // no prefix compression; preserve exact URI
  for (size_t i = 0; i < uri.length(); ++i) record.push_back(static_cast<uint8_t>(uri[i]));

  return writeType2NdefRecord(record, status);
}

String NfcService::snapshotText() const {
  if (!lastTag_.found) return "No NFC tag scanned";
  String out;
  out.reserve(700);
  out += "UID: " + lastTag_.uid + "\n";
  out += "Technology: " + lastTag_.technology + "\n";
  out += "Type: " + lastTag_.type + "\n";
  if (lastTag_.product.length()) out += "Product: " + lastTag_.product + "\n";
  if (lastTag_.capacityBytes) out += "User memory: " + String(lastTag_.capacityBytes) + " bytes\n";
  out += String("Writable: ") + (lastTag_.writable ? "yes" : "no/unknown") + "\n";
  if (lastTag_.ndefPresent) {
    out += "NDEF: " + (lastTag_.ndefType.length() ? lastTag_.ndefType : String("present")) + "\n";
    if (lastTag_.ndefPayload.length()) out += "Payload: " + lastTag_.ndefPayload + "\n";
  } else {
    out += "NDEF: not found\n";
  }
  if (lastTag_.protectedData) out += "Protected/secure areas: possible; no automatic key cracking or credential cloning\n";
  if (lastTag_.memoryPreview.length()) out += "Memory preview: " + lastTag_.memoryPreview + "\n";
  return out;
}

}

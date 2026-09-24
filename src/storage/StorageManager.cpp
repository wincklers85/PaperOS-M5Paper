#include "StorageManager.h"
#include "PaperOS.h"
#include <SdFat.h>

namespace paperos {
static constexpr int PIN_SD_CS = 4;
static constexpr int PIN_SD_SCK = 14;
static constexpr int PIN_SD_MISO = 13;
static constexpr int PIN_SD_MOSI = 12;

bool StorageManager::begin() {
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  mounted_ = SD.begin(PIN_SD_CS, SPI, 25000000);
  if (mounted_) ensureLayout();
  return mounted_;
}

void StorageManager::ensureLayout() {
  const char* dirs[] = {"/PaperOS","/PaperOS/Documents","/PaperOS/Books","/PaperOS/PDF","/PaperOS/Notes","/PaperOS/Images","/PaperOS/Downloads","/PaperOS/Logs","/PaperOS/Backup","/PaperOS/Config","/PaperOS/Labs","/PaperOS/Labs/HID","/PaperOS/Labs/Scripts"};
  for (auto d : dirs) if (!SD.exists(d)) SD.mkdir(d);
}

bool StorageManager::validPath(const String& path) const {
  return path == "/PaperOS" || path.startsWith("/PaperOS/");
}

uint64_t StorageManager::totalBytes() const { return mounted_ ? SD.totalBytes() : 0; }
uint64_t StorageManager::usedBytes() const { return mounted_ ? SD.usedBytes() : 0; }
uint64_t StorageManager::freeBytes() const { auto t=totalBytes(), u=usedBytes(); return t > u ? t-u : 0; }

String StorageManager::listJson(const String& path) {
  DynamicJsonDocument doc(8192);
  JsonArray arr = doc.createNestedArray("items");
  doc["path"] = path;
  if (!mounted_ || !validPath(path)) { doc["ok"] = false; String out; serializeJson(doc,out); return out; }
  File root = SD.open(path);
  if (!root || !root.isDirectory()) { doc["ok"] = false; String out; serializeJson(doc,out); return out; }
  doc["ok"] = true;
  for (File f = root.openNextFile(); f; f = root.openNextFile()) {
    JsonObject o = arr.createNestedObject();
    o["name"] = String(f.name());
    o["directory"] = f.isDirectory();
    o["size"] = static_cast<uint64_t>(f.size());
    f.close();
  }
  root.close();
  String out; serializeJson(doc,out); return out;
}

bool StorageManager::makeDir(const String& path) {
  return mounted_ && validPath(path) && (SD.exists(path) || SD.mkdir(path));
}

bool StorageManager::removePath(const String& path) {
  if (!mounted_ || !validPath(path) || path == "/PaperOS") return false;
  File f = SD.open(path);
  if (!f) return false;
  bool dir = f.isDirectory(); f.close();
  return dir ? SD.rmdir(path) : SD.remove(path);
}

bool StorageManager::renamePath(const String& from, const String& to) {
  return mounted_ && validPath(from) && validPath(to) && SD.rename(from, to);
}

bool StorageManager::copyFile(const String& from, const String& to) {
  if (!mounted_ || !validPath(from) || !validPath(to)) return false;
  File src = SD.open(from, FILE_READ);
  if (!src || src.isDirectory()) { if (src) src.close(); return false; }
  File dst = SD.open(to, FILE_WRITE);
  if (!dst) { src.close(); return false; }
  uint8_t buf[1024];
  while (src.available()) { size_t n = src.read(buf, sizeof(buf)); if (dst.write(buf,n) != n) { src.close(); dst.close(); return false; } }
  src.close(); dst.close(); return true;
}
bool StorageManager::formatCard(const String& typeRaw) {
  String type = typeRaw;
  type.toLowerCase();

  SD.end();
  mounted_ = false;
  delay(80);

  bool ok = false;
  {
    SdCardFactory factory;
    SdCard* card = factory.newCard(SdSpiConfig(PIN_SD_CS, SHARED_SPI, SD_SCK_MHZ(16), &SPI));
    if (!card || card->errorCode()) {
      mounted_ = SD.begin(PIN_SD_CS, SPI, 25000000);
      return false;
    }

    alignas(4) uint8_t sectorBuffer[512];
    if (type == "fat" || type == "fat32") {
      FatFormatter formatter;
      ok = formatter.format(card, sectorBuffer, nullptr);
    } else if (type == "exfat") {
      ExFatFormatter formatter;
      ok = formatter.format(card, sectorBuffer, nullptr);
    } else {
      FsFormatter formatter;
      ok = formatter.format(card, sectorBuffer, nullptr);
    }
  }

  delay(120);
  mounted_ = SD.begin(PIN_SD_CS, SPI, 25000000);
  if (mounted_) ensureLayout();
  return ok && mounted_;
}

String StorageManager::filesystemHint() const {
  if (!mounted_) return "Not mounted";
  uint64_t mb = totalBytes() / 1048576ULL;
  if (mb > 32768ULL) return "Likely exFAT / large SDXC";
  return "FAT-compatible volume";
}

}

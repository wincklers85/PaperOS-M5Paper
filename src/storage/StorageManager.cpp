#include "StorageManager.h"
#include "PaperOS.h"
#include <SdFat.h>
#include <algorithm>
#include <cstring>
#include <vector>

namespace paperos {
static constexpr int PIN_SD_CS = 4;
static constexpr int PIN_SD_SCK = 14;
static constexpr int PIN_SD_MISO = 13;
static constexpr int PIN_SD_MOSI = 12;

bool StorageManager::begin() {
  if (mounted_) return true;
  SPI.begin(PIN_SD_SCK, PIN_SD_MISO, PIN_SD_MOSI, PIN_SD_CS);
  mounted_ = SD.begin(PIN_SD_CS, SPI, 25000000);
  return mounted_;
}

bool StorageManager::mount() {
  if (mounted_) return true;
  // Mount must remain metadata-neutral; do not create folders on a card just
  // reinserted for browsing or recovery.
  return begin();
}

void StorageManager::unmount() {
  recoveryScanning_ = false;
  recoveredFiles_.clear();
  recoveryVisitedDirs_.clear();
  recoveryCursorFile_ = static_cast<size_t>(-1);
  recoveryStatus_ = "SD unmounted";
  if (mounted_) SD.end();
  mounted_ = false;
}

void StorageManager::ensureLayout() {
  const char* dirs[] = {"/PaperOS","/PaperOS/Documents","/PaperOS/Books","/PaperOS/PDF","/PaperOS/Notes","/PaperOS/Images","/PaperOS/Downloads","/PaperOS/Logs","/PaperOS/Backup","/PaperOS/Config","/PaperOS/Labs","/PaperOS/Labs/HID","/PaperOS/Labs/Scripts"};
  for (auto d : dirs) if (!SD.exists(d)) SD.mkdir(d);
}

bool StorageManager::validPath(const String& path) const {
  if (!path.length() || path[0] != '/') return false;
  if (path.indexOf("..") >= 0 || path.indexOf("//") >= 0) return false;
  return path.length() <= 240;
}

bool StorageManager::exists(const String& path) const {
  return mounted_ && validPath(path) && SD.exists(path);
}

bool StorageManager::isDirectory(const String& path) const {
  if (!mounted_ || !validPath(path)) return false;
  File f = SD.open(path);
  if (!f) return false;
  bool d = f.isDirectory();
  f.close();
  return d;
}

String StorageManager::uniqueDestination(const String& directory, const String& name) const {
  String base = directory;
  if (!base.endsWith("/")) base += "/";
  String candidate = base + name;
  if (!SD.exists(candidate)) return candidate;

  int dot = name.lastIndexOf('.');
  String stem = dot > 0 ? name.substring(0, dot) : name;
  String ext = dot > 0 ? name.substring(dot) : String();
  for (int i = 1; i < 1000; ++i) {
    candidate = base + stem + " (" + String(i) + ")" + ext;
    if (!SD.exists(candidate)) return candidate;
  }
  return base + stem + " copy" + ext;
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
  if (recoveryScanning_) return false;
  return mounted_ && validPath(path) && (SD.exists(path) || SD.mkdir(path));
}

bool StorageManager::removePath(const String& path) {
  if (recoveryScanning_) return false;
  if (!mounted_ || !validPath(path) || path == "/" || path == "/PaperOS") return false;
  File f = SD.open(path);
  if (!f) return false;
  bool dir = f.isDirectory();
  f.close();
  if (!dir) return SD.remove(path);

  File d = SD.open(path);
  if (!d) return false;
  std::vector<String> children;
  for (File child = d.openNextFile(); child; child = d.openNextFile()) {
    String n = String(child.name());
    child.close();
    if (!n.startsWith("/")) n = path + (path.endsWith("/") ? "" : "/") + n;
    children.push_back(n);
  }
  d.close();
  for (const auto& childPath : children) {
    if (!removePath(childPath)) return false;
  }
  return SD.rmdir(path);
}

bool StorageManager::clearPaperOSContents() {
  if (recoveryScanning_) return false;
  if (!mounted_) return false;
  if (!SD.exists("/PaperOS")) return true;
  File root = SD.open("/PaperOS");
  if (!root || !root.isDirectory()) { if (root) root.close(); return false; }
  std::vector<String> children;
  for (File child = root.openNextFile(); child; child = root.openNextFile()) {
    String name = String(child.name());
    child.close();
    if (!name.startsWith("/")) name = String("/PaperOS/") + name;
    children.push_back(name);
  }
  root.close();
  for (const auto& path : children) if (!removePath(path)) return false;
  return true;
}

bool StorageManager::renamePath(const String& from, const String& to) {
  if (recoveryScanning_) return false;
  return mounted_ && validPath(from) && validPath(to) && SD.rename(from, to);
}

bool StorageManager::copyFile(const String& from, const String& to) {
  if (recoveryScanning_) return false;
  if (!mounted_ || !validPath(from) || !validPath(to) || from == to) return false;
  if (SD.exists(to)) return false;
  File src = SD.open(from, FILE_READ);
  if (!src || src.isDirectory()) { if (src) src.close(); return false; }
  File dst = SD.open(to, FILE_WRITE);
  if (!dst) { src.close(); return false; }
  uint8_t buf[1024];
  while (src.available()) { size_t n = src.read(buf, sizeof(buf)); if (dst.write(buf,n) != n) { src.close(); dst.close(); return false; } }
  src.close(); dst.close(); return true;
}

bool StorageManager::copyPath(const String& from, const String& to) {
  if (recoveryScanning_) return false;
  if (!mounted_ || !validPath(from) || !validPath(to) || from == "/" || from == to) return false;
  if (to.startsWith(from + "/")) return false;
  File src = SD.open(from);
  if (!src) return false;

  if (!src.isDirectory()) {
    src.close();
    return copyFile(from, to);
  }

  src.close();
  if (!SD.exists(to) && !SD.mkdir(to)) return false;

  File dir = SD.open(from);
  if (!dir || !dir.isDirectory()) { if (dir) dir.close(); return false; }

  bool ok = true;
  for (File child = dir.openNextFile(); child; child = dir.openNextFile()) {
    String childName = String(child.name());
    int slash = childName.lastIndexOf('/');
    if (slash >= 0) childName = childName.substring(slash + 1);
    bool childDir = child.isDirectory();
    child.close();

    String childFrom = from + (from.endsWith("/") ? "" : "/") + childName;
    String childTo = to + (to.endsWith("/") ? "" : "/") + childName;
    if (childDir) {
      if (!copyPath(childFrom, childTo)) { ok = false; break; }
    } else if (!copyFile(childFrom, childTo)) {
      ok = false; break;
    }
  }
  dir.close();
  return ok;
}

bool StorageManager::movePath(const String& from, const String& to) {
  if (recoveryScanning_) return false;
  if (!mounted_ || !validPath(from) || !validPath(to) || from == "/" || from == to) return false;
  if (to.startsWith(from + "/")) return false;
  if (SD.exists(to)) return false;
  if (SD.rename(from, to)) return true;
  if (!copyPath(from, to)) return false;
  return removePath(from);
}

bool StorageManager::formatCard(const String& typeRaw) {
  if (recoveryScanning_) return false;
  recoveredFiles_.clear();
  recoveryCursorFile_ = static_cast<size_t>(-1);
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

const RecoveredSdFile* StorageManager::recoveredFile(size_t index) const {
  return index < recoveredFiles_.size() ? &recoveredFiles_[index] : nullptr;
}

bool StorageManager::readSector(uint32_t sector, uint8_t* buffer) {
  return mounted_ && buffer && SD.readRAW(buffer, sector);
}

bool StorageManager::parseRecoveryVolume() {
  uint8_t sector[512];
  if (!readSector(0, sector)) { recoveryStatus_ = "Cannot read SD sectors"; return false; }

  auto isFat32Boot = [](const uint8_t* b) {
    return b[510] == 0x55 && b[511] == 0xAA &&
      (memcmp(b + 82, "FAT32", 5) == 0 || (b[11] == 0x00 && b[12] == 0x02));
  };

  uint32_t start = 0;
  if (!isFat32Boot(sector)) {
    bool found = false;
    for (uint8_t i = 0; i < 4; ++i) {
      const uint8_t* p = sector + 446 + i * 16;
      const uint8_t type = p[4];
      if (type == 0x0B || type == 0x0C || type == 0x1B || type == 0x1C) {
        start = (uint32_t)p[8] | ((uint32_t)p[9] << 8) | ((uint32_t)p[10] << 16) | ((uint32_t)p[11] << 24);
        if (readSector(start, sector) && isFat32Boot(sector)) { found = true; break; }
      }
    }
    if (!found) { recoveryStatus_ = "FAT32 required; exFAT and other formats are not supported yet"; return false; }
  }

  const uint16_t bytesPerSector = (uint16_t)sector[11] | ((uint16_t)sector[12] << 8);
  const uint8_t spc = sector[13];
  const uint16_t reserved = (uint16_t)sector[14] | ((uint16_t)sector[15] << 8);
  const uint8_t fats = sector[16];
  const uint32_t fatSectors = (uint32_t)sector[36] | ((uint32_t)sector[37] << 8) |
                              ((uint32_t)sector[38] << 16) | ((uint32_t)sector[39] << 24);
  const uint32_t totalSectors = (uint32_t)sector[32] | ((uint32_t)sector[33] << 8) |
                                ((uint32_t)sector[34] << 16) | ((uint32_t)sector[35] << 24);
  const uint32_t rootCluster = (uint32_t)sector[44] | ((uint32_t)sector[45] << 8) |
                               ((uint32_t)sector[46] << 16) | ((uint32_t)sector[47] << 24);
  if (bytesPerSector != 512 || spc == 0 || spc > 128 || reserved == 0 || fats == 0 || fats > 2 || fatSectors == 0 || totalSectors == 0 ||
      (uint64_t)reserved + (uint64_t)fats * fatSectors >= totalSectors) {
    recoveryStatus_ = "Invalid or unsupported FAT32 geometry";
    return false;
  }
  recoveryVolumeStart_ = start;
  recoveryFatStart_ = start + reserved;
  recoveryFatSectors_ = fatSectors;
  recoveryDataStart_ = recoveryFatStart_ + (uint32_t)fats * fatSectors;
  recoverySectorsPerCluster_ = spc;
  if (recoveryDataStart_ < start || recoveryDataStart_ - start >= totalSectors) {
    recoveryStatus_ = "Invalid FAT32 data region";
    return false;
  }
  recoveryDataClusters_ = (totalSectors - (recoveryDataStart_ - start)) / spc;
  if (rootCluster < 2 || rootCluster >= recoveryDataClusters_ + 2) {
    recoveryStatus_ = "Invalid FAT32 root directory";
    return false;
  }
  recoveryVisitedDirs_.clear();
  scanDirectory(rootCluster, 0);
  return true;
}

uint32_t StorageManager::fatEntry(uint32_t cluster) {
  uint8_t sector[512];
  const uint32_t byteOffset = cluster * 4U;
  if (byteOffset / 512U >= recoveryFatSectors_ || !readSector(recoveryFatStart_ + byteOffset / 512U, sector)) return 0;
  const uint32_t offset = byteOffset % 512U;
  return ((uint32_t)sector[offset] | ((uint32_t)sector[offset + 1] << 8) |
          ((uint32_t)sector[offset + 2] << 16) | ((uint32_t)sector[offset + 3] << 24)) & 0x0FFFFFFF;
}

void StorageManager::scanDirectory(uint32_t firstCluster, uint8_t depth) {
  if (depth > 6 || firstCluster < 2 || firstCluster >= recoveryDataClusters_ + 2 ||
      recoveryVisitedDirs_.size() >= 128 ||
      std::find(recoveryVisitedDirs_.begin(), recoveryVisitedDirs_.end(), firstCluster) != recoveryVisitedDirs_.end()) return;
  recoveryVisitedDirs_.push_back(firstCluster);

  uint32_t cluster = firstCluster;
  for (uint32_t chain = 0; chain < recoveryDataClusters_ && chain < 4096; ++chain) {
    uint8_t sector[512];
    const uint32_t lba = recoveryDataStart_ + (cluster - 2) * recoverySectorsPerCluster_;
    bool endOfDirectory = false;
    for (uint8_t s = 0; s < recoverySectorsPerCluster_ && !endOfDirectory; ++s) {
      if (!readSector(lba + s, sector)) return;
      for (uint16_t off = 0; off < 512; off += 32) {
        const uint8_t* e = sector + off;
        if (e[0] == 0x00) { endOfDirectory = true; break; }
        const bool deleted = e[0] == 0xE5;
        const uint8_t attr = e[11];
        if (attr == 0x0F || (attr & 0x08)) continue;
        const uint32_t childCluster = ((uint32_t)e[20] << 24) | ((uint32_t)e[21] << 16) |
                                      ((uint32_t)e[26] << 8) | e[27];
        if (deleted && !(attr & 0x10)) {
          const uint32_t size = (uint32_t)e[28] | ((uint32_t)e[29] << 8) | ((uint32_t)e[30] << 16) | ((uint32_t)e[31] << 24);
          if (size && size <= 64U * 1024U * 1024U && childCluster >= 2 &&
              childCluster < recoveryDataClusters_ + 2 && recoveredFiles_.size() < 64) {
            bool duplicate = false;
            for (const auto& f : recoveredFiles_) if (f.firstCluster == childCluster && f.size == size) { duplicate = true; break; }
            if (!duplicate) {
              char name[32];
              snprintf(name, sizeof(name), "RECOVERED_%03u", (unsigned)recoveredFiles_.size() + 1);
              String filename(name);
              char ext[4] = {0};
              for (uint8_t i = 0; i < 3 && e[8 + i] != ' '; ++i) ext[i] = (char)e[8 + i];
              if (ext[0]) filename += String(".") + ext;
              const uint32_t next = fatEntry(childCluster);
              RecoveredSdFile recovered;
              recovered.name = filename;
              recovered.firstCluster = childCluster;
              recovered.size = size;
              recovered.fatChainAvailable = next >= 2 && next < recoveryDataClusters_ + 2;
              recoveredFiles_.push_back(recovered);
            }
          }
        } else if (!deleted && (attr & 0x10) && e[0] != '.' && childCluster >= 2) {
          scanDirectory(childCluster, depth + 1);
        }
      }
    }
    if (endOfDirectory) break;
    const uint32_t next = fatEntry(cluster);
    if (next < 2 || next >= 0x0FFFFFF8 || next >= recoveryDataClusters_ + 2) break;
    cluster = next;
  }
}

bool StorageManager::scanDeletedFiles() {
  return startRecoveryScan();
}

bool StorageManager::startRecoveryScan() {
  if (!mounted_) { recoveryStatus_ = "Insert and mount an SD card first"; return false; }
  if (recoveryScanning_) return true;

  recoveredFiles_.clear();
  recoveryCursorFile_ = static_cast<size_t>(-1);
  recoveryProgress_ = 0;
  recoveryProgressMark_ = 0;
  recoveryScanLba_ = 0;
  recoveryFatEntriesScanned_ = parseRecoveryVolume();
  recoveryFatCandidateCount_ = recoveredFiles_.size();
  resetCarver();

  const uint64_t sectors = SD.cardSize() / 512ULL;
  if (!sectors) { recoveryStatus_ = "SD card reported zero readable sectors"; return false; }
  recoveryTotalSectors_ = static_cast<uint32_t>(min<uint64_t>(sectors, 0xFFFFFFFFULL));
  recoveryScanning_ = true;
  recoveryStatus_ = recoveryFatEntriesScanned_
    ? "Starting read-only raw sector scan; FAT candidates retained"
    : "Starting read-only raw sector scan; no supported FAT directory found";
  return true;
}

void StorageManager::cancelRecoveryScan() {
  if (!recoveryScanning_) return;
  recoveryScanning_ = false;
  resetCarver();
  recoveryStatus_ = String("Scan stopped at ") + recoveryProgress_ + "% — " + recoveredFiles_.size() + " candidate(s) kept";
}

void StorageManager::resetCarver() {
  recoveryCarveMode_ = CarveMode::None;
  recoveryCarveStart_ = 0;
  recoveryCarveLength_ = 0;
  recoveryCarveExpected_ = 0;
  recoveryCarveHeaderCount_ = 0;
  recoveryTextNewlines_ = 0;
  recoveryTailCount_ = 0;
  memset(recoveryTail_, 0, sizeof(recoveryTail_));
}

void StorageManager::finishRawCandidate(uint32_t size, const char* extension) {
  bool freeClusterVerified = false;
  if (recoveryFatEntriesScanned_) {
    const uint64_t lba = recoveryCarveStart_ / 512ULL;
    const uint64_t dataEnd = (uint64_t)recoveryDataStart_ + (uint64_t)recoveryDataClusters_ * recoverySectorsPerCluster_;
    if (lba < recoveryDataStart_ || lba >= dataEnd) { resetCarver(); return; }
    const uint32_t cluster = static_cast<uint32_t>((lba - recoveryDataStart_) / recoverySectorsPerCluster_) + 2;
    if (fatEntry(cluster) != 0) { resetCarver(); return; }
    for (size_t i = 0; i < recoveryFatCandidateCount_; ++i) {
      if (i < recoveredFiles_.size() && !recoveredFiles_[i].rawCarved && recoveredFiles_[i].firstCluster == cluster) {
        resetCarver(); return;
      }
    }
    freeClusterVerified = true;
  }
  if (size >= 32 && recoveredFiles_.size() < 128) {
    char name[32];
    const size_t serial = recoveredFiles_.size() - recoveryFatCandidateCount_ + 1;
    snprintf(name, sizeof(name), "CARVED_%03u.%s", static_cast<unsigned>(serial), extension);
    RecoveredSdFile file;
    file.name = name;
    file.size = size;
    file.rawCarved = true;
    file.rawDeletedSpaceVerified = freeClusterVerified;
    file.rawStartByte = recoveryCarveStart_;
    recoveredFiles_.push_back(file);
  }
  resetCarver();
}

void StorageManager::processRecoveryByte(uint8_t value, uint64_t absoluteOffset) {
  auto tailEndsWith = [this, value](const uint8_t* pattern, uint8_t length) {
    if (recoveryTailCount_ + 1 < length) return false;
    const uint8_t oldNeeded = length - 1;
    for (uint8_t i = 0; i < oldNeeded; ++i) {
      const int tailIndex = static_cast<int>(recoveryTailCount_) - oldNeeded + i;
      if (tailIndex < 0 || recoveryTail_[tailIndex] != pattern[i]) return false;
    }
    return value == pattern[length - 1];
  };
  static const uint8_t pngSignature[8] = {0x89,0x50,0x4E,0x47,0x0D,0x0A,0x1A,0x0A};
  static const uint8_t pngEnd[12] = {0x00,0x00,0x00,0x00,'I','E','N','D',0xAE,0x42,0x60,0x82};
  static const uint8_t pdfStart[5] = {'%','P','D','F','-'};
  static const uint8_t pdfEnd[5] = {'%','%','E','O','F'};
  auto pushTail = [this, value]() {
    if (recoveryTailCount_ < sizeof(recoveryTail_)) recoveryTail_[recoveryTailCount_++] = value;
    else { memmove(recoveryTail_, recoveryTail_ + 1, sizeof(recoveryTail_) - 1); recoveryTail_[sizeof(recoveryTail_) - 1] = value; }
  };

  // Printable runs can begin at the '%' in PDF or the "BM" in a bitmap.
  // Give file signatures precedence over the text-run heuristic.
  if (recoveryCarveMode_ == CarveMode::Text && tailEndsWith(pdfStart, sizeof(pdfStart))) {
    if (recoveryCarveLength_ >= 128 && recoveryTextNewlines_ >= 2) finishRawCandidate(recoveryCarveLength_, "TXT");
    else resetCarver();
    recoveryCarveMode_ = CarveMode::Pdf; recoveryCarveStart_ = absoluteOffset - 4; recoveryCarveLength_ = 5;
    pushTail();
    return;
  }
  if (recoveryCarveMode_ == CarveMode::Text && recoveryTailCount_ && recoveryTail_[recoveryTailCount_ - 1] == 'B' && value == 'M') {
    if (recoveryCarveLength_ >= 128 && recoveryTextNewlines_ >= 2) finishRawCandidate(recoveryCarveLength_, "TXT");
    else resetCarver();
    recoveryCarveMode_ = CarveMode::Bmp; recoveryCarveStart_ = absoluteOffset - 1; recoveryCarveLength_ = 2; recoveryCarveHeaderCount_ = 0;
    pushTail();
    return;
  }

  if (recoveryCarveMode_ == CarveMode::Jpeg) {
    ++recoveryCarveLength_;
    const bool ended = recoveryTailCount_ && recoveryTail_[recoveryTailCount_ - 1] == 0xFF && value == 0xD9;
    if (ended && recoveryCarveLength_ <= 32U * 1024U * 1024U) finishRawCandidate(recoveryCarveLength_, "JPG");
    else if (recoveryCarveLength_ > 32U * 1024U * 1024U) resetCarver();
  } else if (recoveryCarveMode_ == CarveMode::Png) {
    ++recoveryCarveLength_;
    if (tailEndsWith(pngEnd, sizeof(pngEnd))) finishRawCandidate(recoveryCarveLength_, "PNG");
    else if (recoveryCarveLength_ > 32U * 1024U * 1024U) resetCarver();
  } else if (recoveryCarveMode_ == CarveMode::Pdf) {
    ++recoveryCarveLength_;
    if (tailEndsWith(pdfEnd, sizeof(pdfEnd))) finishRawCandidate(recoveryCarveLength_, "PDF");
    else if (recoveryCarveLength_ > 32U * 1024U * 1024U) resetCarver();
  } else if (recoveryCarveMode_ == CarveMode::Bmp) {
    ++recoveryCarveLength_;
    if (recoveryCarveHeaderCount_ < sizeof(recoveryCarveHeader_)) {
      recoveryCarveHeader_[recoveryCarveHeaderCount_++] = value;
      if (recoveryCarveHeaderCount_ == 6) {
        recoveryCarveExpected_ = (uint32_t)recoveryCarveHeader_[0] |
          ((uint32_t)recoveryCarveHeader_[1] << 8) | ((uint32_t)recoveryCarveHeader_[2] << 16) |
          ((uint32_t)recoveryCarveHeader_[3] << 24);
        if (recoveryCarveExpected_ < 54 || recoveryCarveExpected_ > 32U * 1024U * 1024U) resetCarver();
      }
    }
    if (recoveryCarveMode_ == CarveMode::Bmp && recoveryCarveExpected_ && recoveryCarveLength_ >= recoveryCarveExpected_)
      finishRawCandidate(recoveryCarveExpected_, "BMP");
  } else if (recoveryCarveMode_ == CarveMode::Text) {
    const bool printable = value == '\n' || value == '\r' || value == '\t' || (value >= 32 && value <= 126);
    if (!printable) {
      if (recoveryCarveLength_ >= 128 && recoveryTextNewlines_ >= 2)
        finishRawCandidate(recoveryCarveLength_, "TXT");
      else resetCarver();
    } else {
      ++recoveryCarveLength_;
      if (value == '\n') ++recoveryTextNewlines_;
      if (recoveryCarveLength_ >= 8192) {
        if (recoveryTextNewlines_ >= 2) finishRawCandidate(recoveryCarveLength_, "TXT");
        else resetCarver();
      }
    }
  } else {
    bool started = false;
    if (recoveryTailCount_ && recoveryTail_[recoveryTailCount_ - 1] == 0xFF && value == 0xD8) {
      recoveryCarveMode_ = CarveMode::Jpeg; recoveryCarveStart_ = absoluteOffset - 1; recoveryCarveLength_ = 2; started = true;
    } else if (tailEndsWith(pngSignature, sizeof(pngSignature))) {
      recoveryCarveMode_ = CarveMode::Png; recoveryCarveStart_ = absoluteOffset - 7; recoveryCarveLength_ = 8; started = true;
    } else if (tailEndsWith(pdfStart, sizeof(pdfStart))) {
      recoveryCarveMode_ = CarveMode::Pdf; recoveryCarveStart_ = absoluteOffset - 4; recoveryCarveLength_ = 5; started = true;
    } else if (recoveryTailCount_ && recoveryTail_[recoveryTailCount_ - 1] == 'B' && value == 'M') {
      recoveryCarveMode_ = CarveMode::Bmp; recoveryCarveStart_ = absoluteOffset - 1; recoveryCarveLength_ = 2; recoveryCarveHeaderCount_ = 0; started = true;
    }
    if (!started && (value == '\n' || value == '\r' || value == '\t' || (value >= 32 && value <= 126))) {
      recoveryCarveMode_ = CarveMode::Text; recoveryCarveStart_ = absoluteOffset; recoveryCarveLength_ = 1;
      recoveryTextNewlines_ = value == '\n' ? 1 : 0;
    }
  }

  pushTail();
}

void StorageManager::processRecoverySector(uint32_t lba, const uint8_t* sector) {
  const uint64_t base = (uint64_t)lba * 512ULL;
  for (uint16_t i = 0; i < 512; ++i) processRecoveryByte(sector[i], base + i);
}

void StorageManager::loop() {
  if (!recoveryScanning_) return;
  if (!mounted_) { recoveryScanning_ = false; recoveryStatus_ = "SD unmounted; scan stopped"; return; }
  uint8_t sector[512];
  // Bound each pass so Wi-Fi, touch and the web server keep responding.
  for (uint8_t n = 0; n < 24 && recoveryScanLba_ < recoveryTotalSectors_; ++n, ++recoveryScanLba_) {
    if (!readSector(recoveryScanLba_, sector)) {
      recoveryScanning_ = false;
      resetCarver();
      recoveryStatus_ = String("Sector read failed at LBA ") + recoveryScanLba_ + "; partial candidates kept";
      return;
    }
    processRecoverySector(recoveryScanLba_, sector);
  }
  const uint8_t progress = recoveryTotalSectors_ ? static_cast<uint8_t>((uint64_t)recoveryScanLba_ * 100ULL / recoveryTotalSectors_) : 100;
  if (progress != recoveryProgressMark_) {
    recoveryProgressMark_ = progress;
    recoveryProgress_ = progress;
    recoveryStatus_ = String("Raw scan ") + progress + "% — sector " + recoveryScanLba_ + " / " + recoveryTotalSectors_ + " — " + recoveredFiles_.size() + " candidate(s)";
  }
  if (recoveryScanLba_ >= recoveryTotalSectors_) {
    if (recoveryCarveMode_ == CarveMode::Text && recoveryCarveLength_ >= 128 && recoveryTextNewlines_ >= 2)
      finishRawCandidate(recoveryCarveLength_, "TXT");
    else resetCarver();
    recoveryScanning_ = false;
    recoveryProgress_ = 100;
    recoveryStatus_ = String("Sector scan complete — ") + recoveredFiles_.size() + " candidate(s); raw files assume contiguous data and may be incomplete";
  }
}

size_t StorageManager::readRecoveredFile(size_t index, uint32_t offset, uint8_t* output, size_t length) {
  const RecoveredSdFile* file = recoveredFile(index);
  if (!file || !output || offset >= file->size || !mounted_) return 0;
  size_t wanted = min(length, (size_t)(file->size - offset));
  if (file->rawCarved) {
    size_t written = 0;
    uint8_t sector[512];
    uint64_t absolute = file->rawStartByte + offset;
    while (written < wanted) {
      uint32_t lba = static_cast<uint32_t>(absolute / 512ULL);
      size_t inSector = static_cast<size_t>(absolute % 512ULL);
      if (!readSector(lba, sector)) break;
      size_t count = min(wanted - written, static_cast<size_t>(512 - inSector));
      memcpy(output + written, sector + inSector, count);
      written += count;
      absolute += count;
    }
    return written;
  }
  size_t written = 0;
  const uint32_t clusterBytes = (uint32_t)recoverySectorsPerCluster_ * 512U;
  uint32_t cluster = file->firstCluster;
  uint32_t targetCluster = offset / clusterBytes;
  const bool sequential = recoveryCursorFile_ == index && recoveryCursorOffset_ == offset;
  const bool contiguous = fatEntry(file->firstCluster) == 0;
  if (sequential) cluster = recoveryCursorCluster_;
  else if (contiguous) cluster += targetCluster;
  else for (uint32_t i = 0; i < targetCluster; ++i) {
    uint32_t next = fatEntry(cluster);
    cluster = (next >= 2 && next < recoveryDataClusters_ + 2) ? next : cluster + 1;
  }
  uint32_t inCluster = offset % clusterBytes;
  uint8_t sector[512];
  while (written < wanted) {
    if (cluster < 2 || cluster >= recoveryDataClusters_ + 2) break;
    uint32_t sectorInCluster = inCluster / 512U;
    uint32_t inSector = inCluster % 512U;
    uint32_t lba = recoveryDataStart_ + (cluster - 2) * recoverySectorsPerCluster_ + sectorInCluster;
    if (!readSector(lba, sector)) break;
    size_t count = min(wanted - written, (size_t)(512U - inSector));
    memcpy(output + written, sector + inSector, count);
    written += count;
    inCluster += count;
    if (inCluster >= clusterBytes && written < wanted) {
      inCluster = 0;
      uint32_t next = fatEntry(cluster);
      cluster = (next >= 2 && next < recoveryDataClusters_ + 2) ? next : cluster + 1;
    }
  }
  if (inCluster >= clusterBytes) {
    uint32_t next = contiguous ? 0 : fatEntry(cluster);
    cluster = (next >= 2 && next < recoveryDataClusters_ + 2) ? next : cluster + 1;
  }
  recoveryCursorFile_ = index;
  recoveryCursorOffset_ = offset + written;
  recoveryCursorCluster_ = cluster;
  return written;
}

bool StorageManager::saveRecoveredFileToSource(size_t index, bool confirmed, String& destination, String& status) {
  static constexpr uint32_t kMaxSameCardRestoreBytes = 1024U * 1024U;
  const RecoveredSdFile* file = recoveredFile(index);
  if (!confirmed) { status = "Explicit same-card write confirmation required"; return false; }
  if (recoveryScanning_) { status = "Stop the sector scan before writing to the SD"; return false; }
  if (!mounted_ || !file) { status = "SD card or recovery candidate is unavailable"; return false; }
  if (!file->size || file->size > kMaxSameCardRestoreBytes) {
    status = "Same-card restore is limited to files up to 1 MiB; export to another device instead";
    return false;
  }

  // Read the entire candidate before any filesystem write can reuse its source clusters.
  uint8_t* staged = static_cast<uint8_t*>(ps_malloc(file->size));
  if (!staged) { status = "Not enough PSRAM; export to another device instead"; return false; }
  size_t read = readRecoveredFile(index, 0, staged, file->size);
  if (read != file->size) {
    free(staged);
    status = "Could not read the complete candidate; SD was not written";
    return false;
  }

  if (!SD.exists("/PaperOS") && !SD.mkdir("/PaperOS")) {
    free(staged);
    status = "Could not create /PaperOS; SD write failed";
    return false;
  }
  if (!SD.exists("/PaperOS/Recovery") && !SD.mkdir("/PaperOS/Recovery")) {
    free(staged);
    status = "Could not create /PaperOS/Recovery; SD write failed";
    return false;
  }
  destination = uniqueDestination("/PaperOS/Recovery", file->name);
  File out = SD.open(destination, FILE_WRITE);
  if (!out) {
    free(staged);
    status = "Could not open the recovery destination on the SD";
    return false;
  }
  size_t written = out.write(staged, file->size);
  out.flush();
  out.close();
  free(staged);
  if (written != file->size) {
    SD.remove(destination);
    destination = "";
    status = "Write was incomplete; partial destination removed where possible";
    return false;
  }
  status = "Saved to the same SD. Other deleted data may have been overwritten.";
  return true;
}

}

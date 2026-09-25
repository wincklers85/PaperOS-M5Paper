#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>
#include <vector>

namespace paperos {
struct RecoveredSdFile {
  String name;
  uint32_t firstCluster = 0;
  uint32_t size = 0;
  bool fatChainAvailable = false;
};

class StorageManager {
 public:
  bool begin();
  bool available() const { return mounted_; }
  uint64_t totalBytes() const;
  uint64_t usedBytes() const;
  uint64_t freeBytes() const;
  String listJson(const String& path);
  bool makeDir(const String& path);
  bool removePath(const String& path);
  bool renamePath(const String& from, const String& to);
  bool copyFile(const String& from, const String& to);
  bool copyPath(const String& from, const String& to);
  bool movePath(const String& from, const String& to);
  bool exists(const String& path) const;
  bool isDirectory(const String& path) const;
  String uniqueDestination(const String& directory, const String& name) const;
  bool formatCard(const String& type);
  String filesystemHint() const;
  bool validPath(const String& path) const;
  bool scanDeletedFiles();
  size_t recoveredFileCount() const { return recoveredFiles_.size(); }
  const RecoveredSdFile* recoveredFile(size_t index) const;
  size_t readRecoveredFile(size_t index, uint32_t offset, uint8_t* buffer, size_t length);
  bool saveRecoveredFileToSource(size_t index, bool confirmed, String& destination, String& status);
  String recoveryStatus() const { return recoveryStatus_; }
  fs::FS& fs() { return SD; }
 private:
  bool mounted_ = false;
  std::vector<RecoveredSdFile> recoveredFiles_;
  String recoveryStatus_;
  uint32_t recoveryVolumeStart_ = 0;
  uint32_t recoveryDataStart_ = 0;
  uint32_t recoveryFatStart_ = 0;
  uint32_t recoveryFatSectors_ = 0;
  uint32_t recoveryDataClusters_ = 0;
  uint8_t recoverySectorsPerCluster_ = 0;
  size_t recoveryCursorFile_ = static_cast<size_t>(-1);
  uint32_t recoveryCursorOffset_ = 0;
  uint32_t recoveryCursorCluster_ = 0;
  std::vector<uint32_t> recoveryVisitedDirs_;
  bool readSector(uint32_t sector, uint8_t* buffer);
  bool parseRecoveryVolume();
  void scanDirectory(uint32_t firstCluster, uint8_t depth);
  uint32_t fatEntry(uint32_t cluster);
  void ensureLayout();
};
}

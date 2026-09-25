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
  bool rawCarved = false;
  bool rawDeletedSpaceVerified = false;
  uint64_t rawStartByte = 0;
};

class StorageManager {
 public:
  bool begin();
  bool mount();
  void unmount();
  bool available() const { return mounted_; }
  uint64_t totalBytes() const;
  uint64_t usedBytes() const;
  uint64_t freeBytes() const;
  String listJson(const String& path);
  bool makeDir(const String& path);
  bool removePath(const String& path);
  bool clearPaperOSContents();
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
  bool startRecoveryScan();
  void cancelRecoveryScan();
  void loop();
  bool recoveryScanning() const { return recoveryScanning_; }
  uint8_t recoveryProgress() const { return recoveryProgress_; }
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
  enum class CarveMode : uint8_t { None, Jpeg, Png, Pdf, Bmp, Text };
  bool recoveryScanning_ = false;
  bool recoveryFatEntriesScanned_ = false;
  uint8_t recoveryProgress_ = 0;
  uint32_t recoveryScanLba_ = 0;
  uint32_t recoveryTotalSectors_ = 0;
  uint32_t recoveryProgressMark_ = 0;
  size_t recoveryFatCandidateCount_ = 0;
  CarveMode recoveryCarveMode_ = CarveMode::None;
  uint64_t recoveryCarveStart_ = 0;
  uint32_t recoveryCarveLength_ = 0;
  uint32_t recoveryCarveExpected_ = 0;
  uint8_t recoveryCarveHeader_[8] = {};
  uint8_t recoveryCarveHeaderCount_ = 0;
  uint16_t recoveryTextNewlines_ = 0;
  uint8_t recoveryTail_[12] = {};
  uint8_t recoveryTailCount_ = 0;
  bool readSector(uint32_t sector, uint8_t* buffer);
  bool parseRecoveryVolume();
  void processRecoverySector(uint32_t lba, const uint8_t* sector);
  void processRecoveryByte(uint8_t value, uint64_t absoluteOffset);
  void finishRawCandidate(uint32_t size, const char* extension);
  void resetCarver();
  void scanDirectory(uint32_t firstCluster, uint8_t depth);
  uint32_t fatEntry(uint32_t cluster);
  void ensureLayout();
};
}

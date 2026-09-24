#pragma once
#include <Arduino.h>
#include <SD.h>
#include <SPI.h>
#include <ArduinoJson.h>

namespace paperos {
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
  fs::FS& fs() { return SD; }
 private:
  bool mounted_ = false;
  void ensureLayout();
};
}

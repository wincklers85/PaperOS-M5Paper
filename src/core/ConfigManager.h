#pragma once

#include <Arduino.h>
#include <LittleFS.h>
#include <ArduinoJson.h>
#include "Models.h"

namespace paperos {

class ConfigManager {
 public:
  bool begin();
  bool load();
  bool save();
  bool resetToDefaults();
  const AppConfig& get() const { return config_; }
  AppConfig& edit() { return config_; }
  void setAdminPassword(const String& password);
  bool checkAdminPassword(const String& password) const;
  void upsertNetwork(const String& ssid, const String& password, int priority = 0);
  bool exportBackup(const String& path);
  bool exportBackupToSd(const String& path = "/PaperOS/Backup/settings.json");
  bool restoreBackupFromSd(const String& path = "/PaperOS/Backup/settings.json");
  bool exportWifiTextToSd(const String& path = "/PaperOS/Config/wifi_networks.txt");
  bool importWifiTextFromSd(const String& path = "/PaperOS/Config/wifi_networks.txt");

 private:
  AppConfig config_;
  static constexpr const char* CONFIG_PATH = "/paperos_config.json";
  static constexpr const char* TMP_PATH = "/paperos_config.tmp";
  static constexpr const char* BAK_PATH = "/paperos_config.bak";

  String hashPassword(const String& salt, const String& password) const;
  String randomHex(size_t bytes) const;
  bool loadFromPath(const char* path);
};

}

#include "ConfigManager.h"
#include "PaperOS.h"
#include <esp_system.h>
#include <mbedtls/sha256.h>

namespace paperos {

static String bytesToHex(const uint8_t* data, size_t len) {
  static const char* h = "0123456789abcdef";
  String out;
  out.reserve(len * 2);
  for (size_t i = 0; i < len; ++i) {
    out += h[(data[i] >> 4) & 0x0F];
    out += h[data[i] & 0x0F];
  }
  return out;
}

bool ConfigManager::begin() {
  if (!LittleFS.begin(true)) return false;
  return load();
}

bool ConfigManager::load() {
  if (loadFromPath(CONFIG_PATH)) return true;
  if (loadFromPath(BAK_PATH)) {
    save();
    return true;
  }
  return save();
}

bool ConfigManager::loadFromPath(const char* path) {
  if (!LittleFS.exists(path)) return false;
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;
  DynamicJsonDocument doc(8192);
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  if (err) return false;

  config_.deviceName = doc["deviceName"] | "PaperOS";
  config_.language = doc["language"] | "it";
  config_.timezone = doc["timezone"] | "Europe/Rome";
  config_.setupComplete = doc["setupComplete"] | false;
  config_.adminSalt = doc["adminSalt"] | "";
  config_.adminHash = doc["adminHash"] | "";
  config_.sleepMinutes = doc["sleepMinutes"] | 15;
  config_.powerMode = static_cast<PowerMode>(doc["powerMode"] | 1);
  config_.wifiNetworks.clear();
  for (JsonObject n : doc["wifiNetworks"].as<JsonArray>()) {
    WiFiCredential c;
    c.ssid = n["ssid"] | "";
    c.password = n["password"] | "";
    c.priority = n["priority"] | 0;
    if (c.ssid.length()) config_.wifiNetworks.push_back(c);
  }
  return true;
}

bool ConfigManager::save() {
  DynamicJsonDocument doc(8192);
  doc["version"] = PAPEROS_VERSION;
  doc["deviceName"] = config_.deviceName;
  doc["language"] = config_.language;
  doc["timezone"] = config_.timezone;
  doc["setupComplete"] = config_.setupComplete;
  doc["adminSalt"] = config_.adminSalt;
  doc["adminHash"] = config_.adminHash;
  doc["sleepMinutes"] = config_.sleepMinutes;
  doc["powerMode"] = static_cast<int>(config_.powerMode);
  JsonArray arr = doc.createNestedArray("wifiNetworks");
  for (const auto& n : config_.wifiNetworks) {
    JsonObject o = arr.createNestedObject();
    o["ssid"] = n.ssid;
    o["password"] = n.password;
    o["priority"] = n.priority;
  }

  File tmp = LittleFS.open(TMP_PATH, FILE_WRITE);
  if (!tmp) return false;
  if (serializeJsonPretty(doc, tmp) == 0) {
    tmp.close();
    LittleFS.remove(TMP_PATH);
    return false;
  }
  tmp.flush();
  tmp.close();

  if (LittleFS.exists(BAK_PATH)) LittleFS.remove(BAK_PATH);
  if (LittleFS.exists(CONFIG_PATH)) LittleFS.rename(CONFIG_PATH, BAK_PATH);
  if (!LittleFS.rename(TMP_PATH, CONFIG_PATH)) {
    if (LittleFS.exists(BAK_PATH)) LittleFS.rename(BAK_PATH, CONFIG_PATH);
    return false;
  }
  return true;
}

String ConfigManager::randomHex(size_t bytes) const {
  String out;
  out.reserve(bytes * 2);
  for (size_t i = 0; i < bytes; ++i) {
    uint8_t b = static_cast<uint8_t>(esp_random() & 0xFF);
    char buf[3];
    snprintf(buf, sizeof(buf), "%02x", b);
    out += buf;
  }
  return out;
}

String ConfigManager::hashPassword(const String& salt, const String& password) const {
  String input = salt + ":" + password;
  uint8_t digest[32];
  mbedtls_sha256_context ctx;
  mbedtls_sha256_init(&ctx);
  mbedtls_sha256_starts_ret(&ctx, 0);
  mbedtls_sha256_update_ret(&ctx, reinterpret_cast<const unsigned char*>(input.c_str()), input.length());
  mbedtls_sha256_finish_ret(&ctx, digest);
  mbedtls_sha256_free(&ctx);
  return bytesToHex(digest, sizeof(digest));
}

void ConfigManager::setAdminPassword(const String& password) {
  config_.adminSalt = randomHex(16);
  config_.adminHash = hashPassword(config_.adminSalt, password);
}

bool ConfigManager::checkAdminPassword(const String& password) const {
  if (!config_.adminSalt.length() || !config_.adminHash.length()) return false;
  return hashPassword(config_.adminSalt, password) == config_.adminHash;
}

void ConfigManager::upsertNetwork(const String& ssid, const String& password, int priority) {
  for (auto& n : config_.wifiNetworks) {
    if (n.ssid == ssid) {
      n.password = password;
      n.priority = priority;
      return;
    }
  }
  WiFiCredential c;
  c.ssid = ssid;
  c.password = password;
  c.priority = priority;
  config_.wifiNetworks.push_back(c);
}

bool ConfigManager::exportBackup(const String& path) {
  File src = LittleFS.open(CONFIG_PATH, FILE_READ);
  if (!src) return false;
  File dst = LittleFS.open(path, FILE_WRITE);
  if (!dst) { src.close(); return false; }
  uint8_t buf[512];
  while (src.available()) {
    size_t n = src.read(buf, sizeof(buf));
    dst.write(buf, n);
  }
  src.close();
  dst.close();
  return true;
}

}

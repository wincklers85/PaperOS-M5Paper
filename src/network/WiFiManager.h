#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include "../core/ConfigManager.h"

namespace paperos {
class WiFiManager {
 public:
  explicit WiFiManager(ConfigManager& cfg) : cfg_(cfg) {}
  void begin();
  void loop();
  bool connect(const String& ssid, const String& password, bool saveNetwork = true);
  void startSetupAp();
  String statusJson();
  bool isConnected() const { return WiFi.status() == WL_CONNECTED; }
  bool setupApActive() const { return apActive_; }
  IPAddress ip() const { return isConnected() ? WiFi.localIP() : WiFi.softAPIP(); }
  bool timeSynced() const { return timeSynced_; }
 private:
  ConfigManager& cfg_;
  DNSServer dns_;
  bool apActive_ = false;
  uint32_t lastReconnect_ = 0;
  bool timeSynced_ = false;
  void startMdns();
  bool connectKnown();
  void syncClock();
};
}

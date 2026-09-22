#include "WiFiManager.h"
#include "PaperOS.h"
#include <algorithm>
#include <ArduinoJson.h>

namespace paperos {
void WiFiManager::begin() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  if (!connectKnown()) startSetupAp();
}

bool WiFiManager::connectKnown() {
  auto list = cfg_.get().wifiNetworks;
  std::sort(list.begin(), list.end(), [](const WiFiCredential& a, const WiFiCredential& b){ return a.priority > b.priority; });
  for (const auto& n : list) if (connect(n.ssid, n.password, false)) return true;
  return false;
}

bool WiFiManager::connect(const String& ssid, const String& password, bool saveNetwork) {
  if (!ssid.length()) return false;
  if (apActive_) { dns_.stop(); WiFi.softAPdisconnect(true); apActive_ = false; }
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 9000) delay(100);
  if (WiFi.status() == WL_CONNECTED) {
    if (saveNetwork) { cfg_.upsertNetwork(ssid, password, 100); cfg_.save(); }
    startMdns();
    return true;
  }
  WiFi.disconnect(true);
  return false;
}

void WiFiManager::startSetupAp() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(SETUP_AP);
  dns_.start(53, "*", WiFi.softAPIP());
  apActive_ = true;
}

void WiFiManager::startMdns() {
  MDNS.end();
  if (MDNS.begin(HOSTNAME)) MDNS.addService("http", "tcp", 80);
}

void WiFiManager::loop() {
  if (apActive_) dns_.processNextRequest();
  if (!apActive_ && WiFi.status() != WL_CONNECTED && millis() - lastReconnect_ > 30000) {
    lastReconnect_ = millis();
    if (!connectKnown()) startSetupAp();
  }
}

String WiFiManager::statusJson() {
  DynamicJsonDocument d(2048);
  d["connected"] = isConnected();
  d["apActive"] = apActive_;
  d["ssid"] = isConnected() ? WiFi.SSID() : String(SETUP_AP);
  d["rssi"] = isConnected() ? WiFi.RSSI() : 0;
  d["ip"] = ip().toString();
  d["gateway"] = isConnected() ? WiFi.gatewayIP().toString() : "192.168.4.1";
  d["dns"] = isConnected() ? WiFi.dnsIP().toString() : "192.168.4.1";
  d["mac"] = WiFi.macAddress();
  String out; serializeJson(d,out); return out;
}

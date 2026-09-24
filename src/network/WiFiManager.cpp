#include "WiFiManager.h"
#include "PaperOS.h"
#include <algorithm>
#include <ArduinoJson.h>
#include <M5Unified.h>
#include <time.h>
#include <sys/time.h>

namespace paperos {
void WiFiManager::begin() {
  restoreClockFromRtc();
  radioEnabled_ = true;
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
    if (saveNetwork) {
      cfg_.upsertNetwork(ssid, password, 100);
      cfg_.save();
      cfg_.exportWifiTextToSd();
    }
    startMdns();
    syncClock();
    return true;
  }
  WiFi.disconnect(true);
  return false;
}


void WiFiManager::restoreClockFromRtc() {
  auto dt = M5.Rtc.getDateTime();
  if (dt.date.year < 2020 || dt.date.year > 2099) return;

  const char* tz = cfg_.get().timezone == "Europe/Rome"
    ? "CET-1CEST,M3.5.0,M10.5.0/3"
    : "UTC0";
  setenv("TZ", tz, 1);
  tzset();

  struct tm localTime = {};
  localTime.tm_year = dt.date.year - 1900;
  localTime.tm_mon = dt.date.month - 1;
  localTime.tm_mday = dt.date.date;
  localTime.tm_hour = dt.time.hours;
  localTime.tm_min = dt.time.minutes;
  localTime.tm_sec = dt.time.seconds;
  localTime.tm_isdst = -1;

  time_t epoch = mktime(&localTime);
  if (epoch > 1700000000) {
    struct timeval tv = { epoch, 0 };
    settimeofday(&tv, nullptr);
  }
}

bool WiFiManager::syncClock() {
  const char* tz = cfg_.get().timezone == "Europe/Rome"
    ? "CET-1CEST,M3.5.0,M10.5.0/3"
    : "UTC0";

  configTzTime(tz, "pool.ntp.org", "time.nist.gov");
  struct tm info;
  if (getLocalTime(&info, 1800)) {
    M5.Rtc.setDateTime({{
      static_cast<int16_t>(info.tm_year + 1900),
      static_cast<int8_t>(info.tm_mon + 1),
      static_cast<int8_t>(info.tm_mday)
    }, {
      static_cast<int8_t>(info.tm_hour),
      static_cast<int8_t>(info.tm_min),
      static_cast<int8_t>(info.tm_sec)
    }});
    timeSynced_ = true;
    return true;
  }
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

void WiFiManager::setRadioEnabled(bool enabled) {
  if (radioEnabled_ == enabled) return;
  radioEnabled_ = enabled;
  if (!enabled) {
    if (apActive_) {
      dns_.stop();
      WiFi.softAPdisconnect(true);
      apActive_ = false;
    }
    MDNS.end();
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  if (!connectKnown()) startSetupAp();
}

bool WiFiManager::syncClockNow() {
  if (!radioEnabled_ || !isConnected()) return false;
  return syncClock();
}

void WiFiManager::loop() {
  if (!radioEnabled_) return;
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
}

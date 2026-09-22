#pragma once

#include <Arduino.h>
#include <vector>

namespace paperos {

enum class PowerMode : uint8_t { Performance = 0, Balanced = 1, Eco = 2 };

struct WiFiCredential {
  String ssid;
  String password;
  int priority = 0;
};

struct AppConfig {
  String deviceName = "PaperOS";
  String language = "it";
  String timezone = "Europe/Rome";
  bool setupComplete = false;
  String adminSalt;
  String adminHash;
  PowerMode powerMode = PowerMode::Balanced;
  uint32_t sleepMinutes = 15;
  std::vector<WiFiCredential> wifiNetworks;
};

}

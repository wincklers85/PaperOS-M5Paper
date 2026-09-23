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

  // Minutes of inactivity before deep sleep. 0 disables automatic sleep.
  uint32_t sleepMinutes = 15;

  // Touch wake is the normal PaperOS wake method on original M5Paper.
  bool touchWakeEnabled = true;

  // Optional timed wake for manual sleep. 0 means wake only by touch/button.
  uint32_t scheduledWakeMinutes = 0;

  std::vector<WiFiCredential> wifiNetworks;
};

}

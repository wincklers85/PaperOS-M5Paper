#include <Arduino.h>
#include "PaperOS.h"
#include "core/ConfigManager.h"
#include "core/Logger.h"
#include "drivers/DisplayManager.h"
#include "drivers/TouchManager.h"
#include "storage/StorageManager.h"
#include "network/WiFiManager.h"
#include "services/NotesService.h"
#include "services/PowerManager.h"
#include "ui/UiManager.h"
#include "web/WebServerService.h"

using namespace paperos;

ConfigManager configManager;
DisplayManager displayManager;
TouchManager touchManager;
StorageManager storageManager;
WiFiManager wifiManager(configManager);
NotesService notesService(storageManager);
PowerManager powerManager(configManager);
UiManager uiManager(displayManager,touchManager,wifiManager,storageManager,powerManager,configManager);
WebServerService webServer(configManager,wifiManager,storageManager,notesService,powerManager,uiManager);
Logger logger;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(80);
  displayManager.begin();
  displayManager.splash();

  if (!configManager.begin()) Serial.println("[PaperOS] LittleFS/config init failed");
  bool sdOk = storageManager.begin();
  logger.begin(sdOk ? &storageManager.fs() : nullptr);
  logger.info(String("Boot ") + NAME + " " + VERSION);
  if (!sdOk) logger.warn("microSD not mounted; SD-backed features disabled");

  wifiManager.begin();
  powerManager.markActivity();
  uiManager.begin();
  webServer.begin();
  logger.info(String("Web UI: http://") + wifiManager.ip().toString());
}

void loop() {
  wifiManager.loop();
  webServer.loop();
  uiManager.loop();
  powerManager.loop();
  delay(5);
}

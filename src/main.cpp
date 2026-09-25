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
#include "services/PhoneLinkService.h"
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
PhoneLinkService phoneLinkService;
UiManager uiManager(displayManager,touchManager,wifiManager,storageManager,powerManager,configManager,phoneLinkService);
WebServerService webServer(configManager,wifiManager,storageManager,notesService,powerManager,uiManager,phoneLinkService);
Logger logger;

void setup() {
  Serial.begin(SERIAL_BAUD);
  delay(80);
  displayManager.begin();
  displayManager.splash();

  displayManager.splashProgress(12, "Inizializzazione archivio interno");
  if (!configManager.begin()) Serial.println("[PaperOS] LittleFS/config init failed");
  displayManager.splashProgress(30, "Caricamento impostazioni");
  bool sdOk = storageManager.begin();
  if (sdOk) {
    if (SD.exists("/PaperOS/Config/wifi_networks.txt")) configManager.importWifiTextFromSd();
  }
  // Do not write to the microSD during boot. This preserves deleted sectors
  // long enough for the read-only recovery app to inspect the card.
  logger.begin(nullptr);
  logger.info(String("Boot ") + NAME + " " + VERSION);
  if (!sdOk) logger.warn("microSD not mounted; SD-backed features disabled");

  displayManager.splashProgress(52, "Avvio rete e servizi");
  wifiManager.begin();
  powerManager.markActivity();
  displayManager.splashProgress(74, "Avvio Web Console");
  webServer.begin();
  logger.info(String("Web UI: http://") + wifiManager.ip().toString());
  displayManager.splashProgress(100, "Avvio completato");
  delay(120);
  uiManager.begin();
}

void loop() {
  wifiManager.loop();
  webServer.loop();
  storageManager.loop();
  uiManager.loop();
  powerManager.loop();
  delay(5);
}

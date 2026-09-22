#pragma once
#include <Arduino.h>
#include <WebServer.h>
#include <Update.h>
#include "../core/ConfigManager.h"
#include "../network/WiFiManager.h"
#include "../storage/StorageManager.h"
#include "../services/NotesService.h"
#include "../services/PowerManager.h"
#include "../ui/UiManager.h"

namespace paperos {
class WebServerService {
 public:
  WebServerService(ConfigManager& c, WiFiManager& w, StorageManager& s, NotesService& n, PowerManager& p, UiManager& u)
  : config_(c), wifi_(w), storage_(s), notes_(n), power_(p), ui_(u), server_(80) {}
  void begin();
  void loop() { server_.handleClient(); }
 private:
  ConfigManager& config_; WiFiManager& wifi_; StorageManager& storage_; NotesService& notes_; PowerManager& power_; UiManager& ui_;
  WebServer server_; String sessionToken_; File uploadFile_; bool otaOk_=false;
  bool authorized();
  void sendJson(int code,const String& json);
  String body();
  String newToken();
  void routes();
};
}

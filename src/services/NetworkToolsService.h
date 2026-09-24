#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP32Ping.h>
#include <vector>

namespace paperos {

struct PingResult {
  bool ok = false;
  String target;
  IPAddress ip;
  float averageMs = 0.0f;
  String error;
};

struct LanServiceHost {
  IPAddress ip;
  uint16_t port = 0;
};

struct ApiResult {
  bool ok = false;
  int status = 0;
  String contentType;
  String body;
  String error;
};

class NetworkToolsService {
 public:
  NetworkToolsService();

  bool dnsLookup(const String& host, IPAddress& out);
  PingResult ping(const String& host, uint8_t count = 2);
  std::vector<LanServiceHost> quickLanScan(uint8_t firstHost = 1, uint8_t lastHost = 64);
  ApiResult httpRequest(const String& method, const String& url, const String& body = "");
  bool wakeOnLan(const String& macText, uint16_t port = 9);

  bool mqttConnect(const String& host, uint16_t port = 1883);
  void mqttDisconnect();
  bool mqttPublish(const String& topic, const String& payload, bool retained = false);
  bool mqttSubscribe(const String& topic);
  bool mqttConnected() const;
  int mqttState() const;
  String mqttLastTopic() const { return lastMqttTopic_; }
  String mqttLastPayload() const { return lastMqttPayload_; }
  String mqttHost() const { return mqttHost_; }
  uint16_t mqttPort() const { return mqttPort_; }
  void loop();

 private:
  WiFiClient mqttNet_;
  PubSubClient mqtt_;
  String mqttHost_;
  uint16_t mqttPort_ = 1883;
  String lastMqttTopic_;
  String lastMqttPayload_;
  static NetworkToolsService* callbackOwner_;

  static void mqttCallback(char* topic, uint8_t* payload, unsigned int length);
  bool parseMac(const String& text, uint8_t out[6]) const;
};

}

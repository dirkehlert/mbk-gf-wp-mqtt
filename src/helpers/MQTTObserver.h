#pragma once

#ifdef WITH_MQTT_OBSERVER

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <helpers/CommonCLI.h>

class MQTTObserver {
  NodePrefs *_prefs;
  mesh::LocalIdentity *_identity;
  WiFiClient _plain_client;
  WiFiClientSecure _secure_client;
  PubSubClient _mqtt;
  unsigned long _next_wifi_attempt;
  unsigned long _next_mqtt_attempt;
  uint32_t _packets_published;
  uint32_t _publish_failures;
  uint32_t _wifi_failures;
  uint32_t _mqtt_failures;
  unsigned long _last_status_publish;
  int _last_mqtt_state;
  char _last_error[32];
  bool _running;
  bool _connect_announced;

  Client& activeClient();
  bool ensureWifi();
  bool ensureMqtt(uint32_t timestamp = 0);
  void disconnectMqtt();
  void makeClientId(char *client_id, size_t len) const;
  void formatTopic(char *topic, size_t len) const;
  void formatStatusTopic(char *topic, size_t len) const;
  static void bytesToHex(char *dest, const uint8_t *src, size_t len);
  static int snrQuarterToInteger(int snr_x4);
  static void formatTimestamp(char *dest, size_t len, uint32_t timestamp);
  static void formatTime(char *dest, size_t len, uint32_t timestamp);
  static void formatDate(char *dest, size_t len, uint32_t timestamp);
  bool publishStatus(bool online, uint32_t timestamp, bool detailed);
  bool publishSelfAdvert(uint32_t timestamp);
  bool sendPacketZephCore(mesh::Packet *packet, bool rx_packet, int rssi, int snr_x4, uint32_t timestamp,
                          int score_x1000, bool zero_hash = false);

public:
  MQTTObserver(NodePrefs *prefs, mesh::LocalIdentity *identity);

  void begin();
  void end();
  void loop(uint32_t timestamp = 0);
  bool isRunning() const { return _running; }
  bool isWifiConnected();
  bool isMqttConnected();
  int mqttState();
  const char* lastError() const { return _last_error; }
  uint32_t packetsPublished() const { return _packets_published; }
  uint32_t publishFailures() const { return _publish_failures; }
  uint32_t wifiFailures() const { return _wifi_failures; }
  uint32_t mqttFailures() const { return _mqtt_failures; }
  int lastMqttState() const { return _last_mqtt_state; }
  bool sendPacket(mesh::Packet *packet, bool rx_packet, int rssi, int snr_x4, uint32_t timestamp = 0,
                  int score_x1000 = 0);
};

#endif

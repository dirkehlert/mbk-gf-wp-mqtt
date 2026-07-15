#include "MQTTObserver.h"

#ifdef WITH_MQTT_OBSERVER

#include <Mesh.h>
#include <RTClib.h>
#include <helpers/AdvertDataHelpers.h>

#ifndef MQTT_OBSERVER_BUFFER_SIZE
#define MQTT_OBSERVER_BUFFER_SIZE 1024
#endif

#ifndef MQTT_OBSERVER_WIFI_RETRY_MS
#define MQTT_OBSERVER_WIFI_RETRY_MS 10000
#endif

#ifndef MQTT_OBSERVER_MQTT_RETRY_MS
#define MQTT_OBSERVER_MQTT_RETRY_MS 5000
#endif

#ifndef MQTT_OBSERVER_FORMAT_ZEPHCORE
#define MQTT_OBSERVER_FORMAT_ZEPHCORE 0
#endif

#ifndef MQTT_OBSERVER_STATUS_INTERVAL_MS
#define MQTT_OBSERVER_STATUS_INTERVAL_MS 60000UL
#endif

#ifndef MQTT_OBSERVER_SELF_ADVERT
#define MQTT_OBSERVER_SELF_ADVERT 0
#endif

#ifndef MQTT_OBSERVER_ORIGIN
  #ifdef ADVERT_NAME
    #define MQTT_OBSERVER_ORIGIN ADVERT_NAME
  #else
    #define MQTT_OBSERVER_ORIGIN "MeshCore Observer"
  #endif
#endif

#ifndef MQTT_OBSERVER_MODEL
#define MQTT_OBSERVER_MODEL "MeshCore Observer"
#endif

#ifndef MQTT_OBSERVER_FIRMWARE_VERSION
#define MQTT_OBSERVER_FIRMWARE_VERSION "v1.16.0"
#endif

#ifndef MQTT_OBSERVER_CLIENT_VERSION
#define MQTT_OBSERVER_CLIENT_VERSION "meshcore-mqtt-observer/1.16"
#endif

MQTTObserver::MQTTObserver(NodePrefs *prefs, mesh::LocalIdentity *identity)
    : _prefs(prefs),
      _identity(identity),
      _mqtt(_plain_client),
      _next_wifi_attempt(0),
      _next_mqtt_attempt(0),
      _packets_published(0),
      _publish_failures(0),
      _wifi_failures(0),
      _mqtt_failures(0),
      _last_status_publish(0),
      _last_mqtt_state(0),
      _running(false),
      _connect_announced(false) {
  _last_error[0] = 0;
}

Client& MQTTObserver::activeClient() {
  return _prefs->mqtt_tls ? (Client&)_secure_client : (Client&)_plain_client;
}

void MQTTObserver::begin() {
  if (!_prefs->mqtt_enabled || !_prefs->wifi_ssid[0] || !_prefs->mqtt_host[0] || !_prefs->mqtt_topic[0]) {
    return;
  }

  WiFi.mode(WIFI_STA);
  if (_prefs->mqtt_tls) {
    _secure_client.setInsecure();
  }
  _mqtt.setClient(activeClient());
  _mqtt.setServer(_prefs->mqtt_host, _prefs->mqtt_port);
  _mqtt.setBufferSize(MQTT_OBSERVER_BUFFER_SIZE);
  _next_wifi_attempt = 0;
  _next_mqtt_attempt = 0;
  _last_status_publish = 0;
  _last_error[0] = 0;
  _running = true;
  _connect_announced = false;
}

void MQTTObserver::disconnectMqtt() {
  if (_mqtt.connected()) {
    publishStatus(false, 0, false);
    _mqtt.disconnect();
  }
  _connect_announced = false;
}

void MQTTObserver::end() {
  disconnectMqtt();
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  strncpy(_last_error, "disabled", sizeof(_last_error));
  _last_error[sizeof(_last_error) - 1] = 0;
  _running = false;
}

bool MQTTObserver::ensureWifi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (strcmp(_last_error, "wifi") == 0) {
      _last_error[0] = 0;
    }
    return true;
  }

  unsigned long now = millis();
  if (_next_wifi_attempt && now < _next_wifi_attempt) {
    return false;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(_prefs->wifi_ssid, _prefs->wifi_password);
  _wifi_failures++;
  strncpy(_last_error, "wifi", sizeof(_last_error));
  _last_error[sizeof(_last_error) - 1] = 0;
  _next_wifi_attempt = now + MQTT_OBSERVER_WIFI_RETRY_MS;
  return false;
}

void MQTTObserver::makeClientId(char *client_id, size_t len) const {
  char pubkey[17];
  bytesToHex(pubkey, _identity->pub_key, 8);
  snprintf(client_id, len, "meshcore-%s", pubkey);
}

void MQTTObserver::formatTopic(char *topic, size_t len) const {
  char pubkey_hex[PUB_KEY_SIZE * 2 + 1];
  bytesToHex(pubkey_hex, _identity->pub_key, PUB_KEY_SIZE);

  size_t out = 0;
  for (const char *in = _prefs->mqtt_topic; *in && out + 1 < len; in++) {
    if (strncmp(in, "{PUBLIC_KEY}", 12) == 0) {
      for (const char *key = pubkey_hex; *key && out + 1 < len; key++) {
        topic[out++] = *key;
      }
      in += 11;
    } else {
      topic[out++] = *in;
    }
  }
  topic[out] = 0;
}

void MQTTObserver::formatStatusTopic(char *topic, size_t len) const {
  formatTopic(topic, len);
  const char suffix[] = "/packets";
  size_t topic_len = strlen(topic);
  size_t suffix_len = sizeof(suffix) - 1;
  if (topic_len >= suffix_len && strcmp(topic + topic_len - suffix_len, suffix) == 0) {
    snprintf(topic + topic_len - suffix_len, len - (topic_len - suffix_len), "/status");
  } else if (topic_len + 7 < len) {
    strncat(topic, "/status", len - topic_len - 1);
  }
}

bool MQTTObserver::ensureMqtt(uint32_t timestamp) {
  if (_mqtt.connected()) {
    if (strcmp(_last_error, "mqtt") == 0) {
      _last_error[0] = 0;
    }
    return true;
  }
  _connect_announced = false;

  if (!ensureWifi()) {
    return false;
  }

  unsigned long now = millis();
  if (_next_mqtt_attempt && now < _next_mqtt_attempt) {
    return false;
  }

  char client_id[32];
  char status_topic[sizeof(_prefs->mqtt_topic) + PUB_KEY_SIZE * 2];
  makeClientId(client_id, sizeof(client_id));
  formatStatusTopic(status_topic, sizeof(status_topic));

  bool ok;
  if (_prefs->mqtt_username[0] || _prefs->mqtt_password[0]) {
    ok = _mqtt.connect(client_id, _prefs->mqtt_username, _prefs->mqtt_password, status_topic, 0, true,
                       "{\"status\":\"offline\"}");
  } else {
    ok = _mqtt.connect(client_id, status_topic, 0, true, "{\"status\":\"offline\"}");
  }
  _last_mqtt_state = _mqtt.state();
  if (!ok) {
    _mqtt_failures++;
    strncpy(_last_error, "mqtt", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
  } else if (strcmp(_last_error, "mqtt") == 0) {
    _last_error[0] = 0;
  }
  if (ok) {
    publishStatus(true, timestamp, false);
    publishStatus(true, timestamp, true);
    publishSelfAdvert(timestamp);
    _connect_announced = true;
    _last_status_publish = millis();
  }
  _next_mqtt_attempt = now + MQTT_OBSERVER_MQTT_RETRY_MS;
  return ok;
}

void MQTTObserver::loop(uint32_t timestamp) {
  if (!_running) {
    return;
  }

  if (ensureMqtt(timestamp)) {
    _mqtt.loop();
    unsigned long now = millis();
    if (_last_status_publish == 0 || now - _last_status_publish >= MQTT_OBSERVER_STATUS_INTERVAL_MS) {
      publishStatus(true, timestamp, true);
      _last_status_publish = now;
    }
  }
}

bool MQTTObserver::isWifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

bool MQTTObserver::isMqttConnected() {
  return _mqtt.connected();
}

int MQTTObserver::mqttState() {
  return _mqtt.state();
}

void MQTTObserver::bytesToHex(char *dest, const uint8_t *src, size_t len) {
  static const char *hex = "0123456789ABCDEF";
  for (size_t i = 0; i < len; i++) {
    dest[i * 2] = hex[(src[i] >> 4) & 0x0F];
    dest[i * 2 + 1] = hex[src[i] & 0x0F];
  }
  dest[len * 2] = 0;
}

int MQTTObserver::snrQuarterToInteger(int snr_x4) {
  if (snr_x4 >= 0) {
    return (snr_x4 + 2) / 4;
  }
  return (snr_x4 - 2) / 4;
}

void MQTTObserver::formatTimestamp(char *dest, size_t len, uint32_t timestamp) {
  DateTime dt = DateTime(timestamp);
  snprintf(dest, len, "%04d-%02d-%02dT%02d:%02d:%02d.000000+00:00", dt.year(), dt.month(), dt.day(), dt.hour(),
           dt.minute(), dt.second());
}

void MQTTObserver::formatTime(char *dest, size_t len, uint32_t timestamp) {
  DateTime dt = DateTime(timestamp);
  snprintf(dest, len, "%02d:%02d:%02d", dt.hour(), dt.minute(), dt.second());
}

void MQTTObserver::formatDate(char *dest, size_t len, uint32_t timestamp) {
  DateTime dt = DateTime(timestamp);
  snprintf(dest, len, "%d/%d/%04d", dt.day(), dt.month(), dt.year());
}

bool MQTTObserver::publishStatus(bool online, uint32_t timestamp, bool detailed) {
  char topic[sizeof(_prefs->mqtt_topic) + PUB_KEY_SIZE * 2];
  char payload[MQTT_OBSERVER_BUFFER_SIZE];
  formatStatusTopic(topic, sizeof(topic));

  if (!detailed) {
    snprintf(payload, sizeof(payload), "{\"status\":\"%s\"}", online ? "online" : "offline");
  } else {
    char pubkey_hex[PUB_KEY_SIZE * 2 + 1];
    char timestamp_str[40];
    bytesToHex(pubkey_hex, _identity->pub_key, PUB_KEY_SIZE);
    formatTimestamp(timestamp_str, sizeof(timestamp_str), timestamp);
    snprintf(payload, sizeof(payload),
             "{\"status\":\"%s\",\"timestamp\":\"%s\",\"origin\":\"%s\",\"origin_id\":\"%s\","
             "\"radio\":\"%.3f,%u,%u,%u\",\"model\":\"%s\",\"firmware_version\":\"%s\","
             "\"client_version\":\"%s\",\"stats\":{\"packets_published\":%lu,\"publish_failures\":%lu,"
             "\"wifi_failures\":%lu,\"mqtt_failures\":%lu,\"uptime\":%lu}}",
             online ? "online" : "offline",
             timestamp_str,
             MQTT_OBSERVER_ORIGIN,
             pubkey_hex,
             _prefs->freq,
             (uint32_t)_prefs->bw,
             (uint32_t)_prefs->sf,
             (uint32_t)_prefs->cr,
             MQTT_OBSERVER_MODEL,
             MQTT_OBSERVER_FIRMWARE_VERSION,
             MQTT_OBSERVER_CLIENT_VERSION,
             (unsigned long)_packets_published,
             (unsigned long)_publish_failures,
             (unsigned long)_wifi_failures,
             (unsigned long)_mqtt_failures,
             (unsigned long)(millis() / 1000UL));
  }

  int payload_len = strlen(payload);
  if (payload_len <= 0 || payload_len >= (int)sizeof(payload)) {
    _publish_failures++;
    strncpy(_last_error, "payload", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
    return false;
  }

  if (_mqtt.publish(topic, payload, !detailed)) {
    if (strcmp(_last_error, "publish") == 0 || strcmp(_last_error, "payload") == 0) {
      _last_error[0] = 0;
    }
    return true;
  }

  _publish_failures++;
  _last_mqtt_state = _mqtt.state();
  strncpy(_last_error, "publish", sizeof(_last_error));
  _last_error[sizeof(_last_error) - 1] = 0;
  return false;
}

bool MQTTObserver::publishSelfAdvert(uint32_t timestamp) {
#if MQTT_OBSERVER_FORMAT_ZEPHCORE && MQTT_OBSERVER_SELF_ADVERT
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  AdvertDataBuilder builder(ADV_TYPE_REPEATER, _prefs->node_name, _prefs->node_lat, _prefs->node_lon);
  uint8_t app_data_len = builder.encodeTo(app_data);

  mesh::Packet packet;
  packet.header = (PAYLOAD_TYPE_ADVERT << PH_TYPE_SHIFT) | ROUTE_TYPE_DIRECT;
  packet.path_len = 0;

  int len = 0;
  memcpy(&packet.payload[len], _identity->pub_key, PUB_KEY_SIZE);
  len += PUB_KEY_SIZE;
  memcpy(&packet.payload[len], &timestamp, 4);
  len += 4;

  uint8_t *signature = &packet.payload[len];
  len += SIGNATURE_SIZE;
  memcpy(&packet.payload[len], app_data, app_data_len);
  len += app_data_len;
  packet.payload_len = len;

  uint8_t message[PUB_KEY_SIZE + 4 + MAX_ADVERT_DATA_SIZE];
  int msg_len = 0;
  memcpy(&message[msg_len], _identity->pub_key, PUB_KEY_SIZE);
  msg_len += PUB_KEY_SIZE;
  memcpy(&message[msg_len], &timestamp, 4);
  msg_len += 4;
  memcpy(&message[msg_len], app_data, app_data_len);
  msg_len += app_data_len;
  _identity->sign(signature, message, msg_len);

  return sendPacketZephCore(&packet, true, 0, 0, timestamp, 0, true);
#else
  (void)timestamp;
  return true;
#endif
}

bool MQTTObserver::sendPacketZephCore(mesh::Packet *packet, bool rx_packet, int rssi, int snr_x4, uint32_t timestamp,
                                      int score_x1000, bool zero_hash) {
  uint8_t raw[MAX_TRANS_UNIT + 1];
  uint8_t hash[MAX_HASH_SIZE];
  uint8_t raw_len = packet->writeTo(raw);
  char raw_hex[(MAX_TRANS_UNIT + 1) * 2 + 1];
  char hash_hex[17];
  char pubkey_hex[PUB_KEY_SIZE * 2 + 1];
  char topic[sizeof(_prefs->mqtt_topic) + PUB_KEY_SIZE * 2];
  char timestamp_str[40];
  char time_str[16];
  char date_str[16];
  char payload[MQTT_OBSERVER_BUFFER_SIZE];

  bytesToHex(raw_hex, raw, raw_len);
  packet->calculatePacketHash(hash);
  if (zero_hash) {
    strcpy(hash_hex, "0000000000000000");
  } else {
    bytesToHex(hash_hex, hash, 8);
  }
  bytesToHex(pubkey_hex, _identity->pub_key, PUB_KEY_SIZE);
  formatTimestamp(timestamp_str, sizeof(timestamp_str), timestamp);
  formatTime(time_str, sizeof(time_str), timestamp);
  formatDate(date_str, sizeof(date_str), timestamp);
  formatTopic(topic, sizeof(topic));

  int len = snprintf(payload, sizeof(payload),
                     "{\"type\":\"PACKET\",\"origin\":\"%s\",\"origin_id\":\"%s\",\"timestamp\":\"%s\","
                     "\"direction\":\"%s\",\"time\":\"%s\",\"date\":\"%s\",\"len\":\"%u\","
                     "\"packet_type\":\"%u\",\"route\":\"%s\",\"payload_len\":\"%u\",\"raw\":\"%s\","
                     "\"SNR\":\"%d\",\"RSSI\":\"%d\",\"score\":\"%d\",\"hash\":\"%s\"}",
                     MQTT_OBSERVER_ORIGIN,
                     pubkey_hex,
                     timestamp_str,
                     rx_packet ? "rx" : "tx",
                     time_str,
                     date_str,
                     (uint32_t)raw_len,
                     (uint32_t)packet->getPayloadType(),
                     packet->isRouteDirect() ? "D" : "F",
                     (uint32_t)packet->payload_len,
                     raw_hex,
                     snrQuarterToInteger(snr_x4),
                     (int32_t)rssi,
                     (int32_t)score_x1000,
                     hash_hex);

  if (len <= 0 || len >= (int)sizeof(payload)) {
    _publish_failures++;
    strncpy(_last_error, "payload", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
    return false;
  }

  if (_mqtt.publish(topic, payload)) {
    _packets_published++;
    if (strcmp(_last_error, "publish") == 0 || strcmp(_last_error, "payload") == 0) {
      _last_error[0] = 0;
    }
    return true;
  }

  _publish_failures++;
  _last_mqtt_state = _mqtt.state();
  strncpy(_last_error, "publish", sizeof(_last_error));
  _last_error[sizeof(_last_error) - 1] = 0;
  return false;
}

bool MQTTObserver::sendPacket(mesh::Packet *packet, bool rx_packet, int rssi, int snr_x4, uint32_t timestamp,
                              int score_x1000) {
  // MQTT is an observer-only side channel. Failure here must never enqueue,
  // acknowledge, advertise, or otherwise send anything back into the mesh.
  if (!_running || !packet || !ensureMqtt(timestamp)) {
    return false;
  }

#if MQTT_OBSERVER_FORMAT_ZEPHCORE
  return sendPacketZephCore(packet, rx_packet, rssi, snr_x4, timestamp, score_x1000);
#else
  (void)timestamp;
  (void)score_x1000;

  uint8_t raw[MAX_TRANS_UNIT + 1];
  uint8_t raw_len = packet->writeTo(raw);
  char raw_hex[(MAX_TRANS_UNIT + 1) * 2 + 1];
  char pubkey_hex[PUB_KEY_SIZE * 2 + 1];
  char topic[sizeof(_prefs->mqtt_topic) + PUB_KEY_SIZE * 2];
  char payload[MQTT_OBSERVER_BUFFER_SIZE];

  bytesToHex(raw_hex, raw, raw_len);
  bytesToHex(pubkey_hex, _identity->pub_key, PUB_KEY_SIZE);
  formatTopic(topic, sizeof(topic));

  int len = snprintf(payload, sizeof(payload),
                     "{\"type\":\"%s\",\"pubkey\":\"%s\",\"packet\":\"%s\",\"payload_type\":%u,\"route\":%u,\"path_hash_size\":%u,\"path_hash_count\":%u,\"rssi\":%d,\"snr\":%d}",
                     rx_packet ? "rx" : "tx",
                     pubkey_hex,
                     raw_hex,
                     (uint32_t)packet->getPayloadType(),
                     (uint32_t)packet->getRouteType(),
                     (uint32_t)packet->getPathHashSize(),
                     (uint32_t)packet->getPathHashCount(),
                     (int32_t)rssi,
                     (int32_t)snr_x4);

  if (len <= 0 || len >= (int)sizeof(payload)) {
    _publish_failures++;
    strncpy(_last_error, "payload", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
    return false;
  }

  if (_mqtt.publish(topic, payload)) {
    _packets_published++;
    if (strcmp(_last_error, "publish") == 0 || strcmp(_last_error, "payload") == 0) {
      _last_error[0] = 0;
    }
    return true;
  } else {
    _publish_failures++;
    _last_mqtt_state = _mqtt.state();
    strncpy(_last_error, "publish", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
    return false;
  }
#endif
}

#endif

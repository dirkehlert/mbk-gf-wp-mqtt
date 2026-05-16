#include "MQTTObserver.h"

#ifdef WITH_MQTT_OBSERVER

#include <Mesh.h>

#ifndef MQTT_OBSERVER_BUFFER_SIZE
#define MQTT_OBSERVER_BUFFER_SIZE 1024
#endif

#ifndef MQTT_OBSERVER_WIFI_RETRY_MS
#define MQTT_OBSERVER_WIFI_RETRY_MS 10000
#endif

#ifndef MQTT_OBSERVER_MQTT_RETRY_MS
#define MQTT_OBSERVER_MQTT_RETRY_MS 5000
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
      _last_mqtt_state(0),
      _running(false) {
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
  _last_error[0] = 0;
  _running = true;
}

void MQTTObserver::disconnectMqtt() {
  if (_mqtt.connected()) {
    _mqtt.disconnect();
  }
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

bool MQTTObserver::ensureMqtt() {
  if (_mqtt.connected()) {
    if (strcmp(_last_error, "mqtt") == 0) {
      _last_error[0] = 0;
    }
    return true;
  }

  if (!ensureWifi()) {
    return false;
  }

  unsigned long now = millis();
  if (_next_mqtt_attempt && now < _next_mqtt_attempt) {
    return false;
  }

  char client_id[32];
  makeClientId(client_id, sizeof(client_id));

  bool ok;
  if (_prefs->mqtt_username[0] || _prefs->mqtt_password[0]) {
    ok = _mqtt.connect(client_id, _prefs->mqtt_username, _prefs->mqtt_password);
  } else {
    ok = _mqtt.connect(client_id);
  }
  _last_mqtt_state = _mqtt.state();
  if (!ok) {
    _mqtt_failures++;
    strncpy(_last_error, "mqtt", sizeof(_last_error));
    _last_error[sizeof(_last_error) - 1] = 0;
  } else if (strcmp(_last_error, "mqtt") == 0) {
    _last_error[0] = 0;
  }
  _next_mqtt_attempt = now + MQTT_OBSERVER_MQTT_RETRY_MS;
  return ok;
}

void MQTTObserver::loop() {
  if (!_running) {
    return;
  }

  if (ensureMqtt()) {
    _mqtt.loop();
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

bool MQTTObserver::sendPacket(mesh::Packet *packet, bool rx_packet, int rssi, int snr_x4) {
  // MQTT is an observer-only side channel. Failure here must never enqueue,
  // acknowledge, advertise, or otherwise send anything back into the mesh.
  if (!_running || !packet || !ensureMqtt()) {
    return false;
  }

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
}

#endif

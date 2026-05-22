#include "MyMesh.h"

#include <Arduino.h> // needed for PlatformIO
#include <Mesh.h>

#define CMD_APP_START                 1
#define CMD_SEND_TXT_MSG              2
#define CMD_SEND_CHANNEL_TXT_MSG      3
#define CMD_GET_CONTACTS              4 // with optional 'since' (for efficient sync)
#define CMD_GET_DEVICE_TIME           5
#define CMD_SET_DEVICE_TIME           6
#define CMD_SEND_SELF_ADVERT          7
#define CMD_SET_ADVERT_NAME           8
#define CMD_ADD_UPDATE_CONTACT        9
#define CMD_SYNC_NEXT_MESSAGE         10
#define CMD_SET_RADIO_PARAMS          11
#define CMD_SET_RADIO_TX_POWER        12
#define CMD_RESET_PATH                13
#define CMD_SET_ADVERT_LATLON         14
#define CMD_REMOVE_CONTACT            15
#define CMD_SHARE_CONTACT             16
#define CMD_EXPORT_CONTACT            17
#define CMD_IMPORT_CONTACT            18
#define CMD_REBOOT                    19
#define CMD_GET_BATT_AND_STORAGE      20   // was CMD_GET_BATTERY_VOLTAGE
#define CMD_SET_TUNING_PARAMS         21
#define CMD_DEVICE_QEURY              22
#define CMD_EXPORT_PRIVATE_KEY        23
#define CMD_IMPORT_PRIVATE_KEY        24
#define CMD_SEND_RAW_DATA             25
#define CMD_SEND_LOGIN                26
#define CMD_SEND_STATUS_REQ           27
#define CMD_HAS_CONNECTION            28
#define CMD_LOGOUT                    29 // 'Disconnect'
#define CMD_GET_CONTACT_BY_KEY        30
#define CMD_GET_CHANNEL               31
#define CMD_SET_CHANNEL               32
#define CMD_SIGN_START                33
#define CMD_SIGN_DATA                 34
#define CMD_SIGN_FINISH               35
#define CMD_SEND_TRACE_PATH           36
#define CMD_SET_DEVICE_PIN            37
#define CMD_SET_OTHER_PARAMS          38
#define CMD_SEND_TELEMETRY_REQ        39  // can deprecate this
#define CMD_GET_CUSTOM_VARS           40
#define CMD_SET_CUSTOM_VAR            41
#define CMD_GET_ADVERT_PATH           42
#define CMD_GET_TUNING_PARAMS         43
// NOTE: CMD range 44..49 parked, potentially for WiFi operations
#define CMD_SEND_BINARY_REQ           50
#define CMD_FACTORY_RESET             51
#define CMD_SEND_PATH_DISCOVERY_REQ   52
#define CMD_SET_FLOOD_SCOPE_KEY       54   // v8+
#define CMD_SEND_CONTROL_DATA         55   // v8+
#define CMD_GET_STATS                 56   // v8+, second byte is stats type
#define CMD_SEND_ANON_REQ             57
#define CMD_SET_AUTOADD_CONFIG        58
#define CMD_GET_AUTOADD_CONFIG        59
#define CMD_GET_ALLOWED_REPEAT_FREQ   60
#define CMD_SET_PATH_HASH_MODE        61
#define CMD_SEND_CHANNEL_DATA         62
#define CMD_SET_DEFAULT_FLOOD_SCOPE   63
#define CMD_GET_DEFAULT_FLOOD_SCOPE   64

// Stats sub-types for CMD_GET_STATS
#define STATS_TYPE_CORE               0
#define STATS_TYPE_RADIO              1
#define STATS_TYPE_PACKETS             2

#define RESP_CODE_OK                  0
#define RESP_CODE_ERR                 1
#define RESP_CODE_CONTACTS_START      2  // first reply to CMD_GET_CONTACTS
#define RESP_CODE_CONTACT             3  // multiple of these (after CMD_GET_CONTACTS)
#define RESP_CODE_END_OF_CONTACTS     4  // last reply to CMD_GET_CONTACTS
#define RESP_CODE_SELF_INFO           5  // reply to CMD_APP_START
#define RESP_CODE_SENT                6  // reply to CMD_SEND_TXT_MSG
#define RESP_CODE_CONTACT_MSG_RECV    7  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CHANNEL_MSG_RECV    8  // a reply to CMD_SYNC_NEXT_MESSAGE (ver < 3)
#define RESP_CODE_CURR_TIME           9  // a reply to CMD_GET_DEVICE_TIME
#define RESP_CODE_NO_MORE_MESSAGES    10 // a reply to CMD_SYNC_NEXT_MESSAGE
#define RESP_CODE_EXPORT_CONTACT      11
#define RESP_CODE_BATT_AND_STORAGE    12 // a reply to a CMD_GET_BATT_AND_STORAGE
#define RESP_CODE_DEVICE_INFO         13 // a reply to CMD_DEVICE_QEURY
#define RESP_CODE_PRIVATE_KEY         14 // a reply to CMD_EXPORT_PRIVATE_KEY
#define RESP_CODE_DISABLED            15
#define RESP_CODE_CONTACT_MSG_RECV_V3 16 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_MSG_RECV_V3 17 // a reply to CMD_SYNC_NEXT_MESSAGE (ver >= 3)
#define RESP_CODE_CHANNEL_INFO        18 // a reply to CMD_GET_CHANNEL
#define RESP_CODE_SIGN_START          19
#define RESP_CODE_SIGNATURE           20
#define RESP_CODE_CUSTOM_VARS         21
#define RESP_CODE_ADVERT_PATH         22
#define RESP_CODE_TUNING_PARAMS       23
#define RESP_CODE_STATS               24   // v8+, second byte is stats type
#define RESP_CODE_AUTOADD_CONFIG      25
#define RESP_ALLOWED_REPEAT_FREQ      26
#define RESP_CODE_CHANNEL_DATA_RECV   27
#define RESP_CODE_DEFAULT_FLOOD_SCOPE 28

#define MAX_CHANNEL_DATA_LENGTH       (MAX_FRAME_SIZE - 9)

#define SEND_TIMEOUT_BASE_MILLIS        500
#define FLOOD_SEND_TIMEOUT_FACTOR       16.0f
#define DIRECT_SEND_PERHOP_FACTOR       6.0f
#define DIRECT_SEND_PERHOP_EXTRA_MILLIS 250
#define LAZY_CONTACTS_WRITE_DELAY       5000

#define PUBLIC_GROUP_PSK                "izOH6cXN6mrJ5e26oRXNcg=="
#define QUICK_MESSAGES_FILE             "/quickmsg"
#define ANON_REQ_TYPE_REGIONS           0x01
#define CTL_TYPE_NODE_DISCOVER_REQ      0x80
#define CTL_TYPE_NODE_DISCOVER_RESP     0x90

// these are _pushed_ to client app at any time
#define PUSH_CODE_ADVERT                0x80
#define PUSH_CODE_PATH_UPDATED          0x81
#define PUSH_CODE_SEND_CONFIRMED        0x82
#define PUSH_CODE_MSG_WAITING           0x83
#define PUSH_CODE_RAW_DATA              0x84
#define PUSH_CODE_LOGIN_SUCCESS         0x85
#define PUSH_CODE_LOGIN_FAIL            0x86
#define PUSH_CODE_STATUS_RESPONSE       0x87
#define PUSH_CODE_LOG_RX_DATA           0x88
#define PUSH_CODE_TRACE_DATA            0x89
#define PUSH_CODE_NEW_ADVERT            0x8A
#define PUSH_CODE_TELEMETRY_RESPONSE    0x8B
#define PUSH_CODE_BINARY_RESPONSE       0x8C
#define PUSH_CODE_PATH_DISCOVERY_RESPONSE 0x8D
#define PUSH_CODE_CONTROL_DATA          0x8E   // v8+
#define PUSH_CODE_CONTACT_DELETED       0x8F // used to notify client app of deleted contact when overwriting oldest
#define PUSH_CODE_CONTACTS_FULL         0x90 // used to notify client app that contacts storage is full

#define ERR_CODE_UNSUPPORTED_CMD        1
#define ERR_CODE_NOT_FOUND              2
#define ERR_CODE_TABLE_FULL             3
#define ERR_CODE_BAD_STATE              4
#define ERR_CODE_FILE_IO_ERROR          5
#define ERR_CODE_ILLEGAL_ARG            6

#define MAX_SIGN_DATA_LEN               (8 * 1024) // 8K

// Auto-add config bitmask
// Bit 0: If set, overwrite oldest non-favourite contact when contacts file is full
// Bits 1-4: these indicate which contact types to auto-add when manual_contact_mode = 0x01
#define AUTO_ADD_OVERWRITE_OLDEST (1 << 0)  // 0x01 - overwrite oldest non-favourite when full
#define AUTO_ADD_CHAT             (1 << 1)  // 0x02 - auto-add Chat (Companion) (ADV_TYPE_CHAT)
#define AUTO_ADD_REPEATER         (1 << 2)  // 0x04 - auto-add Repeater (ADV_TYPE_REPEATER)
#define AUTO_ADD_ROOM_SERVER      (1 << 3)  // 0x08 - auto-add Room Server (ADV_TYPE_ROOM)
#define AUTO_ADD_SENSOR           (1 << 4)  // 0x10 - auto-add Sensor (ADV_TYPE_SENSOR)

void MyMesh::writeOKFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_OK;
  _serial->writeFrame(buf, 1);
}
void MyMesh::writeErrFrame(uint8_t err_code) {
  uint8_t buf[2];
  buf[0] = RESP_CODE_ERR;
  buf[1] = err_code;
  _serial->writeFrame(buf, 2);
}

void MyMesh::writeDisabledFrame() {
  uint8_t buf[1];
  buf[0] = RESP_CODE_DISABLED;
  _serial->writeFrame(buf, 1);
}

void MyMesh::writeContactRespFrame(uint8_t code, const ContactInfo &contact) {
  int i = 0;
  out_frame[i++] = code;
  memcpy(&out_frame[i], contact.id.pub_key, PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  out_frame[i++] = contact.type;
  out_frame[i++] = contact.flags;
  out_frame[i++] = contact.out_path_len;
  memcpy(&out_frame[i], contact.out_path, MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  StrHelper::strzcpy((char *)&out_frame[i], contact.name, 32);
  i += 32;
  memcpy(&out_frame[i], &contact.last_advert_timestamp, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lat, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.gps_lon, 4);
  i += 4;
  memcpy(&out_frame[i], &contact.lastmod, 4);
  i += 4;
  _serial->writeFrame(out_frame, i);
}

void MyMesh::updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len) {
  int i = 0;
  uint8_t code = frame[i++]; // eg. CMD_ADD_UPDATE_CONTACT
  memcpy(contact.id.pub_key, &frame[i], PUB_KEY_SIZE);
  i += PUB_KEY_SIZE;
  contact.type = frame[i++];
  contact.flags = frame[i++];
  contact.out_path_len = frame[i++];
  memcpy(contact.out_path, &frame[i], MAX_PATH_SIZE);
  i += MAX_PATH_SIZE;
  memcpy(contact.name, &frame[i], 32);
  i += 32;
  memcpy(&contact.last_advert_timestamp, &frame[i], 4);
  i += 4;
  if (len >= i + 8) { // optional fields
    memcpy(&contact.gps_lat, &frame[i], 4);
    i += 4;
    memcpy(&contact.gps_lon, &frame[i], 4);
    i += 4;
    if (len >= i + 4) {
      memcpy(&last_mod, &frame[i], 4);
    }
  }
}

bool MyMesh::Frame::isChannelMsg() const {
  return buf[0] == RESP_CODE_CHANNEL_MSG_RECV || buf[0] == RESP_CODE_CHANNEL_MSG_RECV_V3 ||
         buf[0] == RESP_CODE_CHANNEL_DATA_RECV;
}

void MyMesh::addToOfflineQueue(const uint8_t frame[], int len) {
  if (offline_queue_len >= OFFLINE_QUEUE_SIZE) {
    MESH_DEBUG_PRINTLN("WARN: offline_queue is full!");
    int pos = 0;
    while (pos < offline_queue_len) {
      if (offline_queue[pos].isChannelMsg()) {
        for (int i = pos; i < offline_queue_len - 1; i++) { // delete oldest channel msg from queue
          offline_queue[i] = offline_queue[i + 1];
        }
        MESH_DEBUG_PRINTLN("INFO: removed oldest channel message from queue.");
        offline_queue[offline_queue_len - 1].len = len;
        memcpy(offline_queue[offline_queue_len - 1].buf, frame, len);
        return;
      }
      pos++;
    }
    MESH_DEBUG_PRINTLN("INFO: no channel messages to remove from queue.");
  } else {
    offline_queue[offline_queue_len].len = len;
    memcpy(offline_queue[offline_queue_len].buf, frame, len);
    offline_queue_len++;
  }
}

int MyMesh::getFromOfflineQueue(uint8_t frame[]) {
  if (offline_queue_len > 0) {         // check offline queue
    size_t len = offline_queue[0].len; // take from top of queue
    memcpy(frame, offline_queue[0].buf, len);

    offline_queue_len--;
    for (int i = 0; i < offline_queue_len; i++) { // delete top item from queue
      offline_queue[i] = offline_queue[i + 1];
    }
    return len;
  }
  return 0; // queue is empty
}

float MyMesh::getAirtimeBudgetFactor() const {
  return _prefs.airtime_factor;
}

int MyMesh::getInterferenceThreshold() const {
  return 0; // disabled for now, until currentRSSI() problem is resolved
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.5f);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * 0.2f);
  return getRNG()->nextInt(0, 5*t + 1);
}

uint8_t MyMesh::getExtraAckTransmitCount() const {
  return _prefs.multi_acks;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
  if (_serial->isConnected() && len + 3 <= MAX_FRAME_SIZE) {
    int i = 0;
    out_frame[i++] = PUSH_CODE_LOG_RX_DATA;
    out_frame[i++] = (int8_t)(snr * 4);
    out_frame[i++] = (int8_t)(rssi);
    memcpy(&out_frame[i], raw, len);
    i += len;

    _serial->writeFrame(out_frame, i);
  }
}

static void formatMonitorAge(char* dest, size_t dest_size, unsigned long age_secs) {
  if (age_secs <= 180) {
    snprintf(dest, dest_size, "%lus", age_secs);
  } else {
    unsigned long age_mins = age_secs / 60;
    if (age_mins > 999) {
      snprintf(dest, dest_size, ">999");
    } else {
      snprintf(dest, dest_size, "%lum", age_mins);
    }
  }
}

static void formatMonitorSnrX4(char* dest, size_t dest_size, int8_t snr_x4) {
  int value = snr_x4;
  int abs_value = abs(value);
  int whole = abs_value / 4;
  snprintf(dest, dest_size, "%s%d", value < 0 ? "-" : "", whole);
}

void MyMesh::monitorRollActivity() {
  unsigned long now = millis();
  if (monitor_next_activity_rollover == 0) {
    monitor_next_activity_rollover = now + MONITOR_ACTIVITY_BIN_MILLIS;
    return;
  }
  while ((long)(now - monitor_next_activity_rollover) >= 0) {
    monitor_activity_index = (monitor_activity_index + 1) % MONITOR_ACTIVITY_BINS;
    monitor_activity_bins[monitor_activity_index] = 0;
    monitor_airtime_bins[monitor_activity_index] = 0;
    monitor_next_activity_rollover += MONITOR_ACTIVITY_BIN_MILLIS;
  }
}

void MyMesh::rememberMonitorPath(const mesh::Packet* packet) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t display_hops = min((uint8_t)MONITOR_PATH_DISPLAY_HOPS, hash_count);
  char text[MONITOR_PATH_TEXT_SIZE];
  char key[MONITOR_PATH_KEY_SIZE];
  int written = snprintf(text, sizeof(text), "%u ", (unsigned int)hash_count);
  int key_written = snprintf(key, sizeof(key), "%u:", (unsigned int)hash_size);
  size_t key_pos = key_written > 0 ? (size_t)key_written : 0;

  if (written < 0) {
    text[0] = 0;
  } else if (hash_count == 0) {
    snprintf(&text[written], sizeof(text) - written, "-");
  } else {
    size_t pos = (size_t)written;
    const char* hex = "0123456789ABCDEF";
    for (uint8_t n = 0; n < display_hops && pos + 2 < sizeof(text); n++) {
      uint8_t i = hash_count - n;
      if (n > 0 && pos + 1 < sizeof(text)) text[pos++] = ' ';
      const uint8_t* hash = &packet->path[(i - 1) * hash_size];
      for (uint8_t j = 0; j < hash_size && pos + 2 < sizeof(text); j++) {
        text[pos++] = hex[hash[j] >> 4];
        text[pos++] = hex[hash[j] & 0x0F];
      }
    }
    if (hash_count > display_hops && pos + 2 < sizeof(text)) {
      text[pos++] = ' ';
      text[pos++] = '+';
    }
    text[pos] = 0;
  }

  const char* hex = "0123456789ABCDEF";
  uint16_t path_bytes = min((uint16_t)(hash_count * hash_size), (uint16_t)MAX_PATH_SIZE);
  for (uint16_t i = 0; i < path_bytes && key_pos + 2 < sizeof(key); i++) {
    key[key_pos++] = hex[packet->path[i] >> 4];
    key[key_pos++] = hex[packet->path[i] & 0x0F];
  }
  key[key_pos] = 0;

  unsigned long now = millis();
  for (uint8_t i = 0; i < MONITOR_PATH_HISTORY_SIZE; i++) {
    MonitorPathInfo& existing = monitor_paths[i];
    if (existing.seen_at == 0) continue;
    if (strcmp(existing.key, key) == 0) {
      existing.seen_at = now;
      if (existing.count < 0xFFFF) existing.count++;
      return;
    }
  }

  int slot = -1;
  unsigned long oldest_seen = 0xFFFFFFFF;
  uint16_t lowest_count = 0xFFFF;
  for (uint8_t i = 0; i < MONITOR_PATH_HISTORY_SIZE; i++) {
    MonitorPathInfo& item = monitor_paths[i];
    if (item.seen_at == 0) {
      slot = i;
      break;
    }
    if (item.count < lowest_count || (item.count == lowest_count && item.seen_at < oldest_seen)) {
      slot = i;
      lowest_count = item.count;
      oldest_seen = item.seen_at;
    }
  }

  MonitorPathInfo& item = monitor_paths[slot >= 0 ? slot : 0];
  item.seen_at = now;
  item.count = 1;
  StrHelper::strncpy(item.text, text, sizeof(item.text));
  StrHelper::strncpy(item.key, key, sizeof(item.key));
}

void MyMesh::rememberMonitorLastHop(const mesh::Packet* packet, int8_t snr_x4) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  char text[MONITOR_LAST_HOP_TEXT_SIZE];
  char key[MONITOR_LAST_HOP_TEXT_SIZE];
  const char* hex = "0123456789ABCDEF";
  size_t pos = 0;

  if (hash_count == 0) {
    StrHelper::strncpy(text, "-", sizeof(text));
    StrHelper::strncpy(key, "0:-", sizeof(key));
  } else {
    const uint8_t* hash = &packet->path[(hash_count - 1) * hash_size];
    for (uint8_t i = 0; i < hash_size && pos + 2 < sizeof(text); i++) {
      text[pos++] = hex[hash[i] >> 4];
      text[pos++] = hex[hash[i] & 0x0F];
    }
    text[pos] = 0;
    snprintf(key, sizeof(key), "%u:%s", (unsigned int)hash_size, text);
  }

  unsigned long now = millis();
  for (uint8_t i = 0; i < MONITOR_LAST_HOP_HISTORY_SIZE; i++) {
    MonitorLastHopInfo& item = monitor_last_hops[i];
    if (item.seen_at == 0) continue;
    if (strcmp(item.key, key) == 0) {
      item.seen_at = now;
      item.last_snr = snr_x4;
      if (snr_x4 > item.max_snr) item.max_snr = snr_x4;
      return;
    }
  }

  int slot = -1;
  unsigned long oldest_seen = 0xFFFFFFFF;
  for (uint8_t i = 0; i < MONITOR_LAST_HOP_HISTORY_SIZE; i++) {
    MonitorLastHopInfo& item = monitor_last_hops[i];
    if (item.seen_at == 0) {
      slot = i;
      break;
    }
    if (item.seen_at < oldest_seen) {
      slot = i;
      oldest_seen = item.seen_at;
    }
  }

  MonitorLastHopInfo& item = monitor_last_hops[slot >= 0 ? slot : 0];
  item.seen_at = now;
  item.last_snr = snr_x4;
  item.max_snr = snr_x4;
  StrHelper::strncpy(item.text, text, sizeof(item.text));
  StrHelper::strncpy(item.key, key, sizeof(item.key));
}

void MyMesh::logRx(mesh::Packet* packet, int len, float score) {
  (void)score;
  monitorRollActivity();
  monitor_rx_packets++;
  monitor_activity_bins[monitor_activity_index]++;
  uint32_t airtime_ms = _radio->getEstAirtimeFor(len);
  uint32_t airtime_bin = (uint32_t)monitor_airtime_bins[monitor_activity_index] + airtime_ms;
  monitor_airtime_bins[monitor_activity_index] = airtime_bin > UINT16_MAX ? UINT16_MAX : (uint16_t)airtime_bin;
  monitor_last_snr_x4 = (int8_t)(packet->getSNR() * 4);
  notePacketScope(packet);
  rememberMonitorPath(packet);
  rememberMonitorLastHop(packet, monitor_last_snr_x4);
}

bool MyMesh::getMonitorPathLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  uint8_t found = 0;
  bool selected[MONITOR_PATH_HISTORY_SIZE];
  memset(selected, 0, sizeof(selected));

  for (uint8_t i = 0; i < MONITOR_PATH_HISTORY_SIZE; i++) {
    int best = -1;
    uint16_t best_count = 0;
    unsigned long best_seen = 0;

    for (uint8_t j = 0; j < MONITOR_PATH_HISTORY_SIZE; j++) {
      const MonitorPathInfo& item = monitor_paths[j];
      if (item.seen_at == 0 || selected[j]) continue;

      if (item.count > best_count || (item.count == best_count && item.seen_at > best_seen)) {
        best = j;
        best_count = item.count;
        best_seen = item.seen_at;
      }
    }

    if (best < 0) return false;
    selected[best] = true;
    if (found == index) {
      const MonitorPathInfo& item = monitor_paths[best];
      snprintf(dest, dest_size, "%s", item.text);
      return true;
    }
    found++;
  }
  return false;
}

bool MyMesh::getMonitorPathMetaLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  uint8_t found = 0;
  unsigned long now = millis();
  bool selected[MONITOR_PATH_HISTORY_SIZE];
  memset(selected, 0, sizeof(selected));

  for (uint8_t i = 0; i < MONITOR_PATH_HISTORY_SIZE; i++) {
    int best = -1;
    uint16_t best_count = 0;
    unsigned long best_seen = 0;

    for (uint8_t j = 0; j < MONITOR_PATH_HISTORY_SIZE; j++) {
      const MonitorPathInfo& item = monitor_paths[j];
      if (item.seen_at == 0 || selected[j]) continue;

      if (item.count > best_count || (item.count == best_count && item.seen_at > best_seen)) {
        best = j;
        best_count = item.count;
        best_seen = item.seen_at;
      }
    }

    if (best < 0) return false;
    selected[best] = true;
    if (found == index) {
      const MonitorPathInfo& item = monitor_paths[best];
      char count_col[6];
      char age_col[5];
      unsigned long age_secs = (now - item.seen_at) / 1000;
      if (item.count > 999) {
        snprintf(count_col, sizeof(count_col), "999+");
      } else {
        snprintf(count_col, sizeof(count_col), "%u", (unsigned int)item.count);
      }
      formatMonitorAge(age_col, sizeof(age_col), age_secs);
      snprintf(dest, dest_size, "cnt %s  age %s", count_col, age_col);
      return true;
    }
    found++;
  }
  return false;
}

bool MyMesh::getMonitorLatestPathLine(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  int best = -1;
  unsigned long best_seen = 0;
  for (uint8_t i = 0; i < MONITOR_PATH_HISTORY_SIZE; i++) {
    const MonitorPathInfo& item = monitor_paths[i];
    if (item.seen_at == 0) continue;
    if (item.seen_at > best_seen) {
      best = i;
      best_seen = item.seen_at;
    }
  }

  if (best < 0) return false;

  const MonitorPathInfo& item = monitor_paths[best];
  char age_col[5];
  unsigned long age_secs = (millis() - item.seen_at) / 1000;
  formatMonitorAge(age_col, sizeof(age_col), age_secs);
  snprintf(dest, dest_size, "%s %s", age_col, item.text);
  return true;
}

bool MyMesh::getMonitorLastHopLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  uint8_t found = 0;
  unsigned long now = millis();
  bool selected[MONITOR_LAST_HOP_HISTORY_SIZE];
  memset(selected, 0, sizeof(selected));

  for (uint8_t i = 0; i < MONITOR_LAST_HOP_HISTORY_SIZE; i++) {
    int best = -1;
    unsigned long best_seen = 0;

    for (uint8_t j = 0; j < MONITOR_LAST_HOP_HISTORY_SIZE; j++) {
      const MonitorLastHopInfo& item = monitor_last_hops[j];
      if (item.seen_at == 0 || selected[j]) continue;

      if (item.seen_at > best_seen) {
        best = j;
        best_seen = item.seen_at;
      }
    }

    if (best < 0) return false;
    selected[best] = true;
    if (found == index) {
      const MonitorLastHopInfo& item = monitor_last_hops[best];
      char age_col[5];
      char max_col[8];
      char last_col[8];
      unsigned long age_secs = (now - item.seen_at) / 1000;
      formatMonitorAge(age_col, sizeof(age_col), age_secs);
      formatMonitorSnrX4(max_col, sizeof(max_col), item.max_snr);
      formatMonitorSnrX4(last_col, sizeof(last_col), item.last_snr);
      snprintf(dest, dest_size, "%6s %s %s/%s", item.text, age_col, max_col, last_col);
      return true;
    }
    found++;
  }
  return false;
}

uint8_t MyMesh::getMonitorActivity(uint16_t* dest, uint8_t max_count) {
  if (!dest || max_count == 0) return 0;
  monitorRollActivity();
  uint8_t count = min(max_count, (uint8_t)MONITOR_ACTIVITY_BINS);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (monitor_activity_index + MONITOR_ACTIVITY_BINS - count + 1 + i) % MONITOR_ACTIVITY_BINS;
    dest[i] = monitor_activity_bins[idx];
  }
  return count;
}

uint8_t MyMesh::getMonitorAirtime(uint16_t* dest, uint8_t max_count) {
  if (!dest || max_count == 0) return 0;
  monitorRollActivity();
  uint8_t count = min(max_count, (uint8_t)MONITOR_ACTIVITY_BINS);
  for (uint8_t i = 0; i < count; i++) {
    uint8_t idx = (monitor_activity_index + MONITOR_ACTIVITY_BINS - count + 1 + i) % MONITOR_ACTIVITY_BINS;
    dest[i] = monitor_airtime_bins[idx];
  }
  return count;
}

bool MyMesh::isAutoAddEnabled() const {
  return (_prefs.manual_add_contacts & 1) == 0;
}

bool MyMesh::shouldAutoAddContactType(uint8_t contact_type) const {
  if ((_prefs.manual_add_contacts & 1) == 0) {
    return true;
  }

  uint8_t type_bit = 0;
  switch (contact_type) {
    case ADV_TYPE_CHAT:
      type_bit = AUTO_ADD_CHAT;
      break;
    case ADV_TYPE_REPEATER:
      type_bit = AUTO_ADD_REPEATER;
      break;
    case ADV_TYPE_ROOM:
      type_bit = AUTO_ADD_ROOM_SERVER;
      break;
    case ADV_TYPE_SENSOR:
      type_bit = AUTO_ADD_SENSOR;
      break;
    default:
      return false;  // Unknown type, don't auto-add
  }

  return (_prefs.autoadd_config & type_bit) != 0;
}

bool MyMesh::shouldOverwriteWhenFull() const {
  return (_prefs.autoadd_config & AUTO_ADD_OVERWRITE_OLDEST) != 0;
}

uint8_t MyMesh::getAutoAddMaxHops() const {
  return _prefs.autoadd_max_hops;
}

void MyMesh::onContactOverwrite(const uint8_t* pub_key) {
    _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE); // delete from storage
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACT_DELETED;
    memcpy(&out_frame[1], pub_key, PUB_KEY_SIZE);
    _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
  }
}

void MyMesh::onContactsFull() {
  if (_serial->isConnected()) {
    out_frame[0] = PUSH_CODE_CONTACTS_FULL;
    _serial->writeFrame(out_frame, 1);
  }
}

static uint8_t reverseMonitorPath(uint8_t* dest, const uint8_t* src, uint8_t path_len) {
  if (!dest || !src || !mesh::Packet::isValidPathLen(path_len)) return OUT_PATH_UNKNOWN;
  uint8_t hash_size = (path_len >> 6) + 1;
  uint8_t hash_count = path_len & 63;
  for (uint8_t i = 0; i < hash_count; i++) {
    memcpy(&dest[i * hash_size], &src[(hash_count - 1 - i) * hash_size], hash_size);
  }
  return path_len;
}

void MyMesh::onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) {
  if (_serial->isConnected()) {
    if (is_new) {
      writeContactRespFrame(PUSH_CODE_NEW_ADVERT, contact);
    } else {
      out_frame[0] = PUSH_CODE_ADVERT;
      memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
      _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE);
    }
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::newContactMessage);
#endif
  }

  // add inbound-path to mem cache
  if (path && mesh::Packet::isValidPathLen(path_len)) {  // check path is valid
    rememberScopeTarget(contact, path_len, path);

    if (contact.type == ADV_TYPE_REPEATER) {
      contact.out_path_len = reverseMonitorPath(contact.out_path, path, path_len);
    }

    AdvertPath* p = advert_paths;
    uint32_t oldest = 0xFFFFFFFF;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {   // check if already in table, otherwise evict oldest
      if (memcmp(advert_paths[i].pubkey_prefix, contact.id.pub_key, sizeof(AdvertPath::pubkey_prefix)) == 0) {
        p = &advert_paths[i];   // found
        break;
      }
      if (advert_paths[i].recv_timestamp < oldest) {
        oldest = advert_paths[i].recv_timestamp;
        p = &advert_paths[i];
      }
    }

    memcpy(p->pubkey_prefix, contact.id.pub_key, sizeof(p->pubkey_prefix));
    strcpy(p->name, contact.name);
    p->recv_timestamp = getRTCClock()->getCurrentTime();
    p->path_len = mesh::Packet::copyPath(p->path, path, path_len);
  }

  if (!is_new) dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY); // only schedule lazy write for contacts that are in contacts[]
}

static int sort_by_recent(const void *a, const void *b) {
  return ((AdvertPath *) b)->recv_timestamp - ((AdvertPath *) a)->recv_timestamp;
}

static int sort_scope_by_recent(const void *a, const void *b) {
  const ScopeInfo* sa = (const ScopeInfo *) a;
  const ScopeInfo* sb = (const ScopeInfo *) b;
  const bool a_rx = sa->rx_count > 0;
  const bool b_rx = sb->rx_count > 0;
  if (a_rx != b_rx) return b_rx ? 1 : -1;
  return sb->seen_timestamp - sa->seen_timestamp;
}

int MyMesh::getRecentlyHeard(AdvertPath dest[], int max_num) {
  if (max_num > ADVERT_PATH_TABLE_SIZE) max_num = ADVERT_PATH_TABLE_SIZE;
  qsort(advert_paths, ADVERT_PATH_TABLE_SIZE, sizeof(advert_paths[0]), sort_by_recent);

  for (int i = 0; i < max_num; i++) {
    dest[i] = advert_paths[i];
  }
  return max_num;
}

int MyMesh::getQuickSendTargets(QuickSendTarget dest[], int max_num) {
  if (!dest || max_num <= 0) return 0;

  int count = 0;
#ifdef MAX_GROUP_CHANNELS
  for (int i = 0; i < MAX_GROUP_CHANNELS && count < max_num; i++) {
    ChannelDetails channel;
    if (getChannel(i, channel) && channel.name[0]) {
      QuickSendTarget* t = &dest[count++];
      memset(t, 0, sizeof(*t));
      t->type = QUICK_SEND_CHANNEL;
      t->index = i;
      StrHelper::strncpy(t->name, channel.name, sizeof(t->name));
    }
  }
#endif

  AdvertPath recent[ADVERT_PATH_TABLE_SIZE];
  int recent_count = getRecentlyHeard(recent, ADVERT_PATH_TABLE_SIZE);
  for (int i = 0; i < recent_count && count < max_num; i++) {
    if (recent[i].name[0] == 0 || recent[i].recv_timestamp == 0) continue;
    bool duplicate = false;
    for (int j = 0; j < count; j++) {
      if (dest[j].type == QUICK_SEND_CONTACT &&
          memcmp(dest[j].pubkey_prefix, recent[i].pubkey_prefix, sizeof(dest[j].pubkey_prefix)) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    QuickSendTarget* t = &dest[count++];
    memset(t, 0, sizeof(*t));
    t->type = QUICK_SEND_CONTACT;
    t->index = i;
    memcpy(t->pubkey_prefix, recent[i].pubkey_prefix, sizeof(t->pubkey_prefix));
    StrHelper::strncpy(t->name, recent[i].name, sizeof(t->name));
  }

  return count;
}

bool MyMesh::sendQuickText(const QuickSendTarget& target, const char* text, bool* sent_flood) {
  if (sent_flood) *sent_flood = false;
  if (!text || !text[0]) return false;

  uint32_t msg_timestamp = getRTCClock()->getCurrentTimeUnique();
  if (target.type == QUICK_SEND_CHANNEL) {
    ChannelDetails channel;
    if (!getChannel(target.index, channel) || channel.name[0] == 0) return false;
    if (!sendGroupMessage(msg_timestamp, channel.channel, _prefs.node_name, text, strlen(text))) return false;

    int i = 0;
    if (app_target_ver >= 3) {
      out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
      out_frame[i++] = 0; // local echo has no RX SNR
      out_frame[i++] = 0;
      out_frame[i++] = 0;
    } else {
      out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
    }

    out_frame[i++] = target.index;
    out_frame[i++] = 0xFF; // local / direct-to-channel echo
    out_frame[i++] = TXT_TYPE_PLAIN;
    memcpy(&out_frame[i], &msg_timestamp, 4);
    i += 4;

    char local_text[MAX_TEXT_LEN + 1];
    snprintf(local_text, sizeof(local_text), "%s: %s", _prefs.node_name, text);
    int tlen = strlen(local_text);
    if (i + tlen > MAX_FRAME_SIZE) tlen = MAX_FRAME_SIZE - i;
    memcpy(&out_frame[i], local_text, tlen);
    i += tlen;
    addToOfflineQueue(out_frame, i);

    if (_serial->isConnected()) {
      uint8_t frame[1];
      frame[0] = PUSH_CODE_MSG_WAITING;
      _serial->writeFrame(frame, 1);
    }
    return true;
  }

  ContactInfo* recipient = lookupContactByPubKey(target.pubkey_prefix, sizeof(target.pubkey_prefix));
  if (!recipient) return false;

  uint32_t expected_ack = 0;
  uint32_t est_timeout = 0;
  int result = sendMessage(*recipient, msg_timestamp, 0, text, expected_ack, est_timeout);
  if (result == MSG_SEND_FAILED) return false;

  if (sent_flood) *sent_flood = (result == MSG_SEND_SENT_FLOOD);
  if (expected_ack) {
    expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis();
    expected_ack_table[next_ack_idx].ack = expected_ack;
    expected_ack_table[next_ack_idx].contact = recipient;
    next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
  }
  return true;
}

bool MyMesh::sendQuickReply(const uint8_t* pubkey_prefix, uint8_t prefix_len, const char* text, bool* sent_flood) {
  if (sent_flood) *sent_flood = false;
  if (!pubkey_prefix || prefix_len == 0 || !text || !text[0]) return false;

  ContactInfo* recipient = lookupContactByPubKey(pubkey_prefix, prefix_len);
  if (!recipient) return false;

  uint32_t expected_ack = 0;
  uint32_t est_timeout = 0;
  int result = sendMessage(*recipient, getRTCClock()->getCurrentTimeUnique(), 0, text, expected_ack, est_timeout);
  if (result == MSG_SEND_FAILED) return false;

  if (sent_flood) *sent_flood = (result == MSG_SEND_SENT_FLOOD);
  if (expected_ack) {
    expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis();
    expected_ack_table[next_ack_idx].ack = expected_ack;
    expected_ack_table[next_ack_idx].contact = recipient;
    next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
  }
  return true;
}

bool MyMesh::sendQuickChannelReply(uint8_t channel_idx, const char* mention, const char* text) {
#ifdef MAX_GROUP_CHANNELS
  if (!text || !text[0]) return false;

  ChannelDetails channel;
  if (!getChannel(channel_idx, channel) || channel.name[0] == 0) return false;

  char reply[MAX_TEXT_LEN + 1];
  if (mention && mention[0]) {
    snprintf(reply, sizeof(reply), "@%s %s", mention, text);
  } else {
    snprintf(reply, sizeof(reply), "%s", text);
  }
  return sendGroupMessage(getRTCClock()->getCurrentTimeUnique(), channel.channel, _prefs.node_name,
                          reply, strlen(reply));
#else
  return false;
#endif
}

uint8_t MyMesh::queryNearbyScopes() {
  uint8_t sent = 0;
  const uint8_t max_pending = (uint8_t)(sizeof(pending_scope_req) / sizeof(pending_scope_req[0]));
  memset(pending_scope_req, 0, sizeof(pending_scope_req));
  last_scope_scan_sent = 0;
  sendScopeDiscoverReq();

  AdvertPath recent[ADVERT_PATH_TABLE_SIZE];
  int recent_count = getRecentlyHeard(recent, ADVERT_PATH_TABLE_SIZE);
  uint8_t request[2];
  request[0] = ANON_REQ_TYPE_REGIONS;
  request[1] = 0; // reply path: zero-hop back to us; intended for nearby repeaters

  for (uint8_t i = 0; i < SCOPE_TARGET_SIZE && sent < max_pending; i++) {
    if (scope_targets[i].seen_timestamp == 0) continue;
    ContactInfo target;
    memset(&target, 0, sizeof(target));
    target.id = scope_targets[i].id;
    StrHelper::strncpy(target.name, scope_targets[i].name, sizeof(target.name));
    target.type = ADV_TYPE_REPEATER;
    target.out_path_len = mesh::Packet::copyPath(target.out_path, scope_targets[i].path, scope_targets[i].path_len);
    target.shared_secret_valid = false;
    sendScopeRequest(target, request, sizeof(request), sent);
  }

  for (uint32_t i = 0; i < getNumContacts() && sent < max_pending; i++) {
    ContactInfo contact;
    if (!getContactByIdx(i, contact) || contact.type != ADV_TYPE_REPEATER) continue;
    if (contact.out_path_len == OUT_PATH_UNKNOWN) continue;

    bool duplicate = false;
    for (int r = 0; r < recent_count; r++) {
      if (recent[r].recv_timestamp == 0) continue;
      if (memcmp(recent[r].pubkey_prefix, contact.id.pub_key, sizeof(recent[r].pubkey_prefix)) == 0) {
        duplicate = true;
        break;
      }
    }
    if (duplicate) continue;

    sendScopeRequest(contact, request, sizeof(request), sent);
  }
  last_scope_scan_sent = sent;
  return sent;
}

void MyMesh::sendScopeDiscoverReq() {
  uint8_t data[10];
  data[0] = CTL_TYPE_NODE_DISCOVER_REQ; // full pubkey response
  data[1] = (1 << ADV_TYPE_REPEATER);
  getRNG()->random(&data[2], 4);
  memcpy(&pending_scope_discover_tag, &data[2], 4);
  uint32_t since = 0;
  memcpy(&data[6], &since, 4);

  mesh::Packet* pkt = createControlData(data, sizeof(data));
  if (pkt) {
    sendZeroHop(pkt);
  }
}

bool MyMesh::sendScopeRequest(ContactInfo& recipient, const uint8_t* request, size_t request_len, uint8_t& sent) {
  if (!request || request_len == 0 || sent >= (uint8_t)(sizeof(pending_scope_req) / sizeof(pending_scope_req[0]))) return false;
  ContactInfo* stored = ensureScopeContact(recipient);
  if (!stored) return false;
  if (recipient.out_path_len != OUT_PATH_UNKNOWN) {
    stored->out_path_len = mesh::Packet::copyPath(stored->out_path, recipient.out_path, recipient.out_path_len);
  }
  uint32_t tag = 0;
  uint32_t est_timeout = 0;
  int result = sendAnonReq(*stored, request, (uint8_t)request_len, tag, est_timeout);
  if (result == MSG_SEND_FAILED) return false;
  pending_scope_req[sent++] = tag;
  return true;
}

ContactInfo* MyMesh::ensureScopeContact(const ContactInfo& candidate) {
  ContactInfo* stored = lookupContactByPubKey(candidate.id.pub_key, PUB_KEY_SIZE);
  if (stored) return stored;

  ContactInfo add = candidate;
  if (add.name[0] == 0) {
    snprintf(add.name, sizeof(add.name), "%02X%02X%02X%02X",
             add.id.pub_key[0], add.id.pub_key[1], add.id.pub_key[2], add.id.pub_key[3]);
  }
  add.type = ADV_TYPE_REPEATER;
  add.flags = 0;
  add.lastmod = getRTCClock()->getCurrentTime();
  add.last_advert_timestamp = add.lastmod;
  add.shared_secret_valid = false;
  if (!addContact(add)) return nullptr;
  return lookupContactByPubKey(candidate.id.pub_key, PUB_KEY_SIZE);
}

uint8_t MyMesh::getScopeInfo(ScopeInfo dest[], uint8_t max_count) const {
  if (!dest || max_count == 0) return 0;

  ScopeInfo sorted[SCOPE_CACHE_SIZE];
  memcpy(sorted, scope_cache, sizeof(sorted));
  qsort(sorted, SCOPE_CACHE_SIZE, sizeof(sorted[0]), sort_scope_by_recent);

  uint8_t count = 0;
  for (uint8_t i = 0; i < SCOPE_CACHE_SIZE && count < max_count; i++) {
    if (sorted[i].name[0] == 0 || sorted[i].seen_timestamp == 0) continue;
    dest[count++] = sorted[i];
  }
  return count;
}

void MyMesh::formatPacketScope(const mesh::Packet* packet, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;
  dest[0] = 0;
  if (!packet || !packet->hasTransportCodes()) {
    StrHelper::strncpy(dest, "none", dest_size);
    return;
  }

  TransportKey key;
  memcpy(key.key, _prefs.default_scope_key, sizeof(key.key));
  if (!key.isNull() && packet->transport_codes[0] == key.calcTransportCode(packet)) {
    if (_prefs.default_scope_name[0]) {
      snprintf(dest, dest_size, "%s", _prefs.default_scope_name);
    } else {
      snprintf(dest, dest_size, "default");
    }
    return;
  }

  TransportKeyStore temp;
  for (uint8_t i = 0; i < SCOPE_CACHE_SIZE; i++) {
    if (scope_cache[i].name[0] == 0) continue;
    char auto_name[SCOPE_NAME_SIZE + 1];
    snprintf(auto_name, sizeof(auto_name), "#%s", scope_cache[i].name);
    temp.getAutoKeyFor(i + 1, auto_name, key);
    if (packet->transport_codes[0] == key.calcTransportCode(packet)) {
      snprintf(dest, dest_size, "%s", scope_cache[i].name);
      return;
    }
  }

  snprintf(dest, dest_size, "0x%04X", (unsigned int)packet->transport_codes[0]);
}

bool MyMesh::isMonitorHeatstripRequest(const char* text) const {
  if (!text) return false;
  while (*text == ' ' || *text == '\t') text++;
  return strcmp(text, "SHOWPATHS") == 0;
}

bool MyMesh::isDisplayableText(const char* text) const {
  if (!text || !text[0]) return false;

  bool has_printable = false;
  for (size_t i = 0; i < MAX_TEXT_LEN && text[i]; i++) {
    uint8_t c = (uint8_t)text[i];
    if (c >= 0x20) {
      has_printable = true;
      continue;
    }
    if (c == '\r' || c == '\n' || c == '\t') continue;
    return false;
  }
  return has_printable;
}

static bool companionNextPathToken(char*& cursor, char* dest, size_t dest_size) {
  if (!cursor || !dest || dest_size == 0) return false;
  while (*cursor == ' ') cursor++;
  if (*cursor == 0) return false;
  size_t pos = 0;
  while (*cursor && *cursor != ' ') {
    if (pos + 1 < dest_size) dest[pos++] = *cursor;
    cursor++;
  }
  dest[pos] = 0;
  return pos > 0;
}

void MyMesh::formatMonitorHeatstripReply(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;
  dest[0] = 0;

  static const uint8_t HEAT_PATHS = 8;
  char line[80];
  if (!getMonitorPathLine(0, line, sizeof(line))) {
    snprintf(dest, dest_size, "Paths:\nNo RX paths");
    return;
  }

  int written = snprintf(dest, dest_size, "Paths:");
  if (written < 0) return;
  size_t pos = min((size_t)written, dest_size - 1);
  for (uint8_t i = 0; i < HEAT_PATHS && pos + 1 < dest_size; i++) {
    if (!getMonitorPathLine(i, line, sizeof(line))) break;

    char work[80];
    snprintf(work, sizeof(work), "%s", line);
    char* cursor = work;
    char count[8];
    if (!companionNextPathToken(cursor, count, sizeof(count))) continue;

    char path[48] = "";
    char token[12];
    while (companionNextPathToken(cursor, token, sizeof(token))) {
      if (strcmp(token, "+") == 0) continue;
      if (path[0]) strncat(path, " ", sizeof(path) - strlen(path) - 1);
      strncat(path, token, sizeof(path) - strlen(path) - 1);
    }
    if (!path[0]) StrHelper::strncpy(path, "-", sizeof(path));

    int n = snprintf(&dest[pos], dest_size - pos, "\n%u (%s): %s",
                     (unsigned int)i + 1, count, path);
    if (n < 0) break;
    if ((size_t)n >= dest_size - pos) {
      dest[dest_size - 1] = 0;
      break;
    }
    pos += (size_t)n;
  }
}

bool MyMesh::sendMonitorHeatstripReply(const ContactInfo& recipient) {
  char reply[MAX_TEXT_LEN + 1];
  formatMonitorHeatstripReply(reply, sizeof(reply));
  uint32_t expected_ack = 0;
  uint32_t est_timeout = 0;
  return sendMessage(recipient, getRTCClock()->getCurrentTimeUnique(), 0, reply, expected_ack, est_timeout) != MSG_SEND_FAILED;
}

void MyMesh::rememberScopeName(const char* name, const char* source) {
  if (!name || !name[0] || strcmp(name, "*") == 0) return;
  while (*name == ' ' || *name == '#') name++;
  if (!name[0]) return;

  char clean[SCOPE_NAME_SIZE];
  size_t len = 0;
  while (name[len] && name[len] != ',' && name[len] != '\n' && name[len] != '\r' && len + 1 < sizeof(clean)) {
    clean[len] = name[len];
    len++;
  }
  while (len > 0 && clean[len - 1] == ' ') len--;
  clean[len] = 0;
  if (!clean[0] || strcmp(clean, "*") == 0) return;

  ScopeInfo* slot = nullptr;
  ScopeInfo* empty = nullptr;
  ScopeInfo* oldest_zero_rx = nullptr;
  uint32_t oldest_zero_rx_seen = 0xFFFFFFFF;
  for (uint8_t i = 0; i < SCOPE_CACHE_SIZE; i++) {
    if (strcmp(scope_cache[i].name, clean) == 0) {
      slot = &scope_cache[i];
      break;
    }
    if (scope_cache[i].name[0] == 0 && !empty) {
      empty = &scope_cache[i];
    } else if (scope_cache[i].rx_count == 0 && scope_cache[i].seen_timestamp < oldest_zero_rx_seen) {
      oldest_zero_rx_seen = scope_cache[i].seen_timestamp;
      oldest_zero_rx = &scope_cache[i];
    }
  }
  if (!slot) slot = empty ? empty : oldest_zero_rx;
  if (!slot) return;

  if (slot->name[0] == 0 || strcmp(slot->name, clean) != 0) {
    memset(slot, 0, sizeof(*slot));
  }
  StrHelper::strncpy(slot->name, clean, sizeof(slot->name));
  StrHelper::strncpy(slot->source, source && source[0] ? source : "repeater", sizeof(slot->source));
  slot->seen_timestamp = getRTCClock()->getCurrentTime();
}

void MyMesh::noteScopeRx(const char* name, const char* source) {
  if (!name || !name[0]) return;

  ScopeInfo* slot = nullptr;
  uint32_t oldest = 0xFFFFFFFF;
  for (uint8_t i = 0; i < SCOPE_CACHE_SIZE; i++) {
    if (strcmp(scope_cache[i].name, name) == 0) {
      slot = &scope_cache[i];
      break;
    }
    if (scope_cache[i].seen_timestamp < oldest) {
      oldest = scope_cache[i].seen_timestamp;
      slot = &scope_cache[i];
    }
  }
  if (!slot) return;

  if (slot->name[0] == 0 || strcmp(slot->name, name) != 0) {
    memset(slot, 0, sizeof(*slot));
    StrHelper::strncpy(slot->name, name, sizeof(slot->name));
    StrHelper::strncpy(slot->source, source && source[0] ? source : "rx", sizeof(slot->source));
  } else if (source && source[0] && strcmp(slot->source, "rx") == 0) {
    StrHelper::strncpy(slot->source, source, sizeof(slot->source));
  }
  slot->seen_timestamp = getRTCClock()->getCurrentTime();
  if (slot->rx_count < UINT32_MAX) slot->rx_count++;
}

void MyMesh::notePacketScope(const mesh::Packet* packet) {
  char scope[32];
  formatPacketScope(packet, scope, sizeof(scope));
  noteScopeRx(scope, strcmp(scope, "none") == 0 ? "rx" : "scope");
}

void MyMesh::rememberScopeTarget(const ContactInfo& contact, uint8_t path_len, const uint8_t* path) {
  if (contact.type != ADV_TYPE_REPEATER || !path || !mesh::Packet::isValidPathLen(path_len)) return;

  ScopeTarget* slot = nullptr;
  uint32_t oldest = 0xFFFFFFFF;
  for (uint8_t i = 0; i < SCOPE_TARGET_SIZE; i++) {
    if (scope_targets[i].seen_timestamp > 0 && contact.id.matches(scope_targets[i].id)) {
      slot = &scope_targets[i];
      break;
    }
    if (scope_targets[i].seen_timestamp < oldest) {
      oldest = scope_targets[i].seen_timestamp;
      slot = &scope_targets[i];
    }
  }
  if (!slot) return;

  slot->id = contact.id;
  StrHelper::strncpy(slot->name, contact.name, sizeof(slot->name));
  slot->path_len = reverseMonitorPath(slot->path, path, path_len);
  slot->seen_timestamp = getRTCClock()->getCurrentTime();
}

void MyMesh::ingestScopeNames(const ContactInfo& contact, const char* names) {
  if (!names || !names[0]) return;

  char work[120];
  StrHelper::strncpy(work, names, sizeof(work));
  char* cursor = work;
  while (*cursor) {
    while (*cursor == ',' || *cursor == ' ' || *cursor == '\n' || *cursor == '\r') cursor++;
    if (!*cursor) break;
    char* start = cursor;
    while (*cursor && *cursor != ',') cursor++;
    if (*cursor) *cursor++ = 0;
    rememberScopeName(start, contact.name);
  }
}

bool MyMesh::isPendingScopeTag(uint32_t tag) {
  for (uint8_t i = 0; i < (uint8_t)(sizeof(pending_scope_req) / sizeof(pending_scope_req[0])); i++) {
    if (pending_scope_req[i] == tag) {
      pending_scope_req[i] = 0;
      return true;
    }
  }
  return false;
}

static bool readTextLine(File& file, char* dest, size_t dest_size) {
  if (!dest || dest_size == 0) return false;
  size_t pos = 0;
  bool got = false;
  while (file.available()) {
    char c = (char)file.read();
    if (c == '\r') continue;
    if (c == '\n') break;
    got = true;
    if (pos + 1 < dest_size) dest[pos++] = c;
  }
  dest[pos] = 0;
  return got || pos > 0;
}

void MyMesh::resetQuickMessages() {
  static const char* defaults[] = {
    "Bin QRV",
    "Bin unterwegs",
    "Komme gleich",
    "Bitte wiederholen",
    "Alles ok",
    "Danke",
    "Bitte anrufen.",
    "Test aus Gifhorn",
    "Test aus Wolfsburg"
  };
  for (uint8_t i = 0; i < QUICK_MESSAGE_SLOTS; i++) {
    StrHelper::strncpy(quick_messages[i], defaults[i], sizeof(quick_messages[i]));
  }
}

void MyMesh::loadQuickMessages() {
  resetQuickMessages();
  if (!_store) return;

  File file = _store->openRead(QUICK_MESSAGES_FILE);
  if (!file) return;
  for (uint8_t i = 0; i < QUICK_MESSAGE_SLOTS; i++) {
    if (!readTextLine(file, quick_messages[i], sizeof(quick_messages[i]))) break;
  }
  file.close();
}

bool MyMesh::saveQuickMessages() {
  if (!_store) return false;
  File file = _store->openWriteFile(QUICK_MESSAGES_FILE);
  if (!file) return false;
  for (uint8_t i = 0; i < QUICK_MESSAGE_SLOTS; i++) {
    file.print(quick_messages[i]);
    file.print('\n');
  }
  file.close();
  return true;
}

uint8_t MyMesh::getQuickMessageCount() const {
  return QUICK_MESSAGE_SLOTS;
}

const char* MyMesh::getQuickMessage(uint8_t index) const {
  if (index >= QUICK_MESSAGE_SLOTS) return "";
  return quick_messages[index];
}

void MyMesh::printQuickMessages() {
  for (uint8_t i = 0; i < QUICK_MESSAGE_SLOTS; i++) {
    Serial.printf("  %u: %s\n", (unsigned int)i + 1, quick_messages[i][0] ? quick_messages[i] : "<leer>");
  }
  Serial.println("  gps: Meine Position ist: <lat>, <lon>");
}

void MyMesh::onContactPathUpdated(const ContactInfo &contact) {
  out_frame[0] = PUSH_CODE_PATH_UPDATED;
  memcpy(&out_frame[1], contact.id.pub_key, PUB_KEY_SIZE);
  _serial->writeFrame(out_frame, 1 + PUB_KEY_SIZE); // NOTE: app may not be connected

  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
}

ContactInfo*  MyMesh::processAck(const uint8_t *data) {
  // see if matches any in a table
  for (int i = 0; i < EXPECTED_ACK_TABLE_SIZE; i++) {
    if (memcmp(data, &expected_ack_table[i].ack, 4) == 0) { // got an ACK from recipient
      out_frame[0] = PUSH_CODE_SEND_CONFIRMED;
      memcpy(&out_frame[1], data, 4);
      uint32_t trip_time = _ms->getMillis() - expected_ack_table[i].msg_sent;
      memcpy(&out_frame[5], &trip_time, 4);
      _serial->writeFrame(out_frame, 9);

      // NOTE: the same ACK can be received multiple times!
      expected_ack_table[i].ack = 0; // clear expected hash, now that we have received ACK
      return expected_ack_table[i].contact;
    }
  }
  return checkConnectionsAck(data);
}

void MyMesh::queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt,
                          uint32_t sender_timestamp, const uint8_t *extra, int extra_len, const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CONTACT_MSG_RECV;
  }
  memcpy(&out_frame[i], from.id.pub_key, 6);
  i += 6; // just 6-byte prefix
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = txt_type;
  memcpy(&out_frame[i], &sender_timestamp, 4);
  i += 4;
  if (extra_len > 0) {
    memcpy(&out_frame[i], extra, extra_len);
    i += extra_len;
  }
  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }

#ifdef DISPLAY_CLASS
  // we only want to show text messages on display, not cli data
  bool should_display = (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_SIGNED_PLAIN) &&
                        isDisplayableText(text);
  if (should_display && _ui) {
    uint8_t display_path_len = path_len == 0xFF ? 0xFF : pkt->getPathHashCount();
    char scope[32];
    formatPacketScope(pkt, scope, sizeof(scope));
    _ui->newMsg(display_path_len, from.name, from.id.pub_key, 0xFF, nullptr, scope, text, offline_queue_len);
    if (!_serial->isConnected()) {
      _ui->notify(UIEventType::contactMessage);
    }
  }
#endif
}

bool MyMesh::filterRecvFloodPacket(mesh::Packet* packet) {
  // REVISIT: try to determine which Region (from transport_codes[1]) that Sender is indicating for replies/responses
  //    if unknown, fallback to finding Region from transport_codes[0], the 'scope' used by Sender
  return false;
}

bool MyMesh::allowPacketForward(const mesh::Packet* packet) {
  return _prefs.client_repeat != 0;
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, _prefs.path_hash_mode + 1);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, _prefs.path_hash_mode + 1);
  }
}

void MyMesh::sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: dynamic send_scope, depending on recipient and current 'home' Region
  TransportKey default_scope;
  memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

  auto scope = send_scope.isNull() ? &default_scope : &send_scope;
  sendFloodScoped(*scope, pkt, delay_millis);
}
void MyMesh::sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis) {
  // TODO: have per-channel send_scope
  TransportKey default_scope;
  memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));

  auto scope = send_scope.isNull() ? &default_scope : &send_scope;
  sendFloodScoped(*scope, pkt, delay_millis);
}

void MyMesh::onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  if (isMonitorHeatstripRequest(text)) {
    sendMonitorHeatstripReply(from);
    return;
  }
  if (!isDisplayableText(text)) {
    MESH_DEBUG_PRINTLN("onMessageRecv: dropping non-displayable text payload");
    return;
  }
  queueMessage(from, TXT_TYPE_PLAIN, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                               const char *text) {
  markConnectionActive(from); // in case this is from a server, and we have a connection
  queueMessage(from, TXT_TYPE_CLI_DATA, pkt, sender_timestamp, NULL, 0, text);
}

void MyMesh::onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                                 const uint8_t *sender_prefix, const char *text) {
  markConnectionActive(from);
  // from.sync_since change needs to be persisted
  dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
  queueMessage(from, TXT_TYPE_SIGNED_PLAIN, pkt, sender_timestamp, sender_prefix, 4, text);
}

void MyMesh::onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                                  const char *text) {
  int i = 0;
  if (app_target_ver >= 3) {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV_V3;
    out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
    out_frame[i++] = 0; // reserved1
    out_frame[i++] = 0; // reserved2
  } else {
    out_frame[i++] = RESP_CODE_CHANNEL_MSG_RECV;
  }

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  uint8_t path_len = out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;

  out_frame[i++] = TXT_TYPE_PLAIN;
  memcpy(&out_frame[i], &timestamp, 4);
  i += 4;
  int tlen = strlen(text); // TODO: UTF-8 ??
  if (i + tlen > MAX_FRAME_SIZE) {
    tlen = MAX_FRAME_SIZE - i;
  }
  memcpy(&out_frame[i], text, tlen);
  i += tlen;
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  } else {
#ifdef DISPLAY_CLASS
    if (_ui) _ui->notify(UIEventType::channelMessage);
#endif
  }
#ifdef DISPLAY_CLASS
  // Get the channel name from the channel index
  const char *channel_name = "Unknown";
  ChannelDetails channel_details;
  if (getChannel(channel_idx, channel_details)) {
    channel_name = channel_details.name;
  }
  if (_ui && isDisplayableText(text)) {
    uint8_t display_path_len = path_len == 0xFF ? 0xFF : pkt->getPathHashCount();
    char scope[32];
    formatPacketScope(pkt, scope, sizeof(scope));
    char mention[32] = "";
    const char* colon = strchr(text, ':');
    if (colon && colon > text) {
      size_t len = colon - text;
      while (len > 0 && text[len - 1] == ' ') len--;
      if (len >= sizeof(mention)) len = sizeof(mention) - 1;
      memcpy(mention, text, len);
      mention[len] = 0;
    }
    _ui->newMsg(display_path_len, channel_name, nullptr, channel_idx, mention, scope, text, offline_queue_len);
  }
#endif
}

void MyMesh::onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                               const uint8_t *data, size_t data_len) {
  if (data_len > MAX_CHANNEL_DATA_LENGTH) {
    MESH_DEBUG_PRINTLN("onChannelDataRecv: dropping payload_len=%d exceeds frame limit=%d",
                       (uint32_t)data_len, (uint32_t)MAX_CHANNEL_DATA_LENGTH);
    return;
  }

  int i = 0;
  out_frame[i++] = RESP_CODE_CHANNEL_DATA_RECV;
  out_frame[i++] = (int8_t)(pkt->getSNR() * 4);
  out_frame[i++] = 0; // reserved1
  out_frame[i++] = 0; // reserved2

  uint8_t channel_idx = findChannelIdx(channel);
  out_frame[i++] = channel_idx;
  out_frame[i++] = pkt->isRouteFlood() ? pkt->path_len : 0xFF;
  out_frame[i++] = (uint8_t)(data_type & 0xFF);
  out_frame[i++] = (uint8_t)(data_type >> 8);
  out_frame[i++] = (uint8_t)data_len;

  int copy_len = (int)data_len;
  if (copy_len > 0) {
    memcpy(&out_frame[i], data, copy_len);
    i += copy_len;
  }
  addToOfflineQueue(out_frame, i);

  if (_serial->isConnected()) {
    uint8_t frame[1];
    frame[0] = PUSH_CODE_MSG_WAITING; // send push 'tickle'
    _serial->writeFrame(frame, 1);
  }
}

uint8_t MyMesh::onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                                 uint8_t len, uint8_t *reply) {
  if (data[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t permissions = 0;
    uint8_t cp = contact.flags >> 1; // LSB used as 'favourite' bit (so only use upper bits)

    if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_ALL) {
      permissions = TELEM_PERM_BASE;
    } else if (_prefs.telemetry_mode_base == TELEM_MODE_ALLOW_FLAGS) {
      permissions = cp & TELEM_PERM_BASE;
    }

    if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_LOCATION;
    } else if (_prefs.telemetry_mode_loc == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_LOCATION;
    }

    if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_ALL) {
      permissions |= TELEM_PERM_ENVIRONMENT;
    } else if (_prefs.telemetry_mode_env == TELEM_MODE_ALLOW_FLAGS) {
      permissions |= cp & TELEM_PERM_ENVIRONMENT;
    }

    uint8_t perm_mask = ~(data[1]);    // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions
    permissions &= perm_mask;

    if (permissions & TELEM_PERM_BASE) { // only respond if base permission bit is set
      telemetry.reset();
      telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      // query other sensors -- target specific
      sensors.querySensors(permissions, telemetry);

      memcpy(reply, &sender_timestamp,
             4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

      uint8_t tlen = telemetry.getSize();
      memcpy(&reply[4], telemetry.getBuffer(), tlen);
      return 4 + tlen;
    }
  }
  return 0; // unknown
}

void MyMesh::onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) {
  uint32_t tag;
  memcpy(&tag, data, 4);

  if (pending_login && memcmp(&pending_login, contact.id.pub_key, 4) == 0) { // check for login response
    // yes, is response to pending sendLogin()
    pending_login = 0;

    int i = 0;
    if (memcmp(&data[4], "OK", 2) == 0) { // legacy Repeater login OK response
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = 0; // legacy: is_admin = false
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6;                                     // pub_key_prefix
    } else if (data[4] == RESP_SERVER_LOGIN_OK) { // new login response
      uint16_t keep_alive_secs = ((uint16_t)data[5]) * 16;
      if (keep_alive_secs > 0) {
        startConnection(contact, keep_alive_secs);
      }
      out_frame[i++] = PUSH_CODE_LOGIN_SUCCESS;
      out_frame[i++] = data[6]; // permissions (eg. is_admin)
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
      memcpy(&out_frame[i], &tag, 4);
      i += 4; // NEW: include server timestamp
      out_frame[i++] = data[7]; // NEW (v7): ACL permissions
      out_frame[i++] = data[12]; // FIRMWARE_VER_LEVEL
    } else {
      out_frame[i++] = PUSH_CODE_LOGIN_FAIL;
      out_frame[i++] = 0; // reserved
      memcpy(&out_frame[i], contact.id.pub_key, 6);
      i += 6; // pub_key_prefix
    }
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && // check for status response
             pending_status &&
             memcmp(&pending_status, contact.id.pub_key, 4) == 0 // legacy matching scheme
                                                                 // FUTURE: tag == pending_status
  ) {
    pending_status = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_STATUS_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len > 4 && tag == pending_telemetry) {  // check for matching response tag
    pending_telemetry = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], contact.id.pub_key, 6);
    i += 6; // pub_key_prefix
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  } else if (len >= 8 && isPendingScopeTag(tag)) {
    if (scope_response_count < UINT8_MAX) scope_response_count++;
    if (len == 8) {
      if (scope_empty_response_count < UINT8_MAX) scope_empty_response_count++;
    } else {
      char names[120];
      size_t names_len = len - 8;
      if (names_len >= sizeof(names)) names_len = sizeof(names) - 1;
      memcpy(names, &data[8], names_len);
      names[names_len] = 0;
      ingestScopeNames(contact, names);
    }
  } else if (len > 4 && tag == pending_req) {  // check for matching response tag
    pending_req = 0;

    int i = 0;
    out_frame[i++] = PUSH_CODE_BINARY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], &tag, 4);   // app needs to match this to RESP_CODE_SENT.tag
    i += 4;
    memcpy(&out_frame[i], &data[4], len - 4);
    i += (len - 4);
    _serial->writeFrame(out_frame, i);
  }
}

bool MyMesh::onContactPathRecv(ContactInfo& contact, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) {
  if (extra_type == PAYLOAD_TYPE_RESPONSE && extra_len > 4) {
    uint32_t tag;
    memcpy(&tag, extra, 4);

    if (tag == pending_discovery) {  // check for matching response tag)
      pending_discovery = 0;

      if (!mesh::Packet::isValidPathLen(in_path_len) || !mesh::Packet::isValidPathLen(out_path_len)) {
        MESH_DEBUG_PRINTLN("onContactPathRecv, invalid path sizes: %d, %d", in_path_len, out_path_len);
      } else {
        int i = 0;
        out_frame[i++] = PUSH_CODE_PATH_DISCOVERY_RESPONSE;
        out_frame[i++] = 0; // reserved
        memcpy(&out_frame[i], contact.id.pub_key, 6);
        i += 6; // pub_key_prefix
        out_frame[i++] = out_path_len;
        i += mesh::Packet::writePath(&out_frame[i], out_path, out_path_len);
        out_frame[i++] = in_path_len;
        i += mesh::Packet::writePath(&out_frame[i], in_path, in_path_len);
        // NOTE: telemetry data in 'extra' is discarded at present

        _serial->writeFrame(out_frame, i);
      }
      return false;  // DON'T send reciprocal path!
    }
  }
  // let base class handle received path and data
  return BaseChatMesh::onContactPathRecv(contact, in_path, in_path_len, out_path, out_path_len, extra_type, extra, extra_len);
}

void MyMesh::onControlDataRecv(mesh::Packet *packet) {
  uint8_t type = packet->payload_len > 0 ? (packet->payload[0] & 0xF0) : 0;
  if (type == CTL_TYPE_NODE_DISCOVER_RESP && packet->payload_len >= 6 + PUB_KEY_SIZE) {
    uint8_t node_type = packet->payload[0] & 0x0F;
    uint32_t tag;
    memcpy(&tag, &packet->payload[2], 4);
    if (node_type == ADV_TYPE_REPEATER && pending_scope_discover_tag != 0 && tag == pending_scope_discover_tag) {
      if (scope_discover_response_count < UINT8_MAX) scope_discover_response_count++;
      ContactInfo target;
      memset(&target, 0, sizeof(target));
      memcpy(target.id.pub_key, &packet->payload[6], PUB_KEY_SIZE);
      snprintf(target.name, sizeof(target.name), "%02X%02X%02X%02X",
               target.id.pub_key[0], target.id.pub_key[1], target.id.pub_key[2], target.id.pub_key[3]);
      target.type = ADV_TYPE_REPEATER;
      target.out_path_len = 0; // zero-hop response means this repeater is directly reachable
      target.shared_secret_valid = false;

      uint8_t request[2];
      request[0] = ANON_REQ_TYPE_REGIONS;
      request[1] = 0;
      uint8_t sent = last_scope_scan_sent;
      if (sendScopeRequest(target, request, sizeof(request), sent)) {
        last_scope_scan_sent = sent;
      }
    }
  }

  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_CONTROL_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = packet->path_len;
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onControlDataRecv(), data received while app offline");
  }
}

void MyMesh::onRawDataRecv(mesh::Packet *packet) {
  if (packet->payload_len + 4 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), payload_len too long: %d", packet->payload_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_RAW_DATA;
  out_frame[i++] = (int8_t)(_radio->getLastSNR() * 4);
  out_frame[i++] = (int8_t)(_radio->getLastRSSI());
  out_frame[i++] = 0xFF; // reserved (possibly path_len in future)
  memcpy(&out_frame[i], packet->payload, packet->payload_len);
  i += packet->payload_len;

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onRawDataRecv(), data received while app offline");
  }
}

void MyMesh::onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                         const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) {
  uint8_t path_sz = flags & 0x03;  // NEW v1.11+
  if (12 + path_len + (path_len >> path_sz) + 1 > sizeof(out_frame)) {
    MESH_DEBUG_PRINTLN("onTraceRecv(), path_len is too long: %d", (uint32_t)path_len);
    return;
  }
  int i = 0;
  out_frame[i++] = PUSH_CODE_TRACE_DATA;
  out_frame[i++] = 0; // reserved
  out_frame[i++] = path_len;
  out_frame[i++] = flags;
  memcpy(&out_frame[i], &tag, 4);
  i += 4;
  memcpy(&out_frame[i], &auth_code, 4);
  i += 4;
  memcpy(&out_frame[i], path_hashes, path_len);
  i += path_len;

  memcpy(&out_frame[i], path_snrs, path_len >> path_sz);
  i += path_len >> path_sz;
  out_frame[i++] = (int8_t)(packet->getSNR() * 4); // extra/final SNR (to this node)

  if (_serial->isConnected()) {
    _serial->writeFrame(out_frame, i);
  } else {
    MESH_DEBUG_PRINTLN("onTraceRecv(), data received while app offline");
  }
}

uint32_t MyMesh::calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const {
  return SEND_TIMEOUT_BASE_MILLIS + (FLOOD_SEND_TIMEOUT_FACTOR * pkt_airtime_millis);
}
uint32_t MyMesh::calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const {
  uint8_t path_hash_count = path_len & 63;
  return SEND_TIMEOUT_BASE_MILLIS +
         ((pkt_airtime_millis * DIRECT_SEND_PERHOP_FACTOR + DIRECT_SEND_PERHOP_EXTRA_MILLIS) *
          (path_hash_count + 1));
}

void MyMesh::onSendTimeout() {}

MyMesh::MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui)
    : BaseChatMesh(radio, *new ArduinoMillis(), rng, rtc, *new StaticPoolPacketManager(16), tables),
      _serial(NULL), telemetry(MAX_PACKET_PAYLOAD - 4), _store(&store), _ui(ui) {
  _iter_started = false;
  _cli_rescue = false;
  offline_queue_len = 0;
  app_target_ver = 0;
  clearPendingReqs();
  memset(pending_scope_req, 0, sizeof(pending_scope_req));
  pending_scope_discover_tag = 0;
  next_ack_idx = 0;
  sign_data = NULL;
  dirty_contacts_expiry = 0;
  memset(advert_paths, 0, sizeof(advert_paths));
  memset(send_scope.key, 0, sizeof(send_scope.key));
  monitor_rx_packets = 0;
  monitor_last_snr_x4 = 0;
  monitor_activity_index = 0;
  monitor_next_activity_rollover = 0;
  memset(monitor_paths, 0, sizeof(monitor_paths));
  memset(monitor_last_hops, 0, sizeof(monitor_last_hops));
  memset(monitor_activity_bins, 0, sizeof(monitor_activity_bins));
  memset(monitor_airtime_bins, 0, sizeof(monitor_airtime_bins));
  memset(scope_cache, 0, sizeof(scope_cache));
  memset(scope_targets, 0, sizeof(scope_targets));
  last_scope_scan_sent = 0;
  scope_response_count = 0;
  scope_discover_response_count = 0;
  scope_empty_response_count = 0;
  resetQuickMessages();

  // defaults
  memset(&_prefs, 0, sizeof(_prefs));
  _prefs.airtime_factor = 1.0;
  strcpy(_prefs.node_name, "NONAME");
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.gps_enabled = 0;       // GPS disabled by default
  _prefs.gps_interval = 0;      // No automatic GPS updates by default
  //_prefs.rx_delay_base = 10.0f;  enable once new algo fixed
#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default
#endif
#endif
}

void MyMesh::begin(bool has_display) {
  BaseChatMesh::begin();

  if (!_store->loadMainIdentity(self_id)) {
    self_id = radio_new_identity(); // create new random identity
    int count = 0;
    while (count < 10 && (self_id.pub_key[0] == 0x00 || self_id.pub_key[0] == 0xFF)) { // reserved id hashes
      self_id = radio_new_identity();
      count++;
    }
    _store->saveMainIdentity(self_id);
  }

// if name is provided as a build flag, use that as default node name instead
#ifdef ADVERT_NAME
  strcpy(_prefs.node_name, ADVERT_NAME);
#else
  // use hex of first 4 bytes of identity public key as default node name
  char pub_key_hex[10];
  mesh::Utils::toHex(pub_key_hex, self_id.pub_key, 4);
  strcpy(_prefs.node_name, pub_key_hex);
#endif

  // if build provides default-scope, init with that
#ifdef DEFAULT_FLOOD_SCOPE_NAME
  strcpy(_prefs.default_scope_name, DEFAULT_FLOOD_SCOPE_NAME);
  {
    TransportKeyStore temp;
    TransportKey key;
    temp.getAutoKeyFor(0, "#" DEFAULT_FLOOD_SCOPE_NAME, key);
    memcpy(_prefs.default_scope_key, key.key, sizeof(key.key));
  }
#endif

  // load persisted prefs
  _store->loadPrefs(_prefs, sensors.node_lat, sensors.node_lon);

  // sanitise bad pref values
  _prefs.rx_delay_base = constrain(_prefs.rx_delay_base, 0, 20.0f);
  _prefs.airtime_factor = constrain(_prefs.airtime_factor, 0, 9.0f);
  _prefs.freq = constrain(_prefs.freq, 150.0f, 2500.0f);
  _prefs.bw = constrain(_prefs.bw, 7.8f, 500.0f);
  _prefs.sf = constrain(_prefs.sf, 5, 12);
  _prefs.cr = constrain(_prefs.cr, 5, 8);
  _prefs.tx_power_dbm = constrain(_prefs.tx_power_dbm, -9, MAX_LORA_TX_POWER);
  _prefs.gps_enabled = constrain(_prefs.gps_enabled, 0, 1);  // Ensure boolean 0 or 1
  _prefs.gps_interval = constrain(_prefs.gps_interval, 0, 86400);  // Max 24 hours

#ifdef BLE_PIN_CODE // 123456 by default
  if (_prefs.ble_pin == 0) {
#ifdef DISPLAY_CLASS
    if (has_display && BLE_PIN_CODE == 123456) {
      StdRNG rng;
      _active_ble_pin = rng.nextInt(100000, 999999); // random pin each session
    } else {
      _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
    }
#else
    _active_ble_pin = BLE_PIN_CODE; // otherwise static pin
#endif
  } else {
    _active_ble_pin = _prefs.ble_pin;
  }
#else
  _active_ble_pin = 0;
#endif

  resetContacts();
  _store->loadContacts(this);
  bootstrapRTCfromContacts();
  addChannel("Public", PUBLIC_GROUP_PSK); // pre-configure Andy's public channel
  _store->loadChannels(this);
  loadQuickMessages();

  radio_set_params(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_set_tx_power(_prefs.tx_power_dbm);
  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");
}

const char *MyMesh::getNodeName() {
  return _prefs.node_name;
}
NodePrefs *MyMesh::getNodePrefs() {
  return &_prefs;
}
uint32_t MyMesh::getBLEPin() {
  return _active_ble_pin;
}

struct FreqRange {
  uint32_t lower_freq, upper_freq;
};

static FreqRange repeat_freq_ranges[] = {
  { 433000, 433000 },
  { 869000, 869000 },
  { 918000, 918000 }
};

bool MyMesh::isValidClientRepeatFreq(uint32_t f) const {
  for (int i = 0; i < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]); i++) {
    auto r = &repeat_freq_ranges[i];
    if (f >= r->lower_freq && f <= r->upper_freq) return true;
  }
  return false;
}

void MyMesh::startInterface(BaseSerialInterface &serial) {
  _serial = &serial;
  serial.enable();
}

void MyMesh::handleCmdFrame(size_t len) {
  if (cmd_frame[0] == CMD_DEVICE_QEURY && len >= 2) { // sent when app establishes connection
    app_target_ver = cmd_frame[1];                    // which version of protocol does app understand

    int i = 0;
    out_frame[i++] = RESP_CODE_DEVICE_INFO;
    out_frame[i++] = FIRMWARE_VER_CODE;
    out_frame[i++] = MAX_CONTACTS / 2;   // v3+
    out_frame[i++] = MAX_GROUP_CHANNELS; // v3+
    memcpy(&out_frame[i], &_prefs.ble_pin, 4);
    i += 4;
    memset(&out_frame[i], 0, 12);
    strcpy((char *)&out_frame[i], FIRMWARE_BUILD_DATE);
    i += 12;
    StrHelper::strzcpy((char *)&out_frame[i], board.getManufacturerName(), 40);
    i += 40;
    StrHelper::strzcpy((char *)&out_frame[i], CLIENT_FIRMWARE_VERSION, 20);
    i += 20;
    out_frame[i++] = _prefs.client_repeat;   // v9+
    out_frame[i++] = _prefs.path_hash_mode;  // v10+
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_APP_START &&
             len >= 8) { // sent when app establishes connection, respond with node ID
    //  cmd_frame[1..7]  reserved future
    char *app_name = (char *)&cmd_frame[8];
    cmd_frame[len] = 0; // make app_name null terminated
    MESH_DEBUG_PRINTLN("App %s connected", app_name);

    _iter_started = false; // stop any left-over ContactsIterator
    int i = 0;
    out_frame[i++] = RESP_CODE_SELF_INFO;
    out_frame[i++] = ADV_TYPE_CHAT; // what this node Advert identifies as (maybe node's pronouns too?? :-)
    out_frame[i++] = _prefs.tx_power_dbm;
    out_frame[i++] = MAX_LORA_TX_POWER;
    memcpy(&out_frame[i], self_id.pub_key, PUB_KEY_SIZE);
    i += PUB_KEY_SIZE;

    int32_t lat, lon;
    lat = (sensors.node_lat * 1000000.0);
    lon = (sensors.node_lon * 1000000.0);
    memcpy(&out_frame[i], &lat, 4);
    i += 4;
    memcpy(&out_frame[i], &lon, 4);
    i += 4;
    out_frame[i++] = _prefs.multi_acks; // new v7+
    out_frame[i++] = _prefs.advert_loc_policy;
    out_frame[i++] = (_prefs.telemetry_mode_env << 4) | (_prefs.telemetry_mode_loc << 2) |
                     (_prefs.telemetry_mode_base); // v5+
    out_frame[i++] = _prefs.manual_add_contacts;

    uint32_t freq = _prefs.freq * 1000;
    memcpy(&out_frame[i], &freq, 4);
    i += 4;
    uint32_t bw = _prefs.bw * 1000;
    memcpy(&out_frame[i], &bw, 4);
    i += 4;
    out_frame[i++] = _prefs.sf;
    out_frame[i++] = _prefs.cr;

    int tlen = strlen(_prefs.node_name); // revisit: UTF_8 ??
    memcpy(&out_frame[i], _prefs.node_name, tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_TXT_MSG && len >= 14) {
    int i = 1;
    uint8_t txt_type = cmd_frame[i++];
    uint8_t attempt = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    uint8_t *pub_key_prefix = &cmd_frame[i];
    i += 6;
    ContactInfo *recipient = lookupContactByPubKey(pub_key_prefix, 6);
    if (recipient && (txt_type == TXT_TYPE_PLAIN || txt_type == TXT_TYPE_CLI_DATA)) {
      char *text = (char *)&cmd_frame[i];
      int tlen = len - i;
      uint32_t est_timeout;
      text[tlen] = 0; // ensure null
      int result;
      uint32_t expected_ack;
      if (txt_type == TXT_TYPE_CLI_DATA) {
        msg_timestamp = getRTCClock()->getCurrentTimeUnique(); // Use node's RTC instead of app timestamp to avoid tripping replay protection
        result = sendCommandData(*recipient, msg_timestamp, attempt, text, est_timeout);
        expected_ack = 0; // no Ack expected
      } else {
        result = sendMessage(*recipient, msg_timestamp, attempt, text, expected_ack, est_timeout);
      }
      // TODO: add expected ACK to table
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        if (expected_ack) {
          expected_ack_table[next_ack_idx].msg_sent = _ms->getMillis(); // add to circular table
          expected_ack_table[next_ack_idx].ack = expected_ack;
          expected_ack_table[next_ack_idx].contact = recipient;
          next_ack_idx = (next_ack_idx + 1) % EXPECTED_ACK_TABLE_SIZE;
        }

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &expected_ack, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(recipient == NULL
                        ? ERR_CODE_NOT_FOUND
                        : ERR_CODE_UNSUPPORTED_CMD); // unknown recipient, or unsuported TXT_TYPE_*
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_TXT_MSG) { // send GroupChannel text msg
    int i = 1;
    uint8_t txt_type = cmd_frame[i++]; // should be TXT_TYPE_PLAIN
    uint8_t channel_idx = cmd_frame[i++];
    uint32_t msg_timestamp;
    memcpy(&msg_timestamp, &cmd_frame[i], 4);
    i += 4;
    const char *text = (char *)&cmd_frame[i];

    if (txt_type != TXT_TYPE_PLAIN) {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    } else {
      ChannelDetails channel;
      bool success = getChannel(channel_idx, channel);
      if (success && sendGroupMessage(msg_timestamp, channel.channel, _prefs.node_name, text, len - i)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
      }
    }
  } else if (cmd_frame[0] == CMD_SEND_CHANNEL_DATA) { // send GroupChannel datagram
    if (len < 4) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }
    int i = 1;
    uint8_t channel_idx = cmd_frame[i++];
    uint8_t path_len = cmd_frame[i++];

    // validate path len, allowing 0xFF for flood
    if (!mesh::Packet::isValidPathLen(path_len) && path_len != OUT_PATH_UNKNOWN) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA invalid path size: %d", path_len);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      return;
    }

    // parse provided path if not flood
    uint8_t path[MAX_PATH_SIZE];
    if (path_len != OUT_PATH_UNKNOWN) {
      i += mesh::Packet::writePath(path, &cmd_frame[i], path_len);
    }

    uint16_t data_type = ((uint16_t)cmd_frame[i]) | (((uint16_t)cmd_frame[i + 1]) << 8);
    i += 2;
    const uint8_t *payload = &cmd_frame[i];
    int payload_len = (len > (size_t)i) ? (int)(len - i) : 0;

    ChannelDetails channel;
    if (!getChannel(channel_idx, channel)) {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    } else if (data_type == DATA_TYPE_RESERVED) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (payload_len > MAX_CHANNEL_DATA_LENGTH) {
      MESH_DEBUG_PRINTLN("CMD_SEND_CHANNEL_DATA payload too long: %d > %d", payload_len, MAX_CHANNEL_DATA_LENGTH);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (sendGroupData(channel.channel, path, path_len, data_type, payload, payload_len)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACTS) { // get Contact list
    if (_iter_started) {
      writeErrFrame(ERR_CODE_BAD_STATE); // iterator is currently busy
    } else {
      if (len >= 5) { // has optional 'since' param
        memcpy(&_iter_filter_since, &cmd_frame[1], 4);
      } else {
        _iter_filter_since = 0;
      }

      uint8_t reply[5];
      reply[0] = RESP_CODE_CONTACTS_START;
      uint32_t count = getNumContacts(); // total, NOT filtered count
      memcpy(&reply[1], &count, 4);
      _serial->writeFrame(reply, 5);

      // start iterator
      _iter = startContactsIterator();
      _iter_started = true;
      _most_recent_lastmod = 0;
    }
  } else if (cmd_frame[0] == CMD_SET_ADVERT_NAME && len >= 2) {
    int nlen = len - 1;
    if (nlen > sizeof(_prefs.node_name) - 1) nlen = sizeof(_prefs.node_name) - 1; // max len
    memcpy(_prefs.node_name, &cmd_frame[1], nlen);
    _prefs.node_name[nlen] = 0; // null terminator
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_ADVERT_LATLON && len >= 9) {
    int32_t lat, lon, alt = 0;
    memcpy(&lat, &cmd_frame[1], 4);
    memcpy(&lon, &cmd_frame[5], 4);
    if (len >= 13) {
      memcpy(&alt, &cmd_frame[9], 4); // for FUTURE support
    }
    if (lat <= 90 * 1E6 && lat >= -90 * 1E6 && lon <= 180 * 1E6 && lon >= -180 * 1E6) {
      sensors.node_lat = ((double)lat) / 1000000.0;
      sensors.node_lon = ((double)lon) / 1000000.0;
      savePrefs();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid geo coordinate
    }
  } else if (cmd_frame[0] == CMD_GET_DEVICE_TIME) {
    uint8_t reply[5];
    reply[0] = RESP_CODE_CURR_TIME;
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply[1], &now, 4);
    _serial->writeFrame(reply, 5);
  } else if (cmd_frame[0] == CMD_SET_DEVICE_TIME && len >= 5) {
    uint32_t secs;
    memcpy(&secs, &cmd_frame[1], 4);
    uint32_t curr = getRTCClock()->getCurrentTime();
    if (secs >= curr) {
      getRTCClock()->setCurrentTime(secs);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SEND_SELF_ADVERT) {
    mesh::Packet* pkt;
    if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
      pkt = createSelfAdvert(_prefs.node_name);
    } else {
      pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
    }
    if (pkt) {
      if (len >= 2 && cmd_frame[1] == 1) { // optional param (1 = flood, 0 = zero hop)
        unsigned long delay_millis = 0;
        TransportKey default_scope;
        memcpy(&default_scope.key, _prefs.default_scope_key, sizeof(default_scope.key));
        sendFloodScoped(default_scope, pkt, delay_millis);
      } else {
        sendZeroHop(pkt);
      }
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_RESET_PATH && len >= 1 + 32) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      // recipient->lastmod = ??   shouldn't be needed, app already has this version of contact
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // unknown contact
    }
  } else if (cmd_frame[0] == CMD_ADD_UPDATE_CONTACT && len >= 1 + 32 + 2 + 1) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint32_t last_mod = getRTCClock()->getCurrentTime();  // fallback value if not present in cmd_frame
    if (recipient) {
      updateContactFromFrame(*recipient, last_mod, cmd_frame, len);
      recipient->lastmod = last_mod;
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      ContactInfo contact;
      updateContactFromFrame(contact, last_mod, cmd_frame, len);
      contact.lastmod = last_mod;
      contact.sync_since = 0;
      if (addContact(contact)) {
        dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_REMOVE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient && removeContact(*recipient)) {
      _store->deleteBlobByKey(pub_key, PUB_KEY_SIZE);
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found, or unable to remove
    }
  } else if (cmd_frame[0] == CMD_SHARE_CONTACT) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      if (shareContactZeroHop(*recipient)) {
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // unable to send
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_CONTACT_BY_KEY) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *contact = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (contact) {
      writeContactRespFrame(RESP_CODE_CONTACT, *contact);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // not found
    }
  } else if (cmd_frame[0] == CMD_EXPORT_CONTACT) {
    if (len < 1 + PUB_KEY_SIZE) {
      // export SELF
      mesh::Packet* pkt;
      if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
        pkt = createSelfAdvert(_prefs.node_name);
      } else {
        pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
      }
      if (pkt) {
        pkt->header |= ROUTE_TYPE_FLOOD; // would normally be sent in this mode

        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        uint8_t out_len = pkt->writeTo(&out_frame[1]);
        releasePacket(pkt); // undo the obtainNewPacket()
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL); // Error
      }
    } else {
      uint8_t *pub_key = &cmd_frame[1];
      ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
      uint8_t out_len;
      if (recipient && (out_len = exportContact(*recipient, &out_frame[1])) > 0) {
        out_frame[0] = RESP_CODE_EXPORT_CONTACT;
        _serial->writeFrame(out_frame, out_len + 1);
      } else {
        writeErrFrame(ERR_CODE_NOT_FOUND); // not found
      }
    }
  } else if (cmd_frame[0] == CMD_IMPORT_CONTACT && len > 2 + 32 + 64) {
    if (importContact(&cmd_frame[1], len - 1)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SYNC_NEXT_MESSAGE) {
    int out_len;
    if ((out_len = getFromOfflineQueue(out_frame)) > 0) {
      _serial->writeFrame(out_frame, out_len);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(offline_queue_len);
#endif
    } else {
      out_frame[0] = RESP_CODE_NO_MORE_MESSAGES;
      _serial->writeFrame(out_frame, 1);
#ifdef DISPLAY_CLASS
      if (_ui) _ui->msgRead(0);
#endif
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_PARAMS) {
    int i = 1;
    uint32_t freq;
    memcpy(&freq, &cmd_frame[i], 4);
    i += 4;
    uint32_t bw;
    memcpy(&bw, &cmd_frame[i], 4);
    i += 4;
    uint8_t sf = cmd_frame[i++];
    uint8_t cr = cmd_frame[i++];
    uint8_t repeat = 0;  // default - false
    if (len > i) {
      repeat = cmd_frame[i++];   // FIRMWARE_VER_CODE  9+
    }

    if (repeat && !isValidClientRepeatFreq(freq)) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else if (freq >= 150000 && freq <= 2500000 && sf >= 5 && sf <= 12 && cr >= 5 && cr <= 8 && bw >= 7000 &&
        bw <= 500000) {
      _prefs.sf = sf;
      _prefs.cr = cr;
      _prefs.freq = (float)freq / 1000.0;
      _prefs.bw = (float)bw / 1000.0;
      _prefs.client_repeat = repeat;
      savePrefs();

      radio_set_params(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
      MESH_DEBUG_PRINTLN("OK: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);

      writeOKFrame();
    } else {
      MESH_DEBUG_PRINTLN("Error: CMD_SET_RADIO_PARAMS: f=%d, bw=%d, sf=%d, cr=%d", freq, bw, (uint32_t)sf,
                         (uint32_t)cr);
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_SET_RADIO_TX_POWER) {
    int8_t power = (int8_t)cmd_frame[1];
    if (power < -9 || power > MAX_LORA_TX_POWER) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.tx_power_dbm = power;
      savePrefs();
      radio_set_tx_power(_prefs.tx_power_dbm);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SET_TUNING_PARAMS) {
    int i = 1;
    uint32_t rx, af;
    memcpy(&rx, &cmd_frame[i], 4);
    i += 4;
    memcpy(&af, &cmd_frame[i], 4);
    i += 4;
    _prefs.rx_delay_base = ((float)rx) / 1000.0f;
    _prefs.airtime_factor = ((float)af) / 1000.0f;
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_TUNING_PARAMS) {
    uint32_t rx = _prefs.rx_delay_base * 1000, af = _prefs.airtime_factor * 1000;
    int i = 0;
    out_frame[i++] = RESP_CODE_TUNING_PARAMS;
    memcpy(&out_frame[i], &rx, 4); i += 4;
    memcpy(&out_frame[i], &af, 4); i += 4;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SET_OTHER_PARAMS) {
    _prefs.manual_add_contacts = cmd_frame[1];
    if (len >= 3) {
      _prefs.telemetry_mode_base = cmd_frame[2] & 0x03; // v5+
      _prefs.telemetry_mode_loc = (cmd_frame[2] >> 2) & 0x03;
      _prefs.telemetry_mode_env = (cmd_frame[2] >> 4) & 0x03;

      if (len >= 4) {
        _prefs.advert_loc_policy = cmd_frame[3];
        if (len >= 5) {
          _prefs.multi_acks = cmd_frame[4];
        }
      }
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_PATH_HASH_MODE && cmd_frame[1] == 0 && len >= 3) {
    if (cmd_frame[2] >= 3) {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      _prefs.path_hash_mode = cmd_frame[2];
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_REBOOT && memcmp(&cmd_frame[1], "reboot", 6) == 0) {
    if (dirty_contacts_expiry) { // is there are pending dirty contacts write needed?
      saveContacts();
    }
    board.reboot();
  } else if (cmd_frame[0] == CMD_GET_BATT_AND_STORAGE) {
    uint8_t reply[11];
    int i = 0;
    reply[i++] = RESP_CODE_BATT_AND_STORAGE;
    uint16_t battery_millivolts = board.getBattMilliVolts();
    uint32_t used = _store->getStorageUsedKb();
    uint32_t total = _store->getStorageTotalKb();
    memcpy(&reply[i], &battery_millivolts, 2); i += 2;
    memcpy(&reply[i], &used, 4); i += 4;
    memcpy(&reply[i], &total, 4); i += 4;
    _serial->writeFrame(reply, i);
  } else if (cmd_frame[0] == CMD_EXPORT_PRIVATE_KEY) {
#if ENABLE_PRIVATE_KEY_EXPORT
    uint8_t reply[65];
    reply[0] = RESP_CODE_PRIVATE_KEY;
    self_id.writeTo(&reply[1], 64);
    _serial->writeFrame(reply, 65);
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_IMPORT_PRIVATE_KEY && len >= 65) {
#if ENABLE_PRIVATE_KEY_IMPORT
    if (!mesh::LocalIdentity::validatePrivateKey(&cmd_frame[1])) {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid key
    } else {
        mesh::LocalIdentity identity;
        identity.readFrom(&cmd_frame[1], 64);
        if (_store->saveMainIdentity(identity)) {
          self_id = identity;
          writeOKFrame();
          // re-load contacts, to invalidate ecdh shared_secrets
          resetContacts();
          _store->loadContacts(this);
        } else {
          writeErrFrame(ERR_CODE_FILE_IO_ERROR);
        }
    }
#else
    writeDisabledFrame();
#endif
  } else if (cmd_frame[0] == CMD_SEND_RAW_DATA && len >= 6) {
    int i = 1;
    int8_t path_len = cmd_frame[i++];
    if (path_len >= 0 && i + path_len + 4 <= len) { // minimum 4 byte payload
      uint8_t *path = &cmd_frame[i];
      i += path_len;
      auto pkt = createRawData(&cmd_frame[i], len - i);
      if (pkt) {
        sendDirect(pkt, path, path_len);
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    } else {
      writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // flood, not supported (yet)
    }
  } else if (cmd_frame[0] == CMD_SEND_LOGIN && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    char *password = (char *)&cmd_frame[1 + PUB_KEY_SIZE];
    cmd_frame[len] = 0; // ensure null terminator in password
    if (recipient) {
      uint32_t est_timeout;
      int result = sendLogin(*recipient, password, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        memcpy(&pending_login, recipient->id.pub_key, 4); // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &pending_login, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_ANON_REQ && len > 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    uint8_t *data = &cmd_frame[1 + PUB_KEY_SIZE];
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendAnonReq(*recipient, data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this to onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_STATUS_REQ && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_STATUS, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        // FUTURE:  pending_status = tag;  // match this in onContactResponse()
        memcpy(&pending_status, recipient->id.pub_key, 4); // legacy matching scheme
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_PATH_DISCOVERY_REQ && cmd_frame[1] == 0 && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[2];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      // 'Path Discovery' is just a special case of flood + Telemetry req
      uint8_t req_data[9];
      req_data[0] = REQ_TYPE_GET_TELEMETRY_DATA;
      req_data[1] = ~(TELEM_PERM_BASE);  // NEW: inverse permissions mask (ie. we only want BASE telemetry)
      memset(&req_data[2], 0, 3);  // reserved
      getRNG()->random(&req_data[5], 4);   // random blob to help make packet-hash unique
      auto save = recipient->out_path_len;    // temporarily force sendRequest() to flood
      recipient->out_path_len = OUT_PATH_UNKNOWN;
      int result = sendRequest(*recipient, req_data, sizeof(req_data), tag, est_timeout);
      recipient->out_path_len = save;
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_discovery = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len >= 4 + PUB_KEY_SIZE) {  // can deprecate, in favour of CMD_SEND_BINARY_REQ
    uint8_t *pub_key = &cmd_frame[4];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, REQ_TYPE_GET_TELEMETRY_DATA, tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_telemetry = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_SEND_TELEMETRY_REQ && len == 4) {  // 'self' telemetry request
    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
    // query other sensors -- target specific
    sensors.querySensors(0xFF, telemetry);

    int i = 0;
    out_frame[i++] = PUSH_CODE_TELEMETRY_RESPONSE;
    out_frame[i++] = 0; // reserved
    memcpy(&out_frame[i], self_id.pub_key, 6);
    i += 6; // pub_key_prefix
    uint8_t tlen = telemetry.getSize();
    memcpy(&out_frame[i], telemetry.getBuffer(), tlen);
    i += tlen;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_SEND_BINARY_REQ && len >= 2 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    ContactInfo *recipient = lookupContactByPubKey(pub_key, PUB_KEY_SIZE);
    if (recipient) {
      uint8_t *req_data = &cmd_frame[1 + PUB_KEY_SIZE];
      uint32_t tag, est_timeout;
      int result = sendRequest(*recipient, req_data, len - (1 + PUB_KEY_SIZE), tag, est_timeout);
      if (result == MSG_SEND_FAILED) {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      } else {
        clearPendingReqs();
        pending_req = tag; // match this in onContactResponse()
        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = (result == MSG_SEND_SENT_FLOOD) ? 1 : 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      }
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // contact not found
    }
  } else if (cmd_frame[0] == CMD_HAS_CONNECTION && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    if (hasConnectionTo(pub_key)) {
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_LOGOUT && len >= 1 + PUB_KEY_SIZE) {
    uint8_t *pub_key = &cmd_frame[1];
    stopConnection(pub_key);
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_CHANNEL && len >= 2) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    if (getChannel(channel_idx, channel)) {
      int i = 0;
      out_frame[i++] = RESP_CODE_CHANNEL_INFO;
      out_frame[i++] = channel_idx;
      strcpy((char *)&out_frame[i], channel.name);
      i += 32;
      memcpy(&out_frame[i], channel.channel.secret, 16);
      i += 16; // NOTE: only 128-bit supported
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 32) {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD); // not supported (yet)
  } else if (cmd_frame[0] == CMD_SET_CHANNEL && len >= 2 + 32 + 16) {
    uint8_t channel_idx = cmd_frame[1];
    ChannelDetails channel;
    StrHelper::strncpy(channel.name, (char *)&cmd_frame[2], 32);
    memset(channel.channel.secret, 0, sizeof(channel.channel.secret));
    memcpy(channel.channel.secret, &cmd_frame[2 + 32], 16); // NOTE: only 128-bit supported
    if (setChannel(channel_idx, channel)) {
      saveChannels();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND); // bad channel_idx
    }
  } else if (cmd_frame[0] == CMD_SIGN_START) {
    out_frame[0] = RESP_CODE_SIGN_START;
    out_frame[1] = 0; // reserved
    uint32_t len = MAX_SIGN_DATA_LEN;
    memcpy(&out_frame[2], &len, 4);
    _serial->writeFrame(out_frame, 6);

    if (sign_data) {
      free(sign_data);
    }
    sign_data = (uint8_t *)malloc(MAX_SIGN_DATA_LEN);
    sign_data_len = 0;
  } else if (cmd_frame[0] == CMD_SIGN_DATA && len > 1) {
    if (sign_data == NULL || sign_data_len + (len - 1) > MAX_SIGN_DATA_LEN) {
      writeErrFrame(sign_data == NULL ? ERR_CODE_BAD_STATE : ERR_CODE_TABLE_FULL); // error: too long
    } else {
      memcpy(&sign_data[sign_data_len], &cmd_frame[1], len - 1);
      sign_data_len += (len - 1);
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_SIGN_FINISH) {
    if (sign_data) {
      self_id.sign(&out_frame[1], sign_data, sign_data_len);

      free(sign_data); // don't need sign_data now
      sign_data = NULL;

      out_frame[0] = RESP_CODE_SIGNATURE;
      _serial->writeFrame(out_frame, 1 + SIGNATURE_SIZE);
    } else {
      writeErrFrame(ERR_CODE_BAD_STATE);
    }
  } else if (cmd_frame[0] == CMD_SEND_TRACE_PATH && len > 10 && len - 10 < MAX_PACKET_PAYLOAD-5) {
    uint8_t path_len = len - 10;
    uint8_t flags = cmd_frame[9];
    uint8_t path_sz = flags & 0x03;  // NEW v1.11+
    if ((path_len >> path_sz) > MAX_PATH_SIZE || (path_len % (1 << path_sz)) != 0) { // make sure is multiple of path_sz
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    } else {
      uint32_t tag, auth;
      memcpy(&tag, &cmd_frame[1], 4);
      memcpy(&auth, &cmd_frame[5], 4);
      auto pkt = createTrace(tag, auth, flags);
      if (pkt) {
        sendDirect(pkt, &cmd_frame[10], path_len);

        uint32_t t = _radio->getEstAirtimeFor(pkt->payload_len + pkt->path_len + 2);
        uint32_t est_timeout = calcDirectTimeoutMillisFor(t, path_len >> path_sz);

        out_frame[0] = RESP_CODE_SENT;
        out_frame[1] = 0;
        memcpy(&out_frame[2], &tag, 4);
        memcpy(&out_frame[6], &est_timeout, 4);
        _serial->writeFrame(out_frame, 10);
      } else {
        writeErrFrame(ERR_CODE_TABLE_FULL);
      }
    }
  } else if (cmd_frame[0] == CMD_SET_DEVICE_PIN && len >= 5) {

    // get pin from command frame
    uint32_t pin;
    memcpy(&pin, &cmd_frame[1], 4);

    // ensure pin is zero, or a valid 6 digit pin
    if (pin == 0 || (pin >= 100000 && pin <= 999999)) {
      _prefs.ble_pin = pin;
      savePrefs();
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_CUSTOM_VARS) {
    out_frame[0] = RESP_CODE_CUSTOM_VARS;
    char *dp = (char *)&out_frame[1];
    for (int i = 0; i < sensors.getNumSettings() && dp - (char *)&out_frame[1] < 140; i++) {
      if (i > 0) {
        *dp++ = ',';
      }
      strcpy(dp, sensors.getSettingName(i));
      dp = strchr(dp, 0);
      *dp++ = ':';
      strcpy(dp, sensors.getSettingValue(i));
      dp = strchr(dp, 0);
    }
    _serial->writeFrame(out_frame, dp - (char *)out_frame);
  } else if (cmd_frame[0] == CMD_SET_CUSTOM_VAR && len >= 4) {
    cmd_frame[len] = 0;
    char *sp = (char *)&cmd_frame[1];
    char *np = strchr(sp, ':'); // look for separator char
    if (np) {
      *np++ = 0; // modify 'cmd_frame', replace ':' with null
      bool success = sensors.setSettingValue(sp, np);
      if (success) {
        #if ENV_INCLUDE_GPS == 1
        // Update node preferences for GPS settings
        if (strcmp(sp, "gps") == 0) {
          _prefs.gps_enabled = (np[0] == '1') ? 1 : 0;
          savePrefs();
        } else if (strcmp(sp, "gps_interval") == 0) {
          uint32_t interval_seconds = atoi(np);
          _prefs.gps_interval = constrain(interval_seconds, 0, 86400);
          savePrefs();
        }
        #endif
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG);
    }
  } else if (cmd_frame[0] == CMD_GET_ADVERT_PATH && len >= PUB_KEY_SIZE+2) {
    // FUTURE use:  uint8_t reserved = cmd_frame[1];
    uint8_t *pub_key = &cmd_frame[2];
    AdvertPath* found = NULL;
    for (int i = 0; i < ADVERT_PATH_TABLE_SIZE; i++) {
      auto p = &advert_paths[i];
      if (memcmp(p->pubkey_prefix, pub_key, sizeof(p->pubkey_prefix)) == 0) {
        found = p;
        break;
      }
    }
    if (found) {
      int i = 0;
      out_frame[i++] = RESP_CODE_ADVERT_PATH;
      memcpy(&out_frame[i], &found->recv_timestamp, 4); i += 4;
      out_frame[i++] = found->path_len;
      i += mesh::Packet::writePath(&out_frame[i], found->path, found->path_len);
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_NOT_FOUND);
    }
  } else if (cmd_frame[0] == CMD_GET_STATS && len >= 2) {
    uint8_t stats_type = cmd_frame[1];
    if (stats_type == STATS_TYPE_CORE) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_CORE;
      uint16_t battery_mv = board.getBattMilliVolts();
      uint32_t uptime_secs = _ms->getMillis() / 1000;
      uint8_t queue_len = (uint8_t)_mgr->getOutboundTotal();
      memcpy(&out_frame[i], &battery_mv, 2); i += 2;
      memcpy(&out_frame[i], &uptime_secs, 4); i += 4;
      memcpy(&out_frame[i], &_err_flags, 2); i += 2;
      out_frame[i++] = queue_len;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_RADIO) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_RADIO;
      int16_t noise_floor = (int16_t)_radio->getNoiseFloor();
      int8_t last_rssi = (int8_t)radio_driver.getLastRSSI();
      int8_t last_snr = (int8_t)(radio_driver.getLastSNR() * 4); // scaled by 4 for 0.25 dB precision
      uint32_t tx_air_secs = getTotalAirTime() / 1000;
      uint32_t rx_air_secs = getReceiveAirTime() / 1000;
      memcpy(&out_frame[i], &noise_floor, 2); i += 2;
      out_frame[i++] = last_rssi;
      out_frame[i++] = last_snr;
      memcpy(&out_frame[i], &tx_air_secs, 4); i += 4;
      memcpy(&out_frame[i], &rx_air_secs, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else if (stats_type == STATS_TYPE_PACKETS) {
      int i = 0;
      out_frame[i++] = RESP_CODE_STATS;
      out_frame[i++] = STATS_TYPE_PACKETS;
      uint32_t recv = radio_driver.getPacketsRecv();
      uint32_t sent = radio_driver.getPacketsSent();
      uint32_t n_sent_flood = getNumSentFlood();
      uint32_t n_sent_direct = getNumSentDirect();
      uint32_t n_recv_flood = getNumRecvFlood();
      uint32_t n_recv_direct = getNumRecvDirect();
      uint32_t n_recv_errors = radio_driver.getPacketsRecvErrors();
      memcpy(&out_frame[i], &recv, 4); i += 4;
      memcpy(&out_frame[i], &sent, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_sent_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_flood, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_direct, 4); i += 4;
      memcpy(&out_frame[i], &n_recv_errors, 4); i += 4;
      _serial->writeFrame(out_frame, i);
    } else {
      writeErrFrame(ERR_CODE_ILLEGAL_ARG); // invalid stats sub-type
    }
  } else if (cmd_frame[0] == CMD_FACTORY_RESET && memcmp(&cmd_frame[1], "reset", 5) == 0) {
    if (_serial) {
      MESH_DEBUG_PRINTLN("Factory reset: disabling serial interface to prevent reconnects (BLE/WiFi)");
      _serial->disable(); // Phone app disconnects before we can send OK frame so it's safe here
    }
    bool success = _store->formatFileSystem();
    if (success) {
      writeOKFrame();
      delay(1000);
      board.reboot();  // doesn't return
    } else {
      writeErrFrame(ERR_CODE_FILE_IO_ERROR);
    }
  } else if (cmd_frame[0] == CMD_SET_FLOOD_SCOPE_KEY && len >= 2 && cmd_frame[1] == 0) {
    if (len >= 2 + 16) {
      memcpy(send_scope.key, &cmd_frame[2], sizeof(send_scope.key));  // set curr scope TransportKey
    } else {
      memset(send_scope.key, 0, sizeof(send_scope.key));  // set scope to null
    }
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_SET_DEFAULT_FLOOD_SCOPE && len >= 1) {
    if (len >= 1+31+16) {
      int n = strlen((char *) &cmd_frame[1]);
      if (n > 0 && n < 31) {
        strcpy(_prefs.default_scope_name, (char *) &cmd_frame[1]);
        memcpy(_prefs.default_scope_key, &cmd_frame[1+31], 16);
        savePrefs();
        writeOKFrame();
      } else {
        writeErrFrame(ERR_CODE_ILLEGAL_ARG);
      }
    } else {
      memset(_prefs.default_scope_name, 0, sizeof(_prefs.default_scope_name));  // set default scope to null
      memset(_prefs.default_scope_key, 0, sizeof(_prefs.default_scope_key));
      savePrefs();
      writeOKFrame();
    }
  } else if (cmd_frame[0] == CMD_GET_DEFAULT_FLOOD_SCOPE) {
    out_frame[0] = RESP_CODE_DEFAULT_FLOOD_SCOPE;
    if (strlen(_prefs.default_scope_name) > 0) {
      memcpy(&out_frame[1], _prefs.default_scope_name, 31);
      memcpy(&out_frame[1+31], _prefs.default_scope_key, 16);
      _serial->writeFrame(out_frame, 1+31+16);
    } else {
      _serial->writeFrame(out_frame, 1);   // no name or key means null
    }
  } else if (cmd_frame[0] == CMD_SEND_CONTROL_DATA && len >= 2 && (cmd_frame[1] & 0x80) != 0) {
    auto resp = createControlData(&cmd_frame[1], len - 1);
    if (resp) {
      sendZeroHop(resp);
      writeOKFrame();
    } else {
      writeErrFrame(ERR_CODE_TABLE_FULL);
    }
  } else if (cmd_frame[0] == CMD_SET_AUTOADD_CONFIG) {
    _prefs.autoadd_config = cmd_frame[1];
    if (len >= 3) {
      _prefs.autoadd_max_hops = min(cmd_frame[2], (uint8_t)64);
    }
    savePrefs();
    writeOKFrame();
  } else if (cmd_frame[0] == CMD_GET_AUTOADD_CONFIG) {
    int i = 0;
    out_frame[i++] = RESP_CODE_AUTOADD_CONFIG;
    out_frame[i++] = _prefs.autoadd_config;
    out_frame[i++] = _prefs.autoadd_max_hops;
    _serial->writeFrame(out_frame, i);
  } else if (cmd_frame[0] == CMD_GET_ALLOWED_REPEAT_FREQ) {
    int i = 0;
    out_frame[i++] = RESP_ALLOWED_REPEAT_FREQ;
    for (int k = 0; k < sizeof(repeat_freq_ranges)/sizeof(repeat_freq_ranges[0]) && i + 8 < sizeof(out_frame); k++) {
      auto r = &repeat_freq_ranges[k];
      memcpy(&out_frame[i], &r->lower_freq, 4); i += 4;
      memcpy(&out_frame[i], &r->upper_freq, 4); i += 4;
    }
    _serial->writeFrame(out_frame, i);
  } else {
    writeErrFrame(ERR_CODE_UNSUPPORTED_CMD);
    MESH_DEBUG_PRINTLN("ERROR: unknown command: %02X", cmd_frame[0]);
  }
}

void MyMesh::enterCLIRescue() {
  _cli_rescue = true;
  cli_command[0] = 0;
  Serial.println("========= CLI Rescue =========");
}

void MyMesh::checkCLIRescueCmd() {
  int len = strlen(cli_command);
  while (Serial.available() && len < sizeof(cli_command)-1) {
    char c = Serial.read();
    if (c != '\n') {
      cli_command[len++] = c;
      cli_command[len] = 0;
    }
    Serial.print(c);  // echo
  }
  if (len == sizeof(cli_command)-1) {  // command buffer full
    cli_command[sizeof(cli_command)-1] = '\r';
  }

  if (len > 0 && cli_command[len - 1] == '\r') {  // received complete line
    cli_command[len - 1] = 0;  // replace newline with C string null terminator

    if (memcmp(cli_command, "set ", 4) == 0) {
      const char* config = &cli_command[4];
      if (memcmp(config, "pin ", 4) == 0) {
        _prefs.ble_pin = atoi(&config[4]);
        savePrefs();
        Serial.printf("  > pin is now %06d\n", _prefs.ble_pin);
      } else {
        Serial.printf("  Error: unknown config: %s\n", config);
      }
    } else if (strcmp(cli_command, "qm.list") == 0) {
      printQuickMessages();
    } else if (memcmp(cli_command, "qm.set ", 7) == 0) {
      char* cursor = &cli_command[7];
      int slot = atoi(cursor);
      while (*cursor && *cursor != ' ') cursor++;
      while (*cursor == ' ') cursor++;
      if (slot < 1 || slot > QUICK_MESSAGE_SLOTS || !cursor[0]) {
        Serial.printf("  Usage: qm.set <1-%u> <text>\n", (unsigned int)QUICK_MESSAGE_SLOTS);
      } else {
        StrHelper::strncpy(quick_messages[slot - 1], cursor, sizeof(quick_messages[slot - 1]));
        if (saveQuickMessages()) {
          Serial.printf("  > qm %d saved\n", slot);
        } else {
          Serial.println("  Error: save failed");
        }
      }
    } else if (memcmp(cli_command, "qm.clear ", 9) == 0) {
      int slot = atoi(&cli_command[9]);
      if (slot < 1 || slot > QUICK_MESSAGE_SLOTS) {
        Serial.printf("  Usage: qm.clear <1-%u>\n", (unsigned int)QUICK_MESSAGE_SLOTS);
      } else {
        quick_messages[slot - 1][0] = 0;
        if (saveQuickMessages()) {
          Serial.printf("  > qm %d cleared\n", slot);
        } else {
          Serial.println("  Error: save failed");
        }
      }
    } else if (strcmp(cli_command, "qm.reset") == 0) {
      resetQuickMessages();
      if (saveQuickMessages()) {
        Serial.println("  > quick messages reset");
      } else {
        Serial.println("  Error: save failed");
      }
    } else if (strcmp(cli_command, "help") == 0 || strcmp(cli_command, "?") == 0) {
      Serial.println("Commands:");
      Serial.println("  qm.list");
      Serial.printf("  qm.set <1-%u> <text>\n", (unsigned int)QUICK_MESSAGE_SLOTS);
      Serial.printf("  qm.clear <1-%u>\n", (unsigned int)QUICK_MESSAGE_SLOTS);
      Serial.println("  qm.reset");
      Serial.println("  set pin <123456>");
      Serial.println("  ls [UserData/|ExtraFS/]");
      Serial.println("  cat UserData/<file>");
      Serial.println("  rm <file>");
      Serial.println("  reboot");
    } else if (strcmp(cli_command, "rebuild") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        _store->saveMainIdentity(self_id);
        savePrefs();
        saveContacts();
        saveChannels();
        Serial.println("  > erase and rebuild done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (strcmp(cli_command, "erase") == 0) {
      bool success = _store->formatFileSystem();
      if (success) {
        Serial.println("  > erase done");
      } else {
        Serial.println("  Error: erase failed");
      }
    } else if (memcmp(cli_command, "ls", 2) == 0) {

      // get path from command e.g: "ls /adafruit"
      const char *path = &cli_command[3];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }
      Serial.printf("Listing files in %s\n", path);

      // log each file and directory
      File root = _store->openRead(path);
      if (is_fs2 == false) {
        if (root) {
          File file = root.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  UserData%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] UserData%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root.openNextFile();
          }
          root.close();
        }
      }

      if (is_fs2 == true || strlen(path) == 0 || strcmp(path, "/") == 0) {
        if (_store->getSecondaryFS() != nullptr) {
          File root2 = _store->openRead(_store->getSecondaryFS(), path);
          File file = root2.openNextFile();
          while (file) {
            if (file.isDirectory()) {
              Serial.printf("[dir]  ExtraFS%s/%s\n", path, file.name());
            } else {
              Serial.printf("[file] ExtraFS%s/%s (%d bytes)\n", path, file.name(), file.size());
            }
            // move to next file
            file = root2.openNextFile();
          }
          root2.close();
        }
      }
    } else if (memcmp(cli_command, "cat", 3) == 0) {

      // get path from command e.g: "cat /contacts3"
      const char *path = &cli_command[4];

      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      } else {
        Serial.println("Invalid path provided, must start with UserData/ or ExtraFS/");
        cli_command[0] = 0;
        return;
      }

      // log file content as hex
      File file = _store->openRead(path);
      if (is_fs2 == true) {
        file = _store->openRead(_store->getSecondaryFS(), path);
      }
      if(file){

        // get file content
        int file_size = file.available();
        uint8_t buffer[file_size];
        file.read(buffer, file_size);

        // print hex
        mesh::Utils::printHex(Serial, buffer, file_size);
        Serial.print("\n");

        file.close();

      }

    } else if (memcmp(cli_command, "rm ", 3) == 0) {
      // get path from command e.g: "rm /adv_blobs"
      const char *path = &cli_command[3];
      MESH_DEBUG_PRINTLN("Removing file: %s", path);
      // ensure path is not empty, or root dir
      if(!path || strlen(path) == 0 || strcmp(path, "/") == 0){
        Serial.println("Invalid path provided");
      } else {
      bool is_fs2 = false;
      if (memcmp(path, "UserData/", 9) == 0) {
        path += 8; // skip "UserData"
      } else if (memcmp(path, "ExtraFS/", 8) == 0) {
        path += 7; // skip "ExtraFS"
        is_fs2 = true;
      }

        // remove file
        bool removed;
        if (is_fs2) {
          MESH_DEBUG_PRINTLN("Removing file from ExtraFS: %s", path);
          removed = _store->removeFile(_store->getSecondaryFS(), path);
        } else {
          MESH_DEBUG_PRINTLN("Removing file from UserData: %s", path);
          removed = _store->removeFile(path);
        }
        if(removed){
          Serial.println("File removed");
        } else {
          Serial.println("Failed to remove file");
        }

      }

    } else if (strcmp(cli_command, "reboot") == 0) {
      board.reboot();  // doesn't return
    } else {
      Serial.println("  Error: unknown command");
    }

    cli_command[0] = 0;  // reset command buffer
  }
}

void MyMesh::checkSerialInterface() {
  size_t len = _serial->checkRecvFrame(cmd_frame);
  if (len > 0) {
    handleCmdFrame(len);
  } else if (_iter_started              // check if our ContactsIterator is 'running'
             && !_serial->isWriteBusy() // don't spam the Serial Interface too quickly!
  ) {
    ContactInfo contact;
    if (_iter.hasNext(this, contact)) {
      if (contact.lastmod > _iter_filter_since) { // apply the 'since' filter
        writeContactRespFrame(RESP_CODE_CONTACT, contact);
        if (contact.lastmod > _most_recent_lastmod) {
          _most_recent_lastmod = contact.lastmod; // save for the RESP_CODE_END_OF_CONTACTS frame
        }
      }
    } else { // EOF
      out_frame[0] = RESP_CODE_END_OF_CONTACTS;
      memcpy(&out_frame[1], &_most_recent_lastmod,
             4); // include the most recent lastmod, so app can update their 'since'
      _serial->writeFrame(out_frame, 5);
      _iter_started = false;
    }
  //} else if (!_serial->isWriteBusy()) {
  //  checkConnections();    // TODO - deprecate the 'Connections' stuff
  }
}

void MyMesh::loop() {
  BaseChatMesh::loop();

  checkCLIRescueCmd();
  if (!_cli_rescue) checkSerialInterface();

  // is there are pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    saveContacts();
    dirty_contacts_expiry = 0;
  }

#ifdef DISPLAY_CLASS
  if (_ui) _ui->setHasConnection(_serial->isConnected());
#endif
}

bool MyMesh::advert() {
  mesh::Packet* pkt;
  if (_prefs.advert_loc_policy == ADVERT_LOC_NONE) {
    pkt = createSelfAdvert(_prefs.node_name);
  } else {
    pkt = createSelfAdvert(_prefs.node_name, sensors.node_lat, sensors.node_lon);
  }
  if (pkt) {
    sendZeroHop(pkt);
    return true;
  } else {
    return false;
  }
}

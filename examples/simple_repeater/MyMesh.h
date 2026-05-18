#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include <RTClib.h>
#include <target.h>

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  #include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
  #include <LittleFS.h>
#elif defined(ESP32)
  #include <SPIFFS.h>
#endif

#ifdef WITH_RS232_BRIDGE
#include "helpers/bridges/RS232Bridge.h"
#define WITH_BRIDGE
#endif

#ifdef WITH_ESPNOW_BRIDGE
#include "helpers/bridges/ESPNowBridge.h"
#define WITH_BRIDGE
#endif

#include <helpers/AdvertDataHelpers.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/ClientACL.h>
#include <helpers/CommonCLI.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <helpers/StatsFormatHelper.h>
#include <helpers/TxtDataHelpers.h>
#include <helpers/RegionMap.h>
#ifdef WITH_MQTT_OBSERVER
#include <helpers/MQTTObserver.h>
#endif
#include "RateLimiter.h"

#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
class AsyncWebServer;
#endif

#ifdef WITH_BRIDGE
extern AbstractBridge* bridge;
#endif

struct RepeaterStats {
  uint16_t batt_milli_volts;
  uint16_t curr_tx_queue_len;
  int16_t  noise_floor;
  int16_t  last_rssi;
  uint32_t n_packets_recv;
  uint32_t n_packets_sent;
  uint32_t total_air_time_secs;
  uint32_t total_up_time_secs;
  uint32_t n_sent_flood, n_sent_direct;
  uint32_t n_recv_flood, n_recv_direct;
  uint16_t err_events;                // was 'n_full_events'
  int16_t  last_snr;   // x 4
  uint16_t n_direct_dups, n_flood_dups;
  uint32_t total_rx_air_time_secs;
  uint32_t n_recv_errors;
};

#ifndef MAX_CLIENTS
  #define MAX_CLIENTS           32
#endif

struct NeighbourInfo {
  mesh::Identity id;
  uint32_t advert_timestamp;
  uint32_t heard_timestamp;
  int8_t snr; // multiplied by 4, user should divide to get float value
};

#define OBSERVER_PATH_HISTORY_SIZE 24
#define OBSERVER_PATH_TEXT_SIZE    36
#define OBSERVER_PATH_KEY_SIZE     (MAX_PATH_SIZE * 2 + 4)
#define OBSERVER_PATH_DISPLAY_HOPS 3
#define OBSERVER_LAST_HOP_HISTORY_SIZE 24
#define OBSERVER_LAST_HOP_TEXT_SIZE    8
#define OBSERVER_SAVEPOINT_MAX         10
#define OBSERVER_SAVEPOINT_LINE_SIZE   40
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  #define OBSERVER_CLOCK_SYNC_SAMPLES    8
  #define OBSERVER_CLOCK_SYNC_REQUIRED   5
  #define OBSERVER_CLOCK_SYNC_DISTINCT   2
#endif

struct ObserverPathInfo {
  unsigned long seen_at;
  uint16_t count;
  char text[OBSERVER_PATH_TEXT_SIZE];
  char key[OBSERVER_PATH_KEY_SIZE];
};

struct ObserverLastHopInfo {
  unsigned long seen_at;
  int8_t last_snr;
  int8_t max_snr;
  char text[OBSERVER_LAST_HOP_TEXT_SIZE];
  char key[OBSERVER_LAST_HOP_TEXT_SIZE];
};

#ifdef ENABLE_OBSERVER_CLOCK_SYNC
struct ObserverClockSyncSample {
  bool used;
  uint32_t timestamp;
  uint8_t pub_key[PUB_KEY_SIZE];
};
#endif

#ifndef FIRMWARE_BUILD_DATE
  #define FIRMWARE_BUILD_DATE   "19 Apr 2026"
#endif

#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION   "v1.15.0"
#endif

#define FIRMWARE_ROLE "repeater"

#define PACKET_LOG_FILE  "/packet_log"

class MyMesh : public mesh::Mesh, public CommonCLICallbacks {
  FILESYSTEM* _fs;
  uint32_t last_millis;
  uint64_t uptime_millis;
  unsigned long next_local_advert, next_flood_advert;
  bool _logging;
  NodePrefs _prefs;
  ClientACL  acl;
  CommonCLI _cli;
  uint8_t reply_data[MAX_PACKET_PAYLOAD];
  uint8_t reply_path[MAX_PATH_SIZE];
  int8_t  reply_path_len;
  uint8_t reply_path_hash_size;
  TransportKeyStore key_store;
  RegionMap region_map, temp_map;
  RegionEntry* load_stack[8];
  RegionEntry* recv_pkt_region;
  TransportKey default_scope;
  RateLimiter discover_limiter, anon_limiter;
  uint32_t pending_discover_tag;
  unsigned long pending_discover_until;
  bool region_load_active;
  unsigned long dirty_contacts_expiry;
#if MAX_NEIGHBOURS
  NeighbourInfo neighbours[MAX_NEIGHBOURS];
#endif
  CayenneLPP telemetry;
  unsigned long set_radio_at, revert_radio_at;
  float pending_freq;
  float pending_bw;
  uint8_t pending_sf;
  uint8_t pending_cr;
  int  matching_peer_indexes[MAX_CLIENTS];
  uint32_t observer_rx_packets;
  uint32_t observer_mqtt_published;
  unsigned long observer_next_health_at;
  uint32_t observer_prev_health_uptime_s;
  uint32_t observer_prev_health_rx;
  uint32_t observer_prev_health_free_heap;
  uint32_t observer_prev_health_min_heap;
  uint8_t observer_health_screen;
  uint8_t observer_prev_health_screen;
  bool observer_prev_health_valid;
  mesh::MainBoard* _board;
  ObserverPathInfo observer_paths[OBSERVER_PATH_HISTORY_SIZE];
  ObserverLastHopInfo observer_last_hops[OBSERVER_LAST_HOP_HISTORY_SIZE];
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  ObserverClockSyncSample observer_clock_sync_samples[OBSERVER_CLOCK_SYNC_SAMPLES];
#endif
  uint8_t observer_path_next;
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  uint8_t observer_clock_sync_next;
  uint8_t observer_clock_sync_count;
  bool observer_clock_synced;
  bool observer_clock_sync_done;
  uint32_t observer_clock_synced_at;
#endif
#if defined(WITH_RS232_BRIDGE)
  RS232Bridge bridge;
#elif defined(WITH_ESPNOW_BRIDGE)
  ESPNowBridge bridge;
#endif
#ifdef WITH_MQTT_OBSERVER
  MQTTObserver mqtt_observer;
#endif
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  AsyncWebServer* observer_web_server;
  bool observer_web_ap_running;
  bool observer_web_sta_running;
  unsigned long observer_web_sta_next_attempt;
  unsigned long observer_web_next_rollover;
  uint32_t observer_web_prev_rx_total;
  uint32_t observer_web_prev_air_ms;
  uint16_t observer_web_rx_bins[12];
  uint16_t observer_web_air_bins[12];
  uint8_t observer_web_bin_index;
#endif

  void putNeighbour(const mesh::Identity& id, uint32_t timestamp, float snr);
  uint8_t handleLoginReq(const mesh::Identity& sender, const uint8_t* secret, uint32_t sender_timestamp, const uint8_t* data, bool is_flood);
  uint8_t handleAnonRegionsReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data);
  uint8_t handleAnonOwnerReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data);
  uint8_t handleAnonClockReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data);
  int handleRequest(ClientInfo* sender, uint32_t sender_timestamp, uint8_t* payload, size_t payload_len);
  mesh::Packet* createSelfAdvert();

  File openAppend(const char* fname);
  bool isLooped(const mesh::Packet* packet, const uint8_t max_counters[]);
  void rememberObserverPath(const mesh::Packet* packet);
  void rememberObserverLastHop(const mesh::Packet* packet, int8_t snr_x4);
  void observeClockSyncSample(const mesh::Identity& id, uint32_t timestamp);
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  void setupObserverWebRoutes();
  String buildObserverWebStatusJson() const;
  String buildObserverWebMonitorJson() const;
  String buildObserverSavepointListCsv() const;
  void updateObserverWebMetrics();
#endif

protected:
  float getAirtimeBudgetFactor() const override {
    return _prefs.airtime_factor;
  }

  bool allowPacketForward(const mesh::Packet* packet) override;
  const char* getLogDateTime() override;
  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;

  void logRx(mesh::Packet* pkt, int len, float score) override;
  void logTx(mesh::Packet* pkt, int len) override;
  void logTxFail(mesh::Packet* pkt, int len) override;
  int calcRxDelay(float score, uint32_t air_time) const override;

  uint32_t getRetransmitDelay(const mesh::Packet* packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet* packet) override;

  int getInterferenceThreshold() const override {
    return _prefs.interference_threshold;
  }
  int getAGCResetInterval() const override {
    return ((int)_prefs.agc_reset_interval) * 4000;   // milliseconds
  }
  uint8_t getExtraAckTransmitCount() const override {
    return _prefs.multi_acks;
  }

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled?"1":"0");
  }
#endif

  bool filterRecvFloodPacket(mesh::Packet* pkt) override;

  void onAnonDataRecv(mesh::Packet* packet, const uint8_t* secret, const mesh::Identity& sender, uint8_t* data, size_t len) override;
  int searchPeersByHash(const uint8_t* hash) override;
  void getPeerSharedSecret(uint8_t* dest_secret, int peer_idx) override;
  void onAdvertRecv(mesh::Packet* packet, const mesh::Identity& id, uint32_t timestamp, const uint8_t* app_data, size_t app_data_len);
  void onPeerDataRecv(mesh::Packet* packet, uint8_t type, int sender_idx, const uint8_t* secret, uint8_t* data, size_t len) override;
  bool onPeerPathRecv(mesh::Packet* packet, int sender_idx, const uint8_t* secret, uint8_t* path, uint8_t path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onControlDataRecv(mesh::Packet* packet) override;

  void sendFloodReply(mesh::Packet* packet, unsigned long delay_millis, uint8_t path_hash_size);

public:
  MyMesh(mesh::MainBoard& board, mesh::Radio& radio, mesh::MillisecondClock& ms, mesh::RNG& rng, mesh::RTCClock& rtc, mesh::MeshTables& tables);

  void begin(FILESYSTEM* fs);
  void sendNodeDiscoverReq();
  const char* getFirmwareVer() override { return FIRMWARE_VERSION; }
  const char* getBuildDate() override { return FIRMWARE_BUILD_DATE; }
  const char* getRole() override { return FIRMWARE_ROLE; }
  const char* getNodeName() { return _prefs.node_name; }
  NodePrefs* getNodePrefs() {
    return &_prefs;
  }
  uint32_t getObserverRxPackets() const { return observer_rx_packets; }
  uint32_t getObserverRxAirTimeMillis() const { return getReceiveAirTime(); }
  uint32_t getObserverMqttPublished() const { return observer_mqtt_published; }
  const char* getObserverMqttHost() const {
#ifdef WITH_MQTT_OBSERVER
    return _prefs.mqtt_host;
#else
    return "";
#endif
  }
  uint16_t getObserverMqttPort() const {
#ifdef WITH_MQTT_OBSERVER
    return _prefs.mqtt_port;
#else
    return 0;
#endif
  }
  const char* getObserverMqttStatus();
  bool isObserverMqttEnabled() const {
#ifdef WITH_MQTT_OBSERVER
    return _prefs.mqtt_enabled;
#else
    return false;
#endif
  }
  const char* getObserverMqttLastError() const;
  uint32_t getObserverMqttPublishFailures() const;
  uint32_t getObserverMqttWifiFailures() const;
  uint32_t getObserverMqttConnectFailures() const;
  int getObserverMqttState() const;
  void setObserverMqttEnabled(bool enabled);
  bool toggleObserverMqttEnabled();
  uint16_t getObserverBattMilliVolts() const { return _board ? _board->getBattMilliVolts() : 0; }
  int getObserverNoiseFloor() const { return _radio ? _radio->getNoiseFloor() : 0; }
  float getObserverLastSnr() const { return _radio ? _radio->getLastSNR() : 0.0f; }
  void getObserverDiagLine(char* dest, size_t dest_size) const;
  void getObserverHealthLine(char* dest, size_t dest_size) const;
  void setObserverHealthScreen(uint8_t screen);
  bool getObserverPathLine(uint8_t index, char* dest, size_t dest_size) const;
  bool getObserverLatestPathLine(char* dest, size_t dest_size) const;
  bool getObserverLastHopLine(uint8_t index, char* dest, size_t dest_size) const;
#ifdef ENABLE_OBSERVER_SAVEPOINTS
  bool getObserverSavepointLine(uint8_t index, char* dest, size_t dest_size) const;
#else
  bool getObserverSavepointLine(uint8_t, char*, size_t) const { return false; }
#endif
  void getObserverClockSyncStatus(char* dest, size_t dest_size) const;
#ifdef ENABLE_OBSERVER_SAVEPOINTS
  bool createObserverSavepoint(const uint16_t* activity_bins, const uint16_t* airtime_bins, uint8_t bin_count,
                               uint8_t newest_bin, char* status, size_t status_size);
#else
  bool createObserverSavepoint(const uint16_t*, const uint16_t*, uint8_t, uint8_t, char* status, size_t status_size) {
    if (status && status_size) snprintf(status, status_size, "SP n/a");
    return false;
  }
#endif
  void resetObserverLiveStats();
  void hibernate();
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  bool startObserverWebAp(char* status, size_t status_size);
  void stopObserverWebAp();
  bool startObserverWebStaView(char* status, size_t status_size);
  void stopObserverWebStaView();
  bool toggleObserverWebStaView(char* status, size_t status_size);
  bool toggleObserverWebAp(char* status, size_t status_size);
  void getObserverWebApLine(char* dest, size_t dest_size) const;
  void setObserverPosition(double lat, double lon);
  bool isObserverWebApRunning() const { return observer_web_ap_running; }
#endif

  void savePrefs() override {
    _cli.savePrefs(_fs);
  }

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis, uint8_t path_hash_size);
  void updateObserverHealth();

  // CommonCLICallbacks
  void applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) override;
  bool formatFileSystem() override;
  void sendSelfAdvertisement(int delay_millis, bool flood) override;
  void updateAdvertTimer() override;
  void updateFloodAdvertTimer() override;

  void setLoggingOn(bool enable) override { _logging = enable; }

  void eraseLogFile() override {
    _fs->remove(PACKET_LOG_FILE);
  }

  void dumpLogFile() override;
  void setTxPower(int8_t power_dbm) override;
  void formatNeighborsReply(char *reply) override;
  void removeNeighbor(const uint8_t* pubkey, int key_len) override;
  void formatStatsReply(char *reply) override;
  void formatRadioStatsReply(char *reply) override;
  void formatPacketStatsReply(char *reply) override;
  void startRegionsLoad() override;
  bool saveRegions() override;
  void onDefaultRegionChanged(const RegionEntry* r) override;

  mesh::LocalIdentity& getSelfId() override { return self_id; }

  void saveIdentity(const mesh::LocalIdentity& new_id) override;
  void clearStats() override;

  void handleCommand(uint32_t sender_timestamp, char* command, char* reply);
  void loop();

#if defined(WITH_BRIDGE)
  void setBridgeState(bool enable) override {
    if (enable == bridge.isRunning()) return;
    if (enable)
    {
      bridge.begin();
    }
    else 
    {
      bridge.end();
    }
  }

  void restartBridge() override {
    if (!bridge.isRunning()) return;
    bridge.end();
    bridge.begin();
  }
#endif

#ifdef WITH_MQTT_OBSERVER
  void setMqttObserverState(bool enable) override {
    if (enable == mqtt_observer.isRunning()) return;
    if (enable) {
      mqtt_observer.begin();
    } else {
      mqtt_observer.end();
    }
  }

  void restartMqttObserver() override {
    if (!mqtt_observer.isRunning()) return;
    mqtt_observer.end();
    mqtt_observer.begin();
  }
#endif

  // To check if there is pending work
  bool hasPendingWork() const;

#if defined(USE_SX1262) || defined(USE_SX1268)
  void setRxBoostedGain(bool enable) override;
#endif
};

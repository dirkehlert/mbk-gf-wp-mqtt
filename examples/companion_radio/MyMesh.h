#pragma once

#include <Arduino.h>
#include <Mesh.h>
#include "AbstractUITask.h"

/*------------ Frame Protocol --------------*/
#define FIRMWARE_VER_CODE 11

#ifndef FIRMWARE_BUILD_DATE
#define FIRMWARE_BUILD_DATE "19 Apr 2026"
#endif

#ifndef STOCK_FIRMWARE_VERSION
#define STOCK_FIRMWARE_VERSION "v1.15.0"
#endif

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "Fieldtest by Moorbock 1.0 based on " STOCK_FIRMWARE_VERSION
#endif

#ifndef CLIENT_FIRMWARE_VERSION
#define CLIENT_FIRMWARE_VERSION "Moorbock FT1 v1.15"
#endif

#ifndef DISPLAY_FIRMWARE_VERSION
#define DISPLAY_FIRMWARE_VERSION "FT 1.0"
#endif

#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
#include <InternalFileSystem.h>
#elif defined(RP2040_PLATFORM)
#include <LittleFS.h>
#elif defined(ESP32)
#include <SPIFFS.h>
#endif

#include "DataStore.h"
#include "NodePrefs.h"

#include <RTClib.h>
#include <helpers/ArduinoHelpers.h>
#include <helpers/BaseSerialInterface.h>
#include <helpers/IdentityStore.h>
#include <helpers/SimpleMeshTables.h>
#include <helpers/StaticPoolPacketManager.h>
#include <target.h>

/* ---------------------------------- CONFIGURATION ------------------------------------- */

#ifndef LORA_FREQ
#define LORA_FREQ 915.0
#endif
#ifndef LORA_BW
#define LORA_BW 250
#endif
#ifndef LORA_SF
#define LORA_SF 10
#endif
#ifndef LORA_CR
#define LORA_CR 5
#endif
#ifndef LORA_TX_POWER
#define LORA_TX_POWER 20
#endif
#ifndef MAX_LORA_TX_POWER
#define MAX_LORA_TX_POWER LORA_TX_POWER
#endif

#ifndef MAX_CONTACTS
#define MAX_CONTACTS 100
#endif

#ifndef OFFLINE_QUEUE_SIZE
#define OFFLINE_QUEUE_SIZE 16
#endif

#ifndef BLE_NAME_PREFIX
#define BLE_NAME_PREFIX "MeshCore-"
#endif

#include <helpers/BaseChatMesh.h>
#include <helpers/TransportKeyStore.h>

/* -------------------------------------------------------------------------------------- */

#define REQ_TYPE_GET_STATUS             0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE             0x02
#define REQ_TYPE_GET_TELEMETRY_DATA     0x03

struct AdvertPath {
  uint8_t pubkey_prefix[7];
  uint8_t path_len;
  char    name[32];
  uint32_t recv_timestamp;
  uint8_t path[MAX_PATH_SIZE];
};

enum QuickSendTargetType : uint8_t {
  QUICK_SEND_CHANNEL = 0,
  QUICK_SEND_CONTACT = 1
};

struct QuickSendTarget {
  QuickSendTargetType type;
  uint8_t index;
  uint8_t pubkey_prefix[7];
  char name[32];
};

#define MONITOR_PATH_HISTORY_SIZE       24
#define MONITOR_PATH_TEXT_SIZE          36
#define MONITOR_PATH_KEY_SIZE           (MAX_PATH_SIZE * 2 + 4)
#define MONITOR_PATH_DISPLAY_HOPS       3
#define MONITOR_LAST_HOP_HISTORY_SIZE   24
#define MONITOR_LAST_HOP_TEXT_SIZE      8
#define MONITOR_ACTIVITY_BINS           12
#define MONITOR_ACTIVITY_BIN_MILLIS     60000

struct MonitorPathInfo {
  unsigned long seen_at;
  uint16_t count;
  char text[MONITOR_PATH_TEXT_SIZE];
  char key[MONITOR_PATH_KEY_SIZE];
};

struct MonitorLastHopInfo {
  unsigned long seen_at;
  int8_t last_snr;
  int8_t max_snr;
  char text[MONITOR_LAST_HOP_TEXT_SIZE];
  char key[MONITOR_LAST_HOP_TEXT_SIZE];
};

#define SCOPE_CACHE_SIZE                24
#define SCOPE_NAME_SIZE                 31
#define SCOPE_SOURCE_SIZE               24
#define SCOPE_TARGET_SIZE               8

struct ScopeInfo {
  char name[SCOPE_NAME_SIZE];
  char source[SCOPE_SOURCE_SIZE];
  uint32_t seen_timestamp;
  uint32_t rx_count;
};

struct ScopeTarget {
  mesh::Identity id;
  char name[32];
  uint8_t path_len;
  uint8_t path[MAX_PATH_SIZE];
  uint32_t seen_timestamp;
};

class MyMesh : public BaseChatMesh, public DataStoreHost {
public:
  MyMesh(mesh::Radio &radio, mesh::RNG &rng, mesh::RTCClock &rtc, SimpleMeshTables &tables, DataStore& store, AbstractUITask* ui=NULL);

  void begin(bool has_display);
  void startInterface(BaseSerialInterface &serial);

  const char *getNodeName();
  NodePrefs *getNodePrefs();
  uint32_t getBLEPin();

  void loop();
  void handleCmdFrame(size_t len);
  bool advert();
  void enterCLIRescue();

  int  getRecentlyHeard(AdvertPath dest[], int max_num);
  int  getQuickSendTargets(QuickSendTarget dest[], int max_num);
  uint8_t getQuickMessageCount() const;
  const char* getQuickMessage(uint8_t index) const;
  bool sendQuickText(const QuickSendTarget& target, const char* text, bool* sent_flood = NULL);
  bool sendQuickReply(const uint8_t* pubkey_prefix, uint8_t prefix_len, const char* text, bool* sent_flood = NULL);
  bool sendQuickChannelReply(uint8_t channel_idx, const char* mention, const char* text);
  uint32_t getMonitorRxPackets() const { return monitor_rx_packets; }
  int getMonitorNoiseFloor() const { return _radio->getNoiseFloor(); }
  float getMonitorLastSnr() const { return monitor_last_snr_x4 / 4.0f; }
  bool getMonitorPathLine(uint8_t index, char* dest, size_t dest_size) const;
  bool getMonitorPathMetaLine(uint8_t index, char* dest, size_t dest_size) const;
  bool getMonitorLatestPathLine(char* dest, size_t dest_size) const;
  bool getMonitorLastHopLine(uint8_t index, char* dest, size_t dest_size) const;
  uint8_t getMonitorActivity(uint16_t* dest, uint8_t max_count);
  uint8_t getMonitorAirtime(uint16_t* dest, uint8_t max_count);
  uint8_t queryNearbyScopes();
  uint8_t getScopeInfo(ScopeInfo dest[], uint8_t max_count) const;
  void formatPacketScope(const mesh::Packet* packet, char* dest, size_t dest_size) const;
  uint8_t getLastScopeScanSent() const { return last_scope_scan_sent; }
  uint8_t getScopeResponseCount() const { return scope_response_count; }
  uint8_t getScopeDiscoverResponseCount() const { return scope_discover_response_count; }
  uint8_t getScopeEmptyResponseCount() const { return scope_empty_response_count; }

protected:
  float getAirtimeBudgetFactor() const override;
  int getInterferenceThreshold() const override;
  int calcRxDelay(float score, uint32_t air_time) const override;
  uint32_t getRetransmitDelay(const mesh::Packet *packet) override;
  uint32_t getDirectRetransmitDelay(const mesh::Packet *packet) override;
  uint8_t getExtraAckTransmitCount() const override;
  bool filterRecvFloodPacket(mesh::Packet* packet) override;
  bool allowPacketForward(const mesh::Packet* packet) override;

  void sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis);
  void sendFloodScoped(const ContactInfo& recipient, mesh::Packet* pkt, uint32_t delay_millis=0) override;
  void sendFloodScoped(const mesh::GroupChannel& channel, mesh::Packet* pkt, uint32_t delay_millis=0) override;

  void logRxRaw(float snr, float rssi, const uint8_t raw[], int len) override;
  void logRx(mesh::Packet* packet, int len, float score) override;
  bool isAutoAddEnabled() const override;
  bool shouldAutoAddContactType(uint8_t type) const override;
  bool shouldOverwriteWhenFull() const override;
  uint8_t getAutoAddMaxHops() const override;
  void onContactsFull() override;
  void onContactOverwrite(const uint8_t* pub_key) override;
  bool onContactPathRecv(ContactInfo& from, uint8_t* in_path, uint8_t in_path_len, uint8_t* out_path, uint8_t out_path_len, uint8_t extra_type, uint8_t* extra, uint8_t extra_len) override;
  void onDiscoveredContact(ContactInfo &contact, bool is_new, uint8_t path_len, const uint8_t* path) override;
  void onContactPathUpdated(const ContactInfo &contact) override;
  ContactInfo* processAck(const uint8_t *data) override;
  void queueMessage(const ContactInfo &from, uint8_t txt_type, mesh::Packet *pkt, uint32_t sender_timestamp,
                    const uint8_t *extra, int extra_len, const char *text);

  void onMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                     const char *text) override;
  void onCommandDataRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                         const char *text) override;
  void onSignedMessageRecv(const ContactInfo &from, mesh::Packet *pkt, uint32_t sender_timestamp,
                           const uint8_t *sender_prefix, const char *text) override;
  void onChannelMessageRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint32_t timestamp,
                            const char *text) override;
  void onChannelDataRecv(const mesh::GroupChannel &channel, mesh::Packet *pkt, uint16_t data_type,
                         const uint8_t *data, size_t data_len) override;

  uint8_t onContactRequest(const ContactInfo &contact, uint32_t sender_timestamp, const uint8_t *data,
                           uint8_t len, uint8_t *reply) override;
  void onContactResponse(const ContactInfo &contact, const uint8_t *data, uint8_t len) override;
  void onControlDataRecv(mesh::Packet *packet) override;
  void onRawDataRecv(mesh::Packet *packet) override;
  void onTraceRecv(mesh::Packet *packet, uint32_t tag, uint32_t auth_code, uint8_t flags,
                   const uint8_t *path_snrs, const uint8_t *path_hashes, uint8_t path_len) override;

  uint32_t calcFloodTimeoutMillisFor(uint32_t pkt_airtime_millis) const override;
  uint32_t calcDirectTimeoutMillisFor(uint32_t pkt_airtime_millis, uint8_t path_len) const override;
  void onSendTimeout() override;

  // DataStoreHost methods
  bool onContactLoaded(const ContactInfo& contact) override { return addContact(contact); }
  bool getContactForSave(uint32_t idx, ContactInfo& contact) override { return getContactByIdx(idx, contact); }
  bool onChannelLoaded(uint8_t channel_idx, const ChannelDetails& ch) override { return setChannel(channel_idx, ch); }
  bool getChannelForSave(uint8_t channel_idx, ChannelDetails& ch) override { return getChannel(channel_idx, ch); }

  void clearPendingReqs() {
    pending_login = pending_status = pending_telemetry = pending_discovery = pending_req = 0;
  }

public:
  void savePrefs() { _store->savePrefs(_prefs, sensors.node_lat, sensors.node_lon); }

#if ENV_INCLUDE_GPS == 1
  void applyGpsPrefs() {
    sensors.setSettingValue("gps", _prefs.gps_enabled ? "1" : "0");
    if (_prefs.gps_interval > 0) {
      char interval_str[12];  // Max: 24 hours = 86400 seconds (5 digits + null)
      sprintf(interval_str, "%u", _prefs.gps_interval);
      sensors.setSettingValue("gps_interval", interval_str);
    }
  }
#endif

private:
  void writeOKFrame();
  void writeErrFrame(uint8_t err_code);
  void writeDisabledFrame();
  void writeContactRespFrame(uint8_t code, const ContactInfo &contact);
  void updateContactFromFrame(ContactInfo &contact, uint32_t& last_mod, const uint8_t *frame, int len);
  void addToOfflineQueue(const uint8_t frame[], int len);
  int getFromOfflineQueue(uint8_t frame[]);
  int getBlobByKey(const uint8_t key[], int key_len, uint8_t dest_buf[]) override { 
    return _store->getBlobByKey(key, key_len, dest_buf);
  }
  bool putBlobByKey(const uint8_t key[], int key_len, const uint8_t src_buf[], int len) override {
    return _store->putBlobByKey(key, key_len, src_buf, len);
  }

  void checkCLIRescueCmd();
  void checkSerialInterface();
  bool isValidClientRepeatFreq(uint32_t f) const;
  void resetQuickMessages();
  void loadQuickMessages();
  bool saveQuickMessages();
  void printQuickMessages();
  bool isMonitorHeatstripRequest(const char* text) const;
  bool isDisplayableText(const char* text) const;
  void formatMonitorHeatstripReply(char* dest, size_t dest_size) const;
  bool sendMonitorHeatstripReply(const ContactInfo& recipient);
  void rememberScopeName(const char* name, const char* source);
  void ingestScopeNames(const ContactInfo& contact, const char* names);
  bool isPendingScopeTag(uint32_t tag);
  void noteScopeRx(const char* name, const char* source);
  void notePacketScope(const mesh::Packet* packet);
  void rememberScopeTarget(const ContactInfo& contact, uint8_t path_len, const uint8_t* path);
  bool sendScopeRequest(ContactInfo& recipient, const uint8_t* request, size_t request_len, uint8_t& sent);
  ContactInfo* ensureScopeContact(const ContactInfo& candidate);
  void sendScopeDiscoverReq();
  void monitorRollActivity();
  void rememberMonitorPath(const mesh::Packet* packet);
  void rememberMonitorLastHop(const mesh::Packet* packet, int8_t snr_x4);

  // helpers, short-cuts
  void saveChannels() { _store->saveChannels(this); }
  void saveContacts() { _store->saveContacts(this); }

  DataStore* _store;
  NodePrefs _prefs;
  uint32_t pending_login;
  uint32_t pending_status;
  uint32_t pending_telemetry, pending_discovery;   // pending _TELEMETRY_REQ
  uint32_t pending_req;   // pending _BINARY_REQ
  uint32_t pending_scope_req[8];
  uint32_t pending_scope_discover_tag;
  BaseSerialInterface *_serial;
  AbstractUITask* _ui;

  ContactsIterator _iter;
  uint32_t _iter_filter_since;
  uint32_t _most_recent_lastmod;
  uint32_t _active_ble_pin;
  bool _iter_started;
  bool _cli_rescue;
  char cli_command[128];
  static const uint8_t QUICK_MESSAGE_SLOTS = 9;
  static const uint8_t QUICK_MESSAGE_SIZE = 80;
  char quick_messages[QUICK_MESSAGE_SLOTS][QUICK_MESSAGE_SIZE];
  uint8_t app_target_ver;
  uint8_t *sign_data;
  uint32_t sign_data_len;
  unsigned long dirty_contacts_expiry;

  TransportKey send_scope;

  uint8_t cmd_frame[MAX_FRAME_SIZE + 1];
  uint8_t out_frame[MAX_FRAME_SIZE + 1];
  CayenneLPP telemetry;

  struct Frame {
    uint8_t len;
    uint8_t buf[MAX_FRAME_SIZE];

    bool isChannelMsg() const;
  };
  int offline_queue_len;
  Frame offline_queue[OFFLINE_QUEUE_SIZE];

  struct AckTableEntry {
    unsigned long msg_sent;
    uint32_t ack;
    ContactInfo* contact;
  };
  #define EXPECTED_ACK_TABLE_SIZE 8
  AckTableEntry expected_ack_table[EXPECTED_ACK_TABLE_SIZE]; // circular table
  int next_ack_idx;

  #define ADVERT_PATH_TABLE_SIZE   16
  AdvertPath advert_paths[ADVERT_PATH_TABLE_SIZE]; // circular table

  uint32_t monitor_rx_packets;
  int8_t monitor_last_snr_x4;
  MonitorPathInfo monitor_paths[MONITOR_PATH_HISTORY_SIZE];
  MonitorLastHopInfo monitor_last_hops[MONITOR_LAST_HOP_HISTORY_SIZE];
  uint16_t monitor_activity_bins[MONITOR_ACTIVITY_BINS];
  uint16_t monitor_airtime_bins[MONITOR_ACTIVITY_BINS];
  uint8_t monitor_activity_index;
  unsigned long monitor_next_activity_rollover;
  ScopeInfo scope_cache[SCOPE_CACHE_SIZE];
  ScopeTarget scope_targets[SCOPE_TARGET_SIZE];
  uint8_t last_scope_scan_sent;
  uint8_t scope_response_count;
  uint8_t scope_discover_response_count;
  uint8_t scope_empty_response_count;
};

extern MyMesh the_mesh;

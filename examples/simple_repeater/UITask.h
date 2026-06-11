#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>
#include <Stream.h>
#include <string.h>

class MyMesh;

class UITask {
  static const uint8_t RX_ACTIVITY_BINS = 12;
  static const unsigned long RX_ACTIVITY_BIN_MILLIS = 60000;
  static const uint8_t HEAT_ROWS = 7;
  static const uint8_t HEAT_PATHS = 8;

  struct HeatRow {
    char rep[7];
    uint16_t mask;
    uint8_t position[HEAT_PATHS];
    uint8_t pc;
  };

  DisplayDriver* _display;
  unsigned long _next_read, _next_refresh, _auto_off;
  int _prevBtnState;
  NodePrefs* _node_prefs;
  MyMesh* _mesh;
  char _version_info[32];
  char _status[32];
  unsigned long _status_until;
  unsigned long _next_stats_rollover;
  uint32_t _prev_rx_total;
  uint32_t _prev_mqtt_total;
  uint32_t _render_rx_total;
  uint32_t _last_min_rx;
  uint32_t _last_min_mqtt;
  uint32_t _activity_prev_rx_total;
  uint32_t _activity_prev_air_ms;
  uint16_t _activity_bins[RX_ACTIVITY_BINS];
  uint16_t _airtime_bins[RX_ACTIVITY_BINS];
  uint8_t _activity_bin_index;
  unsigned long _next_activity_rollover;
  HeatRow _heat_rows[HEAT_ROWS];
  uint8_t _heat_row_count;
  bool _heat_valid;
  uint8_t _screen;
  bool _dump_mode;
  bool _remote_mode;

  void renderCurrScreen();
  void renderTopStats(int y = 0);
  void renderBattery(uint16_t batt_mv, int x, int y);
  void updateRxActivityBins();
  void renderRxActivityChart();
  void renderRxActivityHistogram();
  void renderNetworkLoadScreen();
  void updatePathHeatSnapshot();
  void renderPathHeatScreen();
  void syncObserverTotalsAfterReset();
public:
  UITask(DisplayDriver& display) : _display(&display) {
    _next_read = _next_refresh = _status_until = _next_stats_rollover = _next_activity_rollover = 0;
    _prev_rx_total = _prev_mqtt_total = _render_rx_total = _last_min_rx = _last_min_mqtt = 0;
    _activity_prev_rx_total = 0;
    _activity_prev_air_ms = 0;
    _activity_bin_index = 0;
    memset(_activity_bins, 0, sizeof(_activity_bins));
    memset(_airtime_bins, 0, sizeof(_airtime_bins));
    memset(_heat_rows, 0, sizeof(_heat_rows));
    _heat_row_count = 0;
    _heat_valid = false;
    _screen = 0;
    _dump_mode = false;
    _remote_mode = false;
  }
  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version, MyMesh* mesh = nullptr);
  static uint8_t screenCount() {
#ifdef FIELD_MONITOR_LITE
    return 5;
#else
    return 8;
#endif
  }
  static const char* screenName(uint8_t screen);
#ifdef ENABLE_DISPLAY_DUMP
  bool renderScreenForDump(uint8_t screen);
#endif

  void setRemoteMode(bool enabled);
  bool isRemoteMode() const { return _remote_mode; }
  uint8_t currentScreen() const { return _screen; }
  bool showScreen(uint8_t screen, const char* status = nullptr);
  bool showScreenByName(const char* name, const char* status = nullptr);
  void showNext();
  void showPrevious();
  void refreshNow(const char* status = nullptr);

  void loop();
};

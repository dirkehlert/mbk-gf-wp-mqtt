#pragma once

#include <helpers/ui/DisplayDriver.h>
#include <helpers/CommonCLI.h>
#include <Stream.h>
#include <string.h>

class MyMesh;

class UITask {
  static const uint8_t RX_ACTIVITY_BINS = 12;
  static const unsigned long RX_ACTIVITY_BIN_MILLIS = 60000;

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
  uint16_t _activity_bins[RX_ACTIVITY_BINS];
  uint8_t _activity_bin_index;
  unsigned long _next_activity_rollover;
  uint8_t _screen;
  bool _dump_mode;

  void renderCurrScreen();
  void renderTopStats(int y = 0);
  void renderBattery(uint16_t batt_mv, int x, int y);
  void updateRxActivityBins();
  void renderRxActivityChart();
  void renderRxActivityHistogram();
  void syncObserverTotalsAfterReset();
public:
  UITask(DisplayDriver& display) : _display(&display) {
    _next_read = _next_refresh = _status_until = _next_stats_rollover = _next_activity_rollover = 0;
    _prev_rx_total = _prev_mqtt_total = _render_rx_total = _last_min_rx = _last_min_mqtt = 0;
    _activity_prev_rx_total = 0;
    _activity_bin_index = 0;
    memset(_activity_bins, 0, sizeof(_activity_bins));
    _screen = 0;
    _dump_mode = false;
  }
  void begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version, MyMesh* mesh = nullptr);
#ifdef ENABLE_DISPLAY_DUMP
  static uint8_t screenCount() { return 5; }
  static const char* screenName(uint8_t screen);
  bool renderScreenForDump(uint8_t screen);
#endif

  void loop();
};

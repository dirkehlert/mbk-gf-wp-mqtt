#include "UITask.h"
#include "MyMesh.h"
#include <Arduino.h>
#ifdef ESP32
#include <Esp.h>
#endif
#include <helpers/CommonCLI.h>
#include <target.h>

#ifndef USER_BTN_PRESSED
#define USER_BTN_PRESSED LOW
#endif

#define AUTO_OFF_MILLIS      20000  // 20 seconds
#define BOOT_SCREEN_MILLIS   4000   // 4 seconds
#define STATS_WINDOW_MILLIS  60000
#define PATH_CHART_WIDTH     42
#define PATH_CHART_TOP       18
#define PATH_CHART_ROW_H     8

// 'meshcore', 128x13px
static const uint8_t meshcore_logo [] PROGMEM = {
    0x3c, 0x01, 0xe3, 0xff, 0xc7, 0xff, 0x8f, 0x03, 0x87, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 0x1f, 0xfe, 
    0x3c, 0x03, 0xe3, 0xff, 0xc7, 0xff, 0x8e, 0x03, 0x8f, 0xfe, 0x3f, 0xfe, 0x1f, 0xff, 0x1f, 0xfe, 
    0x3e, 0x03, 0xc3, 0xff, 0x8f, 0xff, 0x0e, 0x07, 0x8f, 0xfe, 0x7f, 0xfe, 0x1f, 0xff, 0x1f, 0xfc, 
    0x3e, 0x07, 0xc7, 0x80, 0x0e, 0x00, 0x0e, 0x07, 0x9e, 0x00, 0x78, 0x0e, 0x3c, 0x0f, 0x1c, 0x00, 
    0x3e, 0x0f, 0xc7, 0x80, 0x1e, 0x00, 0x0e, 0x07, 0x1e, 0x00, 0x70, 0x0e, 0x38, 0x0f, 0x3c, 0x00, 
    0x7f, 0x0f, 0xc7, 0xfe, 0x1f, 0xfc, 0x1f, 0xff, 0x1c, 0x00, 0x70, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x1f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x0e, 0x38, 0x0e, 0x3f, 0xf8, 
    0x7f, 0x3f, 0xc7, 0xfe, 0x0f, 0xff, 0x1f, 0xff, 0x1c, 0x00, 0xf0, 0x1e, 0x3f, 0xfe, 0x3f, 0xf0, 
    0x77, 0x3b, 0x87, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xfc, 0x38, 0x00, 
    0x77, 0xfb, 0x8f, 0x00, 0x00, 0x07, 0x1c, 0x0f, 0x3c, 0x00, 0xe0, 0x1c, 0x7f, 0xf8, 0x38, 0x00, 
    0x73, 0xf3, 0x8f, 0xff, 0x0f, 0xff, 0x1c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x78, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfe, 0x3c, 0x0e, 0x3f, 0xf8, 0xff, 0xfc, 0x70, 0x3c, 0x7f, 0xf8, 
    0xe3, 0xe3, 0x8f, 0xff, 0x1f, 0xfc, 0x3c, 0x0e, 0x1f, 0xf8, 0xff, 0xf8, 0x70, 0x3c, 0x7f, 0xf8, 
};

void UITask::begin(NodePrefs* node_prefs, const char* build_date, const char* firmware_version, MyMesh* mesh) {
  _prevBtnState = HIGH;
  _auto_off = millis() + AUTO_OFF_MILLIS;
  _node_prefs = node_prefs;
  _mesh = mesh;
  _status[0] = 0;
  _prev_rx_total = _mesh ? _mesh->getObserverRxPackets() : 0;
  _prev_mqtt_total = _mesh ? _mesh->getObserverMqttPublished() : 0;
  _render_rx_total = _prev_rx_total;
  _activity_prev_rx_total = _prev_rx_total;
  memset(_activity_bins, 0, sizeof(_activity_bins));
  _activity_bin_index = 0;
  _last_min_rx = 0;
  _last_min_mqtt = 0;
  _next_stats_rollover = millis() + STATS_WINDOW_MILLIS;
  _next_activity_rollover = millis() + RX_ACTIVITY_BIN_MILLIS;
  _display->turnOn();
#ifdef PIN_USER_BTN
  user_btn.begin();
#endif

  // strip off dash and commit hash by changing dash to null terminator
  // e.g: v1.2.3-abcdef -> v1.2.3
  char *version = strdup(firmware_version);
  char *dash = strchr(version, '-');
  if(dash){
    *dash = 0;
  }

  // v1.2.3 (1 Jan 2025)
  sprintf(_version_info, "%s (%s)", version, build_date);
}

void UITask::renderBattery(uint16_t batt_mv, int x, int y) {
  char tmp[16];
  _display->setColor(DisplayDriver::LIGHT);
  _display->setCursor(x, y);
  if (batt_mv > 0) {
    snprintf(tmp, sizeof(tmp), "%u.%02uV", batt_mv / 1000, (batt_mv % 1000) / 10);
  } else {
    snprintf(tmp, sizeof(tmp), "-.--V");
  }
  _display->print(tmp);
}

void UITask::renderTopStats(int y) {
  char tmp[24];
  _display->setColor(DisplayDriver::LIGHT);
  if (_mesh) {
    _display->setCursor(_display->width() - 174, y);
    snprintf(tmp, sizeof(tmp), "S: %.1f", _mesh->getObserverLastSnr());
    _display->print(tmp);

    _display->setCursor(_display->width() - 120, y);
    snprintf(tmp, sizeof(tmp), "NF: %d", _mesh->getObserverNoiseFloor());
    _display->print(tmp);
  }
#ifdef ESP32
  uint32_t free_kb = ESP.getFreeHeap() / 1024;
  _display->setCursor(_display->width() - 66, y);
  snprintf(tmp, sizeof(tmp), "%luk", (unsigned long)free_kb);
  _display->print(tmp);
#endif
  if (_mesh) {
    renderBattery(_mesh->getObserverBattMilliVolts(), _display->width() - 38, y);
  }
}

void UITask::updateRxActivityBins() {
  if (!_mesh) {
    return;
  }

  bool changed = false;
  unsigned long now = millis();
  while ((long)(now - _next_activity_rollover) >= 0) {
    _activity_bin_index = (_activity_bin_index + 1) % RX_ACTIVITY_BINS;
    _activity_bins[_activity_bin_index] = 0;
    _next_activity_rollover += RX_ACTIVITY_BIN_MILLIS;
    changed = true;
  }

  uint32_t rx_total = _mesh->getObserverRxPackets();
  if (rx_total != _activity_prev_rx_total) {
    uint32_t delta = rx_total - _activity_prev_rx_total;
    uint32_t value = (uint32_t)_activity_bins[_activity_bin_index] + delta;

    _activity_bins[_activity_bin_index] = value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
    _activity_prev_rx_total = rx_total;
    changed = true;
  }

  if (changed && (_screen == 1 || _screen == 2)) {
    _next_refresh = 0;
  }
}

void UITask::renderRxActivityChart() {
  int chart_x = _display->width() - PATH_CHART_WIDTH;
  int chart_w = PATH_CHART_WIDTH - 2;
  uint16_t max_count = 1;

  for (uint8_t i = 0; i < RX_ACTIVITY_BINS; i++) {
    if (_activity_bins[i] > max_count) {
      max_count = _activity_bins[i];
    }
  }

  _display->setColor(DisplayDriver::LIGHT);
  for (uint8_t i = 0; i < RX_ACTIVITY_BINS; i++) {
    uint8_t idx = (_activity_bin_index + RX_ACTIVITY_BINS - i) % RX_ACTIVITY_BINS;
    uint16_t value = _activity_bins[idx];
    int y = PATH_CHART_TOP + i * PATH_CHART_ROW_H;
    int bar_w = value == 0 ? 0 : (int)((uint32_t)value * chart_w / max_count);

    if (bar_w > 0) {
      _display->fillRect(chart_x, y, bar_w, PATH_CHART_ROW_H - 2);
    }
  }

  char label[8];
  if (max_count > 999) {
    snprintf(label, sizeof(label), "0-999+");
  } else {
    snprintf(label, sizeof(label), "0-%u", (unsigned int)max_count);
  }
  _display->setCursor(chart_x, PATH_CHART_TOP + RX_ACTIVITY_BINS * PATH_CHART_ROW_H);
  _display->print(label);
}

void UITask::syncObserverTotalsAfterReset() {
  uint32_t rx_total = _mesh ? _mesh->getObserverRxPackets() : 0;
  uint32_t mqtt_total = _mesh ? _mesh->getObserverMqttPublished() : 0;
  _prev_rx_total = rx_total;
  _prev_mqtt_total = mqtt_total;
  _render_rx_total = rx_total;
  _activity_prev_rx_total = rx_total;
  _last_min_rx = 0;
  _last_min_mqtt = 0;
  _next_stats_rollover = millis() + STATS_WINDOW_MILLIS;
}

void UITask::renderCurrScreen() {
  char tmp[80];
  if (!_dump_mode && millis() < BOOT_SCREEN_MILLIS) { // boot screen
    // meshcore logo
    _display->setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    _display->drawXbm((_display->width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // version info
    _display->setColor(DisplayDriver::LIGHT);
    _display->setTextSize(1);
    uint16_t versionWidth = _display->getTextWidth(_version_info);
    _display->setCursor((_display->width() - versionWidth) / 2, 22);
    _display->print(_version_info);

    // node type
    const char* node_type = "< Repeater >";
    uint16_t typeWidth = _display->getTextWidth(node_type);
    _display->setCursor((_display->width() - typeWidth) / 2, 35);
    _display->print(node_type);
  } else if (_screen == 0) {  // home screen
    // node name
    _display->setCursor(0, 0);
    _display->setTextSize(1);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_node_prefs->node_name);
    renderTopStats(12);

    // freq / sf
    _display->setCursor(0, 28);
    _display->setColor(DisplayDriver::YELLOW);
    sprintf(tmp, "FREQ: %06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->print(tmp);

    // bw / cr
    _display->setCursor(0, 40);
    sprintf(tmp, "BW: %03.2f CR: %d", _node_prefs->bw, _node_prefs->cr);
    _display->print(tmp);

    _display->setCursor(0, 53);
    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      const char* host = _mesh->getObserverMqttHost();
      const char* status = _mesh->getObserverMqttStatus();
      snprintf(tmp, sizeof(tmp), "MQTT:%s %s", status, host[0] ? host : "-");
      _display->print(tmp);

      uint32_t rx_total = _mesh->getObserverRxPackets();
      uint32_t mqtt_total = _mesh->getObserverMqttPublished();
      _display->setCursor(0, 66);
      sprintf(tmp, "1m RX:%lu MQTT:%lu",
              (unsigned long)(rx_total - _prev_rx_total),
              (unsigned long)(mqtt_total - _prev_mqtt_total));
      _display->print(tmp);

      _display->setCursor(0, 79);
      sprintf(tmp, "Tot RX:%lu MQTT:%lu",
              (unsigned long)rx_total,
              (unsigned long)mqtt_total);
      _display->print(tmp);
    } else {
      sprintf(tmp, "1m RX:%lu MQTT:%lu", (unsigned long)_last_min_rx, (unsigned long)_last_min_mqtt);
      _display->print(tmp);
    }

    if (_status[0] && millis() < _status_until) {
      _display->setCursor(0, 92);
      _display->setColor(DisplayDriver::LIGHT);
      _display->print(_status);
    } else if (_mesh) {
      _mesh->getObserverDiagLine(tmp, sizeof(tmp));
      _display->drawTextEllipsized(0, 92, _display->width(), tmp);
    }
  } else if (_screen == 1) {  // path screen
    int chart_x = _display->width() - PATH_CHART_WIDTH;
    int path_width = chart_x - 3;

    _display->setTextSize(1);
    _display->setCursor(0, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Paths");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    renderRxActivityChart();
    if (_mesh) {
      _display->drawTextEllipsized(0, 14, path_width, "Cnt   Age  Path");

      bool any = false;
      for (uint8_t i = 0; i < 6; i++) {
        if (_mesh->getObserverPathLine(i, tmp, sizeof(tmp))) {
          _display->drawTextEllipsized(0, 26 + i * 11, path_width, tmp);
          any = true;
        }
      }
      if (!any) {
        _display->drawTextEllipsized(0, 30, path_width, "No RX paths");
      }
      if (_mesh->getObserverLatestPathLine(tmp, sizeof(tmp))) {
        _display->drawTextEllipsized(0, 92, path_width, "Last path");
        _display->drawTextEllipsized(0, 104, path_width, tmp);
      }
    }
  } else if (_screen == 2) {  // heard screen
    int chart_x = _display->width() - PATH_CHART_WIDTH;
    int table_width = chart_x - 3;

    _display->setTextSize(1);
    _display->setCursor(0, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Heards");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    renderRxActivityChart();
    if (_mesh) {
      if (_status[0] && millis() < _status_until) {
        _display->drawTextEllipsized(0, 16, table_width, _status);
      } else {
        _display->drawTextEllipsized(0, 16, table_width, "Hop    Age   Max   Last");
      }

      bool any = false;
      for (uint8_t i = 0; i < 8; i++) {
        if (_mesh->getObserverLastHopLine(i, tmp, sizeof(tmp))) {
          _display->drawTextEllipsized(0, 28 + i * 11, table_width, tmp);
          any = true;
        }
      }
      if (!any) {
        _display->drawTextEllipsized(0, 30, table_width, "No RX hops");
      }
    }
  } else if (_screen == 3) {  // savepoints screen
    _display->setTextSize(1);
    _display->setCursor(0, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Savepoints");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      _mesh->getObserverClockSyncStatus(tmp, sizeof(tmp));
      _display->drawTextEllipsized(0, 12, _display->width(), tmp);

      if (_status[0] && millis() < _status_until) {
        _display->drawTextEllipsized(0, 24, _display->width(), _status);
      } else {
        _display->setCursor(0, 24);
        _display->print("ID   Time  RX    NF");
      }

      bool any = false;
      for (uint8_t i = 0; i < 6; i++) {
        if (_mesh->getObserverSavepointLine(i, tmp, sizeof(tmp))) {
          _display->drawTextEllipsized(0, 36 + i * 11, _display->width(), tmp);
          any = true;
        }
      }
      if (!any) {
        _display->setCursor(0, 38);
        _display->print("No savepoints");
      }
    }
  } else {  // mqtt screen
    _display->setTextSize(1);
    _display->setCursor(0, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("MQTT");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      _display->setCursor(0, 16);
      snprintf(tmp, sizeof(tmp), "WiFi/MQTT:%s", _mesh->isObserverMqttEnabled() ? "on" : "off");
      _display->print(tmp);

      _display->setCursor(0, 28);
      snprintf(tmp, sizeof(tmp), "State:%s code:%d", _mesh->getObserverMqttStatus(), _mesh->getObserverMqttState());
      _display->print(tmp);

      _display->setCursor(0, 40);
      snprintf(tmp, sizeof(tmp), "Err:%s", _mesh->getObserverMqttLastError());
      _display->print(tmp);

      _display->setCursor(0, 52);
      snprintf(tmp, sizeof(tmp), "WiFiF:%lu ConnF:%lu",
               (unsigned long)_mesh->getObserverMqttWifiFailures(),
               (unsigned long)_mesh->getObserverMqttConnectFailures());
      _display->print(tmp);

      _display->setCursor(0, 64);
      snprintf(tmp, sizeof(tmp), "PubF:%lu",
               (unsigned long)_mesh->getObserverMqttPublishFailures());
      _display->print(tmp);
    }

    if (_status[0] && millis() < _status_until) {
      _display->setCursor(0, 82);
      _display->setColor(DisplayDriver::LIGHT);
      _display->print(_status);
    }
  }
}

#ifdef ENABLE_DISPLAY_DUMP
const char* UITask::screenName(uint8_t screen) {
  switch (screen) {
    case 0: return "status";
    case 1: return "paths";
    case 2: return "heards";
    case 3: return "savepoints";
    case 4: return "mqtt";
    default: return "unknown";
  }
}

bool UITask::renderScreenForDump(uint8_t screen) {
  if (screen >= screenCount()) return false;

  uint8_t prev_screen = _screen;
  bool prev_dump_mode = _dump_mode;
  _screen = screen;
  _dump_mode = true;
  _display->startFrame();
  renderCurrScreen();
  _dump_mode = prev_dump_mode;
  _screen = prev_screen;
  return true;
}
#endif

void UITask::loop() {
#ifdef PIN_USER_BTN
  if (millis() >= _next_read) {
    int ev = user_btn.check();
    if (ev != BUTTON_EVENT_NONE) {
      _display->turnOn();
      _auto_off = millis() + AUTO_OFF_MILLIS;

      if (_mesh) {
        if (ev == BUTTON_EVENT_CLICK) {
          _screen = (_screen + 1) % 5;
          _status[0] = 0;
        } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
          if (_screen == 1 || _screen == 2) {
            _screen = _screen == 1 ? 2 : 1;
            _status[0] = 0;
          } else if (_screen == 3) {
            _mesh->createObserverSavepoint(_activity_bins, RX_ACTIVITY_BINS, _activity_bin_index, _status, sizeof(_status));
          } else {
            _mesh->sendSelfAdvertisement(0, false);
            strcpy(_status, "Advert sent");
          }
        } else if (ev == BUTTON_EVENT_LONG_PRESS) {
          if (_screen == 4) {
            bool enabled = _mesh->toggleObserverMqttEnabled();
            strcpy(_status, enabled ? "WiFi/MQTT on" : "WiFi/MQTT off");
          } else if (_screen == 3) {
            _mesh->resetObserverLiveStats();
            syncObserverTotalsAfterReset();
            strcpy(_status, "Live counters reset");
          } else if (_screen == 2) {
            _mesh->sendNodeDiscoverReq();
            strcpy(_status, "Discover sent");
          } else if (_screen == 1) {
            _next_refresh = 0;
            _display->startFrame();
            _display->setTextSize(1);
            _display->setColor(DisplayDriver::GREEN);
            _display->setCursor(0, 0);
            _display->print(_node_prefs->node_name);
            _display->setColor(DisplayDriver::LIGHT);
            _display->setCursor(0, 28);
            _display->print("Hibernate");
            _display->setCursor(0, 42);
            _display->print("Node is off");
            _display->setCursor(0, 68);
            _display->print("Press button to wake");
            _display->endFrame();
            while (user_btn.isPressed()) {
              delay(20);
            }
            delay(250);
            _mesh->hibernate();
          } else if (_screen == 0) {
            _mesh->sendSelfAdvertisement(0, true);
            strcpy(_status, "Flood advert sent");
          }
        }
        _status_until = millis() + 4000;
        _next_refresh = 0;
      }
    }
    _next_read = millis() + 50;
  }
#endif

  updateRxActivityBins();

  if (_mesh && millis() >= _next_stats_rollover) {
    uint32_t rx_total = _mesh->getObserverRxPackets();
    uint32_t mqtt_total = _mesh->getObserverMqttPublished();
    _last_min_rx = rx_total - _prev_rx_total;
    _last_min_mqtt = mqtt_total - _prev_mqtt_total;
    _prev_rx_total = rx_total;
    _prev_mqtt_total = mqtt_total;
    _next_stats_rollover = millis() + STATS_WINDOW_MILLIS;
    _next_refresh = 0;
  }

  if (_mesh && (_screen == 1 || _screen == 2)) {
    uint32_t rx_total = _mesh->getObserverRxPackets();
    if (rx_total != _render_rx_total) {
      _render_rx_total = rx_total;
      _next_refresh = 0;
    }
  }

  if (_display->isOn()) {
    if (millis() >= _next_refresh) {
      _display->startFrame();
      renderCurrScreen();
      _display->endFrame();

      _next_refresh = millis() + ((_screen == 1 || _screen == 2) ? 60000 : 1000);
      if (_screen == 1 || _screen == 2) {
        _auto_off = _next_refresh + 5000;
      }
    }
    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }
}

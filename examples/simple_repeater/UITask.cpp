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
#define UI_LEFT_MARGIN       6

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
#if defined(FIELD_MONITOR_LITE) && defined(BUTTON_PIN2)
  user_btn2.begin();
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

static void drawPathRow(DisplayDriver* display, int x, int y, int width, const char* line) {
  if (!display || !line || width <= 0) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "%s", line);

  char* count = buf;
  while (*count == ' ') count++;
  char* path = strchr(count, ' ');
  if (!path) {
    display->drawTextEllipsized(x, y, width, line);
    return;
  }
  *path++ = 0;
  while (*path == ' ') path++;

  char* age = strrchr(path, ' ');
  if (!age) {
    display->drawTextEllipsized(x, y, width, line);
    return;
  }
  *age++ = 0;
  while (*age == ' ') age++;

  int age_width = display->getTextWidth(age);
  int age_x = x + width - age_width;
  int count_width = display->getTextWidth("999");
  int count_x = x + count_width - display->getTextWidth(count);
  int path_x = x + count_width + 6;
  int path_width = age_x - path_x - 4;

  display->setCursor(count_x, y);
  display->print(count);
  if (path_width > 8) display->drawTextEllipsized(path_x, y, path_width, path);
  display->setCursor(age_x, y);
  display->print(age);
}

static void drawRightMetricRow(DisplayDriver* display, int x, int y, int width, const char* line) {
  if (!display || !line || width <= 0) return;

  char buf[64];
  snprintf(buf, sizeof(buf), "%s", line);
  char* metric = strrchr(buf, ' ');
  if (!metric) {
    display->drawTextEllipsized(x, y, width, line);
    return;
  }
  *metric++ = 0;
  while (*metric == ' ') metric++;

  size_t len = strlen(buf);
  while (len > 0 && buf[len - 1] == ' ') {
    buf[--len] = 0;
  }

  int metric_width = display->getTextWidth(metric);
  int metric_x = x + width - metric_width;
  int left_width = metric_x - x - 4;
  if (left_width > 8) display->drawTextEllipsized(x, y, left_width, buf);
  display->setCursor(metric_x, y);
  display->print(metric);
}

static bool uiTokenLooksLikeAge(const char* token) {
  if (!token || !token[0]) return false;
  size_t len = strlen(token);
  if (strcmp(token, ">999") == 0) return true;
  return len >= 2 && (token[len - 1] == 's' || token[len - 1] == 'm');
}

static bool uiPathContainsRep(const char* path_text, const char* rep) {
  if (!path_text || !rep || rep[0] == 0) return false;
  size_t rep_len = strlen(rep);
  const char* p = path_text;
  while (*p) {
    while (*p == ' ') p++;
    const char* start = p;
    while (*p && *p != ' ') p++;
    if ((size_t)(p - start) == rep_len && strncmp(start, rep, rep_len) == 0) return true;
  }
  return false;
}

static bool uiNextToken(char*& cursor, char* dest, size_t dest_size) {
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

static void drawHeatCell(DisplayDriver* display, int x, int y, int w, uint8_t position) {
  if (!display) return;

  switch (position) {
    case 1:  // last / nearest rep: darkest
      display->fillRect(x, y + 2, w, 6);
      break;
    case 2:  // previous rep: medium density
      display->fillRect(x, y + 2, w, 2);
      display->fillRect(x, y + 6, w, 2);
      break;
    case 3:  // pre-previous rep: light density
      display->fillRect(x, y + 4, w, 2);
      break;
    default:
      display->fillRect(x + 3, y + 4, 2, 2);
      break;
  }
}

void UITask::updatePathHeatSnapshot() {
  memset(_heat_rows, 0, sizeof(_heat_rows));
  _heat_row_count = 0;
  _heat_valid = true;
  if (!_mesh) return;

  char line[80];
  char path_text[HEAT_PATHS][32];
  uint8_t path_count = 0;
  memset(path_text, 0, sizeof(path_text));

  for (uint8_t i = 0; i < HEAT_PATHS; i++) {
    if (!_mesh->getObserverPathLine(i, line, sizeof(line))) continue;

    char work[80];
    snprintf(work, sizeof(work), "%s", line);
    char* cursor = work;
    char token[12];
    if (!uiNextToken(cursor, token, sizeof(token))) continue;  // count
    char path[32] = "";
    while (uiNextToken(cursor, token, sizeof(token))) {
      if (uiTokenLooksLikeAge(token)) break;
      if (strcmp(token, "-") != 0) {
        if (path[0]) strncat(path, " ", sizeof(path) - strlen(path) - 1);
        strncat(path, token, sizeof(path) - strlen(path) - 1);
      }
    }
    if (!path[0]) continue;
    snprintf(path_text[path_count], sizeof(path_text[path_count]), "%s", path);

    char path_copy[32];
    snprintf(path_copy, sizeof(path_copy), "%s", path);
    char* path_cursor = path_copy;
    char rep[8];
    uint8_t position = 1;
    while (uiNextToken(path_cursor, rep, sizeof(rep))) {
      int row = -1;
      for (uint8_t r = 0; r < _heat_row_count; r++) {
        if (strcmp(_heat_rows[r].rep, rep) == 0) {
          row = r;
          break;
        }
      }
      if (row < 0 && _heat_row_count < HEAT_ROWS) {
        row = _heat_row_count++;
        snprintf(_heat_rows[row].rep, sizeof(_heat_rows[row].rep), "%s", rep);
      }
      if (row >= 0) {
        _heat_rows[row].mask |= (uint16_t)(1U << path_count);
        _heat_rows[row].position[path_count] = position;
      }
      if (position < 3) position++;
    }

    path_count++;
  }

  for (uint8_t r = 0; r < _heat_row_count; r++) {
    uint8_t pc = 0;
    for (uint8_t p = 0; p < path_count; p++) {
      if (uiPathContainsRep(path_text[p], _heat_rows[r].rep)) pc++;
    }
    _heat_rows[r].pc = pc;
  }

  for (uint8_t i = 0; i < _heat_row_count; i++) {
    for (uint8_t j = i + 1; j < _heat_row_count; j++) {
      if (_heat_rows[j].pc > _heat_rows[i].pc) {
        HeatRow tmp = _heat_rows[i];
        _heat_rows[i] = _heat_rows[j];
        _heat_rows[j] = tmp;
      }
    }
  }
}

void UITask::renderPathHeatScreen() {
  _display->setTextSize(1);
  _display->setCursor(UI_LEFT_MARGIN, 0);
  _display->setColor(DisplayDriver::GREEN);
  _display->print("Heatstrip");
  renderTopStats();

  _display->setColor(DisplayDriver::LIGHT);
  if (!_heat_valid) updatePathHeatSnapshot();

  const int pc_right = _display->width() - UI_LEFT_MARGIN;
  int pc_header_width = _display->getTextWidth("PC");
  _display->drawTextEllipsized(UI_LEFT_MARGIN, 16, _display->width() - UI_LEFT_MARGIN, "   Rep  Paths");
  _display->setCursor(pc_right - pc_header_width, 16);
  _display->print("PC");
  if (_heat_row_count == 0) {
    _display->drawTextEllipsized(UI_LEFT_MARGIN, 34, _display->width() - UI_LEFT_MARGIN, "Double: refresh");
    return;
  }

  const int rep_x = UI_LEFT_MARGIN;
  const int block_x = UI_LEFT_MARGIN + 44;
  const int block_w = 8;
  const int block_gap = 3;
  for (uint8_t r = 0; r < _heat_row_count; r++) {
    int y = 28 + r * 11;
    int rep_width = _display->getTextWidth(_heat_rows[r].rep);
    _display->setCursor(rep_x + 36 - rep_width, y);
    _display->print(_heat_rows[r].rep);
    for (uint8_t p = 0; p < HEAT_PATHS; p++) {
      int x = block_x + p * (block_w + block_gap);
      drawHeatCell(_display, x, y, block_w, _heat_rows[r].position[p]);
    }
    char pc[4];
    snprintf(pc, sizeof(pc), "%u", (unsigned int)_heat_rows[r].pc);
    int pc_width = _display->getTextWidth(pc);
    _display->setCursor(pc_right - pc_width, y);
    _display->print(pc);
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

  if (changed && (
#ifdef FIELD_MONITOR_LITE
      _screen == 1 || _screen == 2 || _screen == 3
#else
      _screen == 1 || _screen == 2
#endif
      )) {
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
  _display->fillRect(chart_x, PATH_CHART_TOP, 1, RX_ACTIVITY_BINS * PATH_CHART_ROW_H - 2);

  char label[8];
  if (max_count > 999) {
    snprintf(label, sizeof(label), "0-999+");
  } else {
    snprintf(label, sizeof(label), "0-%u", (unsigned int)max_count);
  }
  _display->setCursor(chart_x, PATH_CHART_TOP + RX_ACTIVITY_BINS * PATH_CHART_ROW_H);
  _display->print(label);
}

void UITask::renderRxActivityHistogram() {
  const int left = UI_LEFT_MARGIN;
  const int top = 18;
  const int row_h = 8;
  const int bar_w_max = _display->width() - left - 16;
  uint16_t max_count = 1;

  for (uint8_t i = 0; i < RX_ACTIVITY_BINS; i++) {
    if (_activity_bins[i] > max_count) max_count = _activity_bins[i];
  }

  _display->setColor(DisplayDriver::LIGHT);
  for (uint8_t i = 0; i < RX_ACTIVITY_BINS; i++) {
    uint8_t idx = (_activity_bin_index + RX_ACTIVITY_BINS - i) % RX_ACTIVITY_BINS;
    uint16_t value = _activity_bins[idx];
    int y = top + i * row_h;
    int bar_w = value == 0 ? 0 : (int)((uint32_t)value * bar_w_max / max_count);
    if (bar_w > 0) _display->fillRect(left, y, bar_w, row_h - 2);
  }
  _display->fillRect(left, top, 1, RX_ACTIVITY_BINS * row_h - 2);

  char label[24];
  snprintf(label, sizeof(label), "range 0-%u / min", (unsigned int)max_count);
  _display->drawTextEllipsized(left, top + RX_ACTIVITY_BINS * row_h + 2, _display->width() - left, label);
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
  if (_mesh) _mesh->setObserverHealthScreen(_screen);
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
  }
#ifdef FIELD_MONITOR_LITE
  else if (_screen == 0) {  // status screen
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setTextSize(1);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_node_prefs->node_name);

    _display->setColor(DisplayDriver::LIGHT);
    _display->setCursor(UI_LEFT_MARGIN, 14);
    sprintf(tmp, "F:%06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->print(tmp);
    _display->setCursor(UI_LEFT_MARGIN, 25);
    snprintf(tmp, sizeof(tmp), "BW:%03.2f CR:%d NF:%d", _node_prefs->bw, _node_prefs->cr,
             _mesh ? _mesh->getObserverNoiseFloor() : 0);
    _display->print(tmp);
    if (_mesh) {
      uint32_t rx_total = _mesh->getObserverRxPackets();
      _display->setCursor(UI_LEFT_MARGIN, 39);
      snprintf(tmp, sizeof(tmp), "RX:%lu 1m:%lu",
               (unsigned long)rx_total,
               (unsigned long)(rx_total - _prev_rx_total));
      _display->print(tmp);
      _display->setCursor(UI_LEFT_MARGIN, 51);
      snprintf(tmp, sizeof(tmp), "SNR: %.1f", _mesh->getObserverLastSnr());
      _display->print(tmp);
      _mesh->getObserverDiagLine(tmp, sizeof(tmp));
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 64, _display->width() - UI_LEFT_MARGIN, tmp);
      renderBattery(_mesh->getObserverBattMilliVolts(), UI_LEFT_MARGIN, 78);
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 92, _display->width() - UI_LEFT_MARGIN, "Last path");
      if (_mesh->getObserverLatestPathLine(tmp, sizeof(tmp))) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 104, _display->width() - UI_LEFT_MARGIN, tmp);
      } else {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 104, _display->width() - UI_LEFT_MARGIN, "-");
      }
    }
  } else if (_screen == 1) {  // paths screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Paths");
    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 16, _display->width() - UI_LEFT_MARGIN, "Cnt Path Age");
      bool any = false;
      for (uint8_t i = 0; i < 8; i++) {
        if (_mesh->getObserverPathLine(i, tmp, sizeof(tmp))) {
          _display->drawTextEllipsized(UI_LEFT_MARGIN, 28 + i * 11, _display->width() - UI_LEFT_MARGIN, tmp);
          any = true;
        }
      }
      if (!any) _display->drawTextEllipsized(UI_LEFT_MARGIN, 30, _display->width() - UI_LEFT_MARGIN, "No RX paths");
    }
  } else if (_screen == 2) {  // heards screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Heards");
    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      drawRightMetricRow(_display, UI_LEFT_MARGIN, 16, _display->width() - UI_LEFT_MARGIN, "   Rep  Age    Max   Last PC");
      bool any = false;
      for (uint8_t i = 0; i < 8; i++) {
        if (_mesh->getObserverLastHopLine(i, tmp, sizeof(tmp))) {
          drawRightMetricRow(_display, UI_LEFT_MARGIN, 28 + i * 11, _display->width() - UI_LEFT_MARGIN, tmp);
          any = true;
        }
      }
      if (!any) _display->drawTextEllipsized(UI_LEFT_MARGIN, 30, _display->width() - UI_LEFT_MARGIN, "No RX hops");
    }
  } else if (_screen == 3) {  // histogram screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Histogram");
    renderRxActivityHistogram();
  } else {  // advert screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Advert");
    _display->setColor(DisplayDriver::LIGHT);
    _display->drawTextEllipsized(UI_LEFT_MARGIN, 24, _display->width() - UI_LEFT_MARGIN, "Double: advert");
    _display->drawTextEllipsized(UI_LEFT_MARGIN, 38, _display->width() - UI_LEFT_MARGIN, "Long: flood advert");
    if (_status[0] && millis() < _status_until) {
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 64, _display->width() - UI_LEFT_MARGIN, _status);
    }
  }
#else
  else if (_screen == 0) {  // home screen
    // node name
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setTextSize(1);
    _display->setColor(DisplayDriver::GREEN);
    _display->print(_node_prefs->node_name);
    renderTopStats(12);

    // freq / sf
    _display->setCursor(UI_LEFT_MARGIN, 28);
    _display->setColor(DisplayDriver::YELLOW);
    sprintf(tmp, "FREQ: %06.3f SF%d", _node_prefs->freq, _node_prefs->sf);
    _display->print(tmp);

    // bw / cr
    _display->setCursor(UI_LEFT_MARGIN, 40);
    sprintf(tmp, "BW: %03.2f CR: %d", _node_prefs->bw, _node_prefs->cr);
    _display->print(tmp);

    _display->setCursor(UI_LEFT_MARGIN, 53);
    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      const char* host = _mesh->getObserverMqttHost();
      const char* status = _mesh->getObserverMqttStatus();
      snprintf(tmp, sizeof(tmp), "MQTT:%s %s", status, host[0] ? host : "-");
      _display->print(tmp);

      uint32_t rx_total = _mesh->getObserverRxPackets();
      uint32_t mqtt_total = _mesh->getObserverMqttPublished();
      _display->setCursor(UI_LEFT_MARGIN, 66);
      sprintf(tmp, "1m RX:%lu MQTT:%lu",
              (unsigned long)(rx_total - _prev_rx_total),
              (unsigned long)(mqtt_total - _prev_mqtt_total));
      _display->print(tmp);

      _display->setCursor(UI_LEFT_MARGIN, 79);
      sprintf(tmp, "Tot RX:%lu MQTT:%lu",
              (unsigned long)rx_total,
              (unsigned long)mqtt_total);
      _display->print(tmp);
    } else {
      sprintf(tmp, "1m RX:%lu MQTT:%lu", (unsigned long)_last_min_rx, (unsigned long)_last_min_mqtt);
      _display->print(tmp);
    }

    if (_status[0] && millis() < _status_until) {
      _display->setCursor(UI_LEFT_MARGIN, 92);
      _display->setColor(DisplayDriver::LIGHT);
      _display->print(_status);
    } else if (_mesh) {
      _mesh->getObserverDiagLine(tmp, sizeof(tmp));
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 92, _display->width() - UI_LEFT_MARGIN, tmp);
      _mesh->getObserverHealthLine(tmp, sizeof(tmp));
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 104, _display->width() - UI_LEFT_MARGIN, tmp);
    }
  } else if (_screen == 1) {  // path screen
    int chart_x = _display->width() - PATH_CHART_WIDTH;
    int path_width = chart_x - UI_LEFT_MARGIN - 3;

    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Paths");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    renderRxActivityChart();
    if (_mesh) {
      drawPathRow(_display, UI_LEFT_MARGIN, 14, path_width, "Cnt Path Age");

      bool any = false;
      for (uint8_t i = 0; i < 6; i++) {
        if (_mesh->getObserverPathLine(i, tmp, sizeof(tmp))) {
          drawPathRow(_display, UI_LEFT_MARGIN, 26 + i * 11, path_width, tmp);
          any = true;
        }
      }
      if (!any) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 30, path_width, "No RX paths");
      }
      if (_mesh->getObserverLatestPathLine(tmp, sizeof(tmp))) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 92, path_width, "Last path");
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 104, path_width, tmp);
      }
    }
  } else if (_screen == 2) {  // heard screen
    int chart_x = _display->width() - PATH_CHART_WIDTH;
    int table_width = chart_x - UI_LEFT_MARGIN - 3;

    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Heards");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    renderRxActivityChart();
    if (_mesh) {
      if (_status[0] && millis() < _status_until) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 16, table_width, _status);
      } else {
        drawRightMetricRow(_display, UI_LEFT_MARGIN, 16, table_width, "   Rep  Age    Max   Last PC");
      }

      bool any = false;
      for (uint8_t i = 0; i < 8; i++) {
        if (_mesh->getObserverLastHopLine(i, tmp, sizeof(tmp))) {
          drawRightMetricRow(_display, UI_LEFT_MARGIN, 28 + i * 11, table_width, tmp);
          any = true;
        }
      }
      if (!any) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 30, table_width, "No RX hops");
      }
    }
  } else if (_screen == 3) {  // path heat screen
    renderPathHeatScreen();
  } else if (_screen == 4) {  // savepoints screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("Savepoints");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      _mesh->getObserverClockSyncStatus(tmp, sizeof(tmp));
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 12, _display->width() - UI_LEFT_MARGIN, tmp);

      if (_status[0] && millis() < _status_until) {
        _display->drawTextEllipsized(UI_LEFT_MARGIN, 24, _display->width() - UI_LEFT_MARGIN, _status);
      } else {
        _display->setCursor(UI_LEFT_MARGIN, 24);
        _display->print("ID   Time  RX    NF");
      }

      bool any = false;
      for (uint8_t i = 0; i < 6; i++) {
        if (_mesh->getObserverSavepointLine(i, tmp, sizeof(tmp))) {
          _display->drawTextEllipsized(UI_LEFT_MARGIN, 36 + i * 11, _display->width() - UI_LEFT_MARGIN, tmp);
          any = true;
        }
      }
      if (!any) {
        _display->setCursor(UI_LEFT_MARGIN, 38);
        _display->print("No savepoints");
      }
    }
  } else {  // mqtt screen
    _display->setTextSize(1);
    _display->setCursor(UI_LEFT_MARGIN, 0);
    _display->setColor(DisplayDriver::GREEN);
    _display->print("MQTT");
    renderTopStats();

    _display->setColor(DisplayDriver::LIGHT);
    if (_mesh) {
      _display->setCursor(UI_LEFT_MARGIN, 16);
      snprintf(tmp, sizeof(tmp), "WiFi/MQTT:%s", _mesh->isObserverMqttEnabled() ? "on" : "off");
      _display->print(tmp);

      _display->setCursor(UI_LEFT_MARGIN, 28);
      snprintf(tmp, sizeof(tmp), "State:%s code:%d", _mesh->getObserverMqttStatus(), _mesh->getObserverMqttState());
      _display->print(tmp);

      _display->setCursor(UI_LEFT_MARGIN, 40);
      snprintf(tmp, sizeof(tmp), "Err:%s", _mesh->getObserverMqttLastError());
      _display->print(tmp);

      _display->setCursor(UI_LEFT_MARGIN, 52);
      snprintf(tmp, sizeof(tmp), "WiFiF:%lu ConnF:%lu",
               (unsigned long)_mesh->getObserverMqttWifiFailures(),
               (unsigned long)_mesh->getObserverMqttConnectFailures());
      _display->print(tmp);

      _display->setCursor(UI_LEFT_MARGIN, 64);
      snprintf(tmp, sizeof(tmp), "PubF:%lu",
               (unsigned long)_mesh->getObserverMqttPublishFailures());
      _display->print(tmp);

      _mesh->getObserverHealthLine(tmp, sizeof(tmp));
      _display->drawTextEllipsized(UI_LEFT_MARGIN, 76, _display->width() - UI_LEFT_MARGIN, tmp);
    }

    if (_status[0] && millis() < _status_until) {
      _display->setCursor(UI_LEFT_MARGIN, 82);
      _display->setColor(DisplayDriver::LIGHT);
      _display->print(_status);
    }
  }
#endif
}

#ifdef ENABLE_DISPLAY_DUMP
const char* UITask::screenName(uint8_t screen) {
#ifdef FIELD_MONITOR_LITE
  switch (screen) {
    case 0: return "status";
    case 1: return "paths";
    case 2: return "heards";
    case 3: return "histogram";
    case 4: return "advert";
    default: return "unknown";
  }
#else
  switch (screen) {
    case 0: return "status";
    case 1: return "paths";
    case 2: return "heards";
    case 3: return "heatstrip";
    case 4: return "savepoints";
    case 5: return "mqtt";
    default: return "unknown";
  }
#endif
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
          _screen = (_screen + 1) % screenCount();
          _status[0] = 0;
        } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
#ifdef FIELD_MONITOR_LITE
          if (_screen == 1 || _screen == 2) {
            _screen = _screen == 1 ? 2 : 1;
            _status[0] = 0;
          } else if (_screen == 4) {
            _mesh->sendSelfAdvertisement(0, false);
            strcpy(_status, "Advert sent");
          }
#else
          if (_screen == 1 || _screen == 2) {
            _screen = _screen == 1 ? 2 : 1;
            _status[0] = 0;
          } else if (_screen == 3) {
            updatePathHeatSnapshot();
            strcpy(_status, "Heat refreshed");
          } else if (_screen == 4) {
            _mesh->createObserverSavepoint(_activity_bins, RX_ACTIVITY_BINS, _activity_bin_index, _status, sizeof(_status));
          } else {
            _mesh->sendSelfAdvertisement(0, false);
            strcpy(_status, "Advert sent");
          }
#endif
        } else if (ev == BUTTON_EVENT_LONG_PRESS) {
#ifdef FIELD_MONITOR_LITE
          if (_screen == 2) {
            _mesh->sendNodeDiscoverReq();
            strcpy(_status, "Discover sent");
          } else if (_screen == 4) {
            _mesh->sendSelfAdvertisement(0, true);
            strcpy(_status, "Flood advert sent");
          }
#else
          if (_screen == 5) {
            bool enabled = _mesh->toggleObserverMqttEnabled();
            strcpy(_status, enabled ? "WiFi/MQTT on" : "WiFi/MQTT off");
          } else if (_screen == 4) {
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
            _display->setCursor(UI_LEFT_MARGIN, 0);
            _display->print(_node_prefs->node_name);
            _display->setColor(DisplayDriver::LIGHT);
            _display->setCursor(UI_LEFT_MARGIN, 28);
            _display->print("Hibernate");
            _display->setCursor(UI_LEFT_MARGIN, 42);
            _display->print("Node is off");
            _display->setCursor(UI_LEFT_MARGIN, 68);
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
#endif
        }
        _status_until = millis() + 4000;
        _next_refresh = 0;
      }
    }
#if defined(FIELD_MONITOR_LITE) && defined(BUTTON_PIN2)
    int ev2 = user_btn2.check();
    if (ev2 != BUTTON_EVENT_NONE) {
      _display->turnOn();
      _auto_off = millis() + AUTO_OFF_MILLIS;

      if (_mesh) {
        if (ev2 == BUTTON_EVENT_CLICK) {
          _screen = (_screen + screenCount() - 1) % screenCount();
          _status[0] = 0;
        } else if (ev2 == BUTTON_EVENT_DOUBLE_CLICK) {
          if (_screen == 1 || _screen == 2) {
            _screen = _screen == 1 ? 2 : 1;
          } else {
            _screen = 1;
          }
          _status[0] = 0;
        } else if (ev2 == BUTTON_EVENT_LONG_PRESS) {
          _mesh->resetObserverLiveStats();
          syncObserverTotalsAfterReset();
          strcpy(_status, "Live counters reset");
        }
        _status_until = millis() + 4000;
        _next_refresh = 0;
      }
    }
#endif
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
    if (
#ifndef FIELD_MONITOR_LITE
        _screen != 3
#else
        true
#endif
        ) {
      _next_refresh = 0;
    }
  }

  if (_mesh && (
#ifdef FIELD_MONITOR_LITE
      _screen == 1 || _screen == 2 || _screen == 3
#else
      _screen == 1 || _screen == 2
#endif
      )) {
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

      bool slow_screen =
#ifdef FIELD_MONITOR_LITE
          (_screen == 1 || _screen == 2 || _screen == 3);
#else
          (_screen == 1 || _screen == 2);
#endif
      if (
#ifndef FIELD_MONITOR_LITE
          _screen == 3
#else
          false
#endif
          ) {
        _next_refresh = millis() + 3600000UL;
      } else {
        _next_refresh = millis() + (slow_screen ? 60000 : 1000);
      }
      if (slow_screen) {
        _auto_off = _next_refresh + 5000;
      }
    }
    if (millis() > _auto_off) {
      _display->turnOff();
    }
  }
}

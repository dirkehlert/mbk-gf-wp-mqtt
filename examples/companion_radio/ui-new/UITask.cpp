#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = DISPLAY_FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(2);
    display.drawTextCentered(display.width()/2, 22, _version_info);

    display.drawTextCentered(display.width()/2, 44, "Fieldtest by");
    display.drawTextCentered(display.width()/2, 66, "Moorbock");

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 94, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};

static void formatLoadPct(char* dest, size_t dest_size, uint16_t pct_x10) {
  if (!dest || dest_size == 0) return;
  if (pct_x10 > 999) {
    snprintf(dest, dest_size, ">99%%");
  } else if (pct_x10 >= 100) {
    snprintf(dest, dest_size, "%u%%", (unsigned int)((pct_x10 + 5) / 10));
  } else {
    snprintf(dest, dest_size, "%u.%u%%", (unsigned int)(pct_x10 / 10), (unsigned int)(pct_x10 % 10));
  }
}

static bool nextPathToken(char*& cursor, char* dest, size_t dest_size) {
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

static void drawHeatCell(DisplayDriver& display, int x, int y, int w, uint8_t position) {
  switch (position) {
    case 1:
      display.fillRect(x, y + 1, w, 5);
      break;
    case 2:
      display.fillRect(x, y + 1, w, 2);
      display.fillRect(x, y + 4, w, 2);
      break;
    case 3:
      display.fillRect(x, y + 3, w, 2);
      break;
    default:
      display.fillRect(x + (w / 2), y + 3, 1, 1);
      break;
  }
}

class HomeScreen : public UIScreen {
  enum HomePage {
    FIRST,
    SEND,
    RECENT,
    RADIO,
    HEATSTRIP,
    HEARDS,
    SCOPES,
    LOAD,
    BLUETOOTH,
    ADVERT,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    SHUTDOWN,
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  AdvertPath recent[UI_RECENT_LIST_SIZE];
  ScopeInfo scopes[UI_RECENT_LIST_SIZE];

  static const uint8_t HEAT_ROWS = 6;
  static const uint8_t HEAT_PATHS = 8;
  static const uint8_t SEND_TARGETS = 48;
  static const uint8_t SEND_MESSAGES = 10; // nine editable messages plus GPS position

  enum SendMode {
    SEND_MODE_NAV,
    SEND_MODE_TARGET,
    SEND_MODE_MESSAGE
  };

  QuickSendTarget send_targets[SEND_TARGETS];
  uint8_t send_target_count = 0;
  uint8_t send_target_idx = 0;
  uint8_t send_msg_idx = 0;
  SendMode send_mode = SEND_MODE_NAV;

  uint8_t getGpsMessageIndex() const {
    return the_mesh.getQuickMessageCount();
  }

  uint8_t getSendMessageCount() const {
    uint8_t count = the_mesh.getQuickMessageCount() + 1;
    return count > SEND_MESSAGES ? SEND_MESSAGES : count;
  }

  const char* getSendMessage(uint8_t index) const {
    if (index == getGpsMessageIndex()) return "Meine Position";
    return the_mesh.getQuickMessage(index);
  }

  bool buildSendMessage(uint8_t index, char* dest, size_t dest_size) const {
    if (!dest || dest_size == 0) return false;
    dest[0] = 0;
    if (index != getGpsMessageIndex()) {
      snprintf(dest, dest_size, "%s", getSendMessage(index));
      return dest[0] != 0;
    }

#if ENV_INCLUDE_GPS == 1
    LocationProvider* gps = _sensors ? _sensors->getLocationProvider() : nullptr;
    if (!gps || !gps->isValid()) return false;
    double lat = ((double)gps->getLatitude()) / 1000000.0;
    double lon = ((double)gps->getLongitude()) / 1000000.0;
    snprintf(dest, dest_size, "Meine Position ist: %.6f, %.6f", lat, lon);
    return true;
#else
    return false;
#endif
  }

  struct HeatRow {
    char rep[7];
    uint8_t position[HEAT_PATHS];
    uint8_t pc;
  };

  void renderHeatstrip(DisplayDriver& display) {
    HeatRow rows[HEAT_ROWS];
    char paths[HEAT_PATHS][32];
    uint8_t row_count = 0;
    uint8_t path_count = 0;
    memset(rows, 0, sizeof(rows));
    memset(paths, 0, sizeof(paths));

    char line[80];
    for (uint8_t i = 0; i < HEAT_PATHS; i++) {
      if (!the_mesh.getMonitorPathLine(i, line, sizeof(line))) continue;

      char work[80];
      snprintf(work, sizeof(work), "%s", line);
      char* cursor = work;
      char token[12];
      if (!nextPathToken(cursor, token, sizeof(token))) continue;  // count

      uint8_t position = 1;
      while (nextPathToken(cursor, token, sizeof(token))) {
        if (strcmp(token, "-") == 0 || strcmp(token, "+") == 0) continue;
        if (paths[path_count][0]) strncat(paths[path_count], " ", sizeof(paths[path_count]) - strlen(paths[path_count]) - 1);
        strncat(paths[path_count], token, sizeof(paths[path_count]) - strlen(paths[path_count]) - 1);

        int row = -1;
        for (uint8_t r = 0; r < row_count; r++) {
          if (strcmp(rows[r].rep, token) == 0) {
            row = r;
            break;
          }
        }
        if (row < 0 && row_count < HEAT_ROWS) {
          row = row_count++;
          snprintf(rows[row].rep, sizeof(rows[row].rep), "%s", token);
        }
        if (row >= 0) {
          rows[row].position[path_count] = position;
        }
        if (position < 3) position++;
      }
      if (paths[path_count][0]) path_count++;
    }

    for (uint8_t r = 0; r < row_count; r++) {
      uint8_t pc = 0;
      for (uint8_t p = 0; p < path_count; p++) {
        char path_copy[32];
        snprintf(path_copy, sizeof(path_copy), "%s", paths[p]);
        char* cursor = path_copy;
        char token[12];
        while (nextPathToken(cursor, token, sizeof(token))) {
          if (strcmp(token, rows[r].rep) == 0) {
            pc++;
            break;
          }
        }
      }
      rows[r].pc = pc;
    }

    for (uint8_t i = 0; i < row_count; i++) {
      for (uint8_t j = i + 1; j < row_count; j++) {
        if (rows[j].pc > rows[i].pc) {
          HeatRow tmp = rows[i];
          rows[i] = rows[j];
          rows[j] = tmp;
        }
      }
    }

    display.setColor(DisplayDriver::GREEN);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("Heatstrip");
    display.setColor(DisplayDriver::LIGHT);
    if (row_count == 0) {
      display.drawTextEllipsized(0, 38, display.width(), "No RX paths");
      return;
    }

    const int rep_w = 30;
    const int pc_right = display.width() - 4;
    const int pc_left = pc_right - display.getTextWidth("PC");
    const int grid_left = rep_w + 1;
    const int grid_right = pc_left - 4;
    const int gap = 1;
    int pitch = (grid_right - grid_left + 1) / HEAT_PATHS;
    if (pitch < 4) pitch = 4;
    int block_w = pitch - gap;
    if (block_w < 3) block_w = 3;

    display.drawTextEllipsized(0, 31, rep_w, "Rep");
    for (uint8_t p = 0; p < HEAT_PATHS; p++) {
      int x = grid_left + p * pitch + block_w / 2;
      display.fillRect(x, 36, 1, 2);
    }
    display.drawTextRightAlign(pc_right, 31, "PC");

    for (uint8_t r = 0; r < row_count; r++) {
      int y = 44 + r * 10;
      int cell_y = y + 1;
      display.drawTextRightAlign(rep_w - 3, y, rows[r].rep);
      for (uint8_t p = 0; p < HEAT_PATHS; p++) {
        int x = grid_left + p * pitch;
        drawHeatCell(display, x, cell_y, block_w, rows[r].position[p]);
      }
      char pc[4];
      snprintf(pc, sizeof(pc), "%u", (unsigned int)rows[r].pc);
      display.drawTextRightAlign(pc_right, y, pc);
    }
  }

  void renderLoad(DisplayDriver& display) {
    uint16_t bins[MONITOR_ACTIVITY_BINS];
    uint8_t bin_count = the_mesh.getMonitorAirtime(bins, MONITOR_ACTIVITY_BINS);
    uint16_t max_pct_x10 = 1;
    uint32_t sum_ms = 0;

    for (uint8_t i = 0; i < bin_count; i++) {
      uint16_t pct_x10 = (uint16_t)((uint32_t)bins[i] * 1000UL / MONITOR_ACTIVITY_BIN_MILLIS);
      if (pct_x10 > max_pct_x10) max_pct_x10 = pct_x10;
      sum_ms += bins[i];
    }

    uint16_t now_pct_x10 = bin_count == 0 ? 0 : (uint16_t)((uint32_t)bins[bin_count - 1] * 1000UL / MONITOR_ACTIVITY_BIN_MILLIS);
    uint16_t avg_pct_x10 = bin_count == 0 ? 0 : (uint16_t)(sum_ms * 1000UL / ((uint32_t)MONITOR_ACTIVITY_BIN_MILLIS * bin_count));
    char now_pct[8];
    char max_pct[8];
    char avg_pct[8];
    formatLoadPct(now_pct, sizeof(now_pct), now_pct_x10);
    formatLoadPct(max_pct, sizeof(max_pct), max_pct_x10);
    formatLoadPct(avg_pct, sizeof(avg_pct), avg_pct_x10);

    display.setColor(DisplayDriver::GREEN);
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.print("Load");
    display.setColor(DisplayDriver::LIGHT);
    char line[48];
    snprintf(line, sizeof(line), "Now %s Max %s", now_pct, max_pct);
    display.drawTextEllipsized(0, 32, display.width(), line);
    snprintf(line, sizeof(line), "Avg %s / airtime", avg_pct);
    display.drawTextEllipsized(0, 43, display.width(), line);

    const int chart_top = 58;
    const int chart_bottom = 94;
    const int chart_h = chart_bottom - chart_top;
    const int chart_w = display.width() - 2;
    const int gap = 2;
    int bar_w = bin_count == 0 ? 5 : (chart_w - (bin_count - 1) * gap) / bin_count;
    if (bar_w < 3) bar_w = 3;

    for (uint8_t i = 0; i < bin_count; i++) {
      uint16_t pct_x10 = (uint16_t)((uint32_t)bins[i] * 1000UL / MONITOR_ACTIVITY_BIN_MILLIS);
      int bar_h = pct_x10 == 0 ? 0 : (int)((uint32_t)pct_x10 * chart_h / max_pct_x10);
      int x = i * (bar_w + gap);
      if (bar_h > 0) display.fillRect(x, chart_bottom - bar_h, bar_w, bar_h);
    }
    display.fillRect(0, chart_bottom, chart_w, 1);
    display.fillRect(0, chart_top, 1, chart_h + 1);
  }

  void refreshSendTargets() {
    send_target_count = the_mesh.getQuickSendTargets(send_targets, SEND_TARGETS);
    if (send_target_count == 0) {
      send_target_idx = 0;
    } else if (send_target_idx >= send_target_count) {
      send_target_idx = send_target_count - 1;
    }
  }

  void renderSend(DisplayDriver& display) {
    refreshSendTargets();

    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    display.setCursor(0, 20);
    display.print("Send");

    char line[64];
    display.setColor(send_mode == SEND_MODE_TARGET ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
    display.drawTextEllipsized(0, 33, display.width(), "To");
    if (send_target_count == 0) {
      snprintf(line, sizeof(line), "-");
    } else {
      const QuickSendTarget& target = send_targets[send_target_idx];
      char filtered_name[sizeof(target.name)];
      display.translateUTF8ToBlocks(filtered_name, target.name, sizeof(filtered_name));
      snprintf(line, sizeof(line), "%s %s",
               target.type == QUICK_SEND_CHANNEL ? "#" : "@",
               filtered_name);
    }
    display.drawTextEllipsized(18, 33, display.width() - 18, line);

    display.setColor(send_mode == SEND_MODE_MESSAGE ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
    uint8_t send_msg_count = getSendMessageCount();
    if (send_msg_idx >= send_msg_count) send_msg_idx = send_msg_count - 1;
    snprintf(line, sizeof(line), "Msg %u/%u", (unsigned int)send_msg_idx + 1, (unsigned int)send_msg_count);
    display.drawTextEllipsized(0, 52, display.width(), line);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextEllipsized(0, 66, display.width(), getSendMessage(send_msg_idx));

    display.setColor(DisplayDriver::GREEN);
    const char* action = "hold: target";
    if (send_mode == SEND_MODE_TARGET) action = "hold: msg";
    else if (send_mode == SEND_MODE_MESSAGE) action = "hold: send";
    display.drawTextCentered(display.width() / 2, 86, action);
  }


  void renderBatteryIndicator(DisplayDriver& display, uint16_t batteryMilliVolts) {
    // Convert millivolts to percentage
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif
    const int minMilliVolts = BATT_MIN_MILLIVOLTS;
    const int maxMilliVolts = BATT_MAX_MILLIVOLTS;
    int batteryPercentage = ((batteryMilliVolts - minMilliVolts) * 100) / (maxMilliVolts - minMilliVolts);
    if (batteryPercentage < 0) batteryPercentage = 0; // Clamp to 0%
    if (batteryPercentage > 100) batteryPercentage = 100; // Clamp to 100%

    // battery icon
    int iconWidth = 24;
    int iconHeight = 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), 3, iconHeight / 2);

    // fill the battery based on the percentage
    int fillWidth = (batteryPercentage * (iconWidth - 4)) / 100;
    display.fillRect(iconX + 2, iconY + 2, fillWidth, iconHeight - 4);

    // show muted icon if buzzer is muted
#ifdef PIN_BUZZER
    if (_task->isBuzzerQuiet()) {
      display.setColor(DisplayDriver::RED);
      display.drawXbm(iconX - 9, iconY + 1, muted_icon, 8, 8);
    }
#endif
  }

  CayenneLPP sensors_lpp;
  int sensors_nb = 0;
  bool sensors_scroll = false;
  int sensors_scroll_offset = 0;
  int next_sensors_refresh = 0;
  
  void refresh_sensors() {
    if (millis() > next_sensors_refresh) {
      sensors_lpp.reset();
      sensors_nb = 0;
      sensors_lpp.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);
      sensors.querySensors(0xFF, sensors_lpp);
      LPPReader reader (sensors_lpp.getBuffer(), sensors_lpp.getSize());
      uint8_t channel, type;
      while(reader.readHeader(channel, type)) {
        reader.skipData(type);
        sensors_nb ++;
      }
      sensors_scroll = sensors_nb > UI_RECENT_LIST_SIZE;
#if AUTO_OFF_MILLIS > 0
      next_sensors_refresh = millis() + 5000; // refresh sensor values every 5 sec
#else
      next_sensors_refresh = millis() + 60000; // refresh sensor values every 1 min
#endif
    }
  }

public:
  HomeScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors, NodePrefs* node_prefs)
     : _task(task), _rtc(rtc), _sensors(sensors), _node_prefs(node_prefs), _page(0), 
       _shutdown_init(false), sensors_lpp(200) {  }

  void poll() override {
    if (_shutdown_init && !_task->isButtonPressed()) {  // must wait for USR button to be released
      _task->shutdown();
    }
  }

  int render(DisplayDriver& display) override {
    char tmp[80];
    // node name
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.setCursor(0, 0);
    display.print(filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int y = 14;
    int x = display.width() / 2 - 5 * (HomePage::Count-1);
    for (uint8_t i = 0; i < HomePage::Count; i++, x += 10) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(2);
      sprintf(tmp, "MSG: %d", _task->getMsgCount());
      display.drawTextCentered(display.width() / 2, 20, tmp);

      #ifdef WIFI_SSID
        IPAddress ip = WiFi.localIP();
        snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 54, tmp); 
      #endif
      if (_task->hasConnection()) {
        display.setColor(DisplayDriver::GREEN);
        display.setTextSize(1);
        display.drawTextCentered(display.width() / 2, 43, "< Connected >");

      } else if (the_mesh.getBLEPin() != 0) { // BT pin
        display.setColor(DisplayDriver::RED);
        display.setTextSize(2);
        sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
        display.drawTextCentered(display.width() / 2, 43, tmp);
      }
    } else if (_page == HomePage::SEND) {
      renderSend(display);
    } else if (_page == HomePage::RECENT) {
      the_mesh.getRecentlyHeard(recent, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      int y = 20;
      for (int i = 0; i < UI_RECENT_LIST_SIZE; i++, y += 11) {
        auto a = &recent[i];
        if (a->name[0] == 0) continue;  // empty slot
        int secs = _rtc->getCurrentTime() - a->recv_timestamp;
        if (secs < 60) {
          sprintf(tmp, "%ds", secs);
        } else if (secs < 60*60) {
          sprintf(tmp, "%dm", secs / 60);
        } else {
          sprintf(tmp, "%dh", secs / (60*60));
        }
        
        int timestamp_width = display.getTextWidth(tmp);
        int max_name_width = display.width() - timestamp_width - 1;
        
        char filtered_recent_name[sizeof(a->name)];
        display.translateUTF8ToBlocks(filtered_recent_name, a->name, sizeof(filtered_recent_name));
        display.drawTextEllipsized(0, y, max_name_width, filtered_recent_name);
        display.setCursor(display.width() - timestamp_width - 1, y);
        display.print(tmp);
      }
    } else if (_page == HomePage::RADIO) {
      display.setColor(DisplayDriver::YELLOW);
      display.setTextSize(1);
      // freq / sf
      display.setCursor(0, 20);
      sprintf(tmp, "FQ: %06.3f   SF: %d", _node_prefs->freq, _node_prefs->sf);
      display.print(tmp);

      display.setCursor(0, 31);
      sprintf(tmp, "BW: %03.2f     CR: %d", _node_prefs->bw, _node_prefs->cr);
      display.print(tmp);

      // tx power,  noise floor
      display.setCursor(0, 42);
      sprintf(tmp, "TX: %ddBm", _node_prefs->tx_power_dbm);
      display.print(tmp);
      display.setCursor(0, 53);
      sprintf(tmp, "Noise floor: %d", radio_driver.getNoiseFloor());
      display.print(tmp);
      display.setCursor(0, 64);
      sprintf(tmp, "RX: %lu  SNR: %.1f",
              (unsigned long)the_mesh.getMonitorRxPackets(), the_mesh.getMonitorLastSnr());
      display.print(tmp);
      display.setCursor(0, 75);
      display.print("Last path:");
      if (the_mesh.getMonitorLatestPathLine(tmp, sizeof(tmp))) {
        display.drawTextEllipsized(0, 86, display.width(), tmp);
      } else {
        display.setCursor(0, 86);
        display.print("-");
      }
    } else if (_page == HomePage::HEATSTRIP) {
      renderHeatstrip(display);
    } else if (_page == HomePage::HEARDS) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      display.setCursor(0, 20);
      display.print("Heard repeaters");
      display.setColor(DisplayDriver::LIGHT);
      bool any = false;
      for (uint8_t i = 0; i < 7; i++) {
        if (the_mesh.getMonitorLastHopLine(i, tmp, sizeof(tmp))) {
          display.drawTextEllipsized(0, 34 + i * 12, display.width(), tmp);
          any = true;
        }
      }
      if (!any) display.drawTextEllipsized(0, 36, display.width(), "No RX hops");
    } else if (_page == HomePage::SCOPES) {
      uint8_t count = the_mesh.getScopeInfo(scopes, UI_RECENT_LIST_SIZE);
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      display.setCursor(0, 20);
      char header[40];
      snprintf(header, sizeof(header), "Scopes D:%u Q:%u R:%u E:%u",
               (unsigned int)the_mesh.getScopeDiscoverResponseCount(),
               (unsigned int)the_mesh.getLastScopeScanSent(),
               (unsigned int)the_mesh.getScopeResponseCount(),
               (unsigned int)the_mesh.getScopeEmptyResponseCount());
      display.drawTextEllipsized(0, 20, display.width(), header);
      display.setColor(DisplayDriver::LIGHT);
      if (count == 0) {
        display.drawTextEllipsized(0, 36, display.width(), "Doubleclick to scan");
      } else {
        int y = 34;
        for (uint8_t i = 0; i < count; i++, y += 13) {
          char line[64];
          char filtered_name[sizeof(scopes[i].name)];
          display.translateUTF8ToBlocks(filtered_name, scopes[i].name, sizeof(filtered_name));
          snprintf(line, sizeof(line), "%s (%lu)", filtered_name,
                   (unsigned long)scopes[i].rx_count);
          display.drawTextEllipsized(0, y, display.width(), line);
        }
      }
    } else if (_page == HomePage::LOAD) {
      renderLoad(display);
    } else if (_page == HomePage::BLUETOOTH) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18,
          _task->isSerialEnabled() ? bluetooth_on : bluetooth_off,
          32, 32);
      display.setTextSize(1);
      display.drawTextCentered(display.width() / 2, 64 - 11, "toggle: " PRESS_LABEL);
    } else if (_page == HomePage::ADVERT) {
      display.setColor(DisplayDriver::GREEN);
      display.drawXbm((display.width() - 32) / 2, 18, advert_icon, 32, 32);
      display.drawTextCentered(display.width() / 2, 64 - 11, "advert: " PRESS_LABEL);
#if ENV_INCLUDE_GPS == 1
    } else if (_page == HomePage::GPS) {
      LocationProvider* nmea = sensors.getLocationProvider();
      char buf[50];
      int y = 18;
      bool gps_state = _task->getGPSState();
#ifdef PIN_GPS_SWITCH
      bool hw_gps_state = digitalRead(PIN_GPS_SWITCH);
      if (gps_state != hw_gps_state) {
        strcpy(buf, gps_state ? "gps off(hw)" : "gps off(sw)");
      } else {
        strcpy(buf, gps_state ? "gps on" : "gps off");
      }
#else
      strcpy(buf, gps_state ? "gps on" : "gps off");
#endif
      display.drawTextLeftAlign(0, y, buf);
      if (nmea == NULL) {
        y = y + 12;
        display.drawTextLeftAlign(0, y, "Can't access GPS");
      } else {
        bool has_fix = nmea->isValid();
        strcpy(buf, has_fix ? "fix" : "no fix");
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "sats used");
        snprintf(buf, sizeof(buf), "%ld", nmea->satellitesCount());
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "hdop");
        long hdop = nmea->getHDOP();
        if (hdop >= 0) {
          snprintf(buf, sizeof(buf), "%ld.%ld", hdop / 10, hdop % 10);
        } else {
          strcpy(buf, "-");
        }
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "utc");
        long ts = has_fix ? nmea->getTimestamp() : 0;
        if (ts > 0) {
          uint32_t secs = (uint32_t)(ts % 86400L);
          snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu",
                   (unsigned long)(secs / 3600UL),
                   (unsigned long)((secs / 60UL) % 60UL),
                   (unsigned long)(secs % 60UL));
        } else {
          strcpy(buf, "--:--:--");
        }
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "lat");
        if (has_fix) {
          snprintf(buf, sizeof(buf), "%.6f", nmea->getLatitude()/1000000.);
        } else {
          strcpy(buf, "-");
        }
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "lon");
        if (has_fix) {
          snprintf(buf, sizeof(buf), "%.6f", nmea->getLongitude()/1000000.);
        } else {
          strcpy(buf, "-");
        }
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
        display.drawTextLeftAlign(0, y, "alt m");
        if (has_fix) {
          snprintf(buf, sizeof(buf), "%.1f", nmea->getAltitude()/1000.);
        } else {
          strcpy(buf, "-");
        }
        display.drawTextRightAlign(display.width()-1, y, buf);
        y = y + 12;
      }
#endif
#if UI_SENSORS_PAGE == 1
    } else if (_page == HomePage::SENSORS) {
      int y = 18;
      refresh_sensors();
      char buf[30];
      char name[30];
      LPPReader r(sensors_lpp.getBuffer(), sensors_lpp.getSize());

      for (int i = 0; i < sensors_scroll_offset; i++) {
        uint8_t channel, type;
        r.readHeader(channel, type);
        r.skipData(type);
      }

      for (int i = 0; i < (sensors_scroll?UI_RECENT_LIST_SIZE:sensors_nb); i++) {
        uint8_t channel, type;
        if (!r.readHeader(channel, type)) { // reached end, reset
          r.reset();
          r.readHeader(channel, type);
        }

        display.setCursor(0, y);
        float v;
        switch (type) {
          case LPP_GPS: // GPS
            float lat, lon, alt;
            r.readGPS(lat, lon, alt);
            strcpy(name, "gps"); sprintf(buf, "%.4f %.4f", lat, lon);
            break;
          case LPP_VOLTAGE:
            r.readVoltage(v);
            strcpy(name, "voltage"); sprintf(buf, "%6.2f", v);
            break;
          case LPP_CURRENT:
            r.readCurrent(v);
            strcpy(name, "current"); sprintf(buf, "%.3f", v);
            break;
          case LPP_TEMPERATURE:
            r.readTemperature(v);
            strcpy(name, "temperature"); sprintf(buf, "%.2f", v);
            break;
          case LPP_RELATIVE_HUMIDITY:
            r.readRelativeHumidity(v);
            strcpy(name, "humidity"); sprintf(buf, "%.2f", v);
            break;
          case LPP_BAROMETRIC_PRESSURE:
            r.readPressure(v);
            strcpy(name, "pressure"); sprintf(buf, "%.2f", v);
            break;
          case LPP_ALTITUDE:
            r.readAltitude(v);
            strcpy(name, "altitude"); sprintf(buf, "%.0f", v);
            break;
          case LPP_POWER:
            r.readPower(v);
            strcpy(name, "power"); sprintf(buf, "%6.2f", v);
            break;
          default:
            r.skipData(type);
            strcpy(name, "unk"); sprintf(buf, "");
        }
        display.setCursor(0, y);
        display.print(name);
        display.setCursor(
          display.width()-display.getTextWidth(buf)-1, y
        );
        display.print(buf);
        y = y + 12;
      }
      if (sensors_scroll) sensors_scroll_offset = (sensors_scroll_offset+1)%sensors_nb;
      else sensors_scroll_offset = 0;
#endif
    } else if (_page == HomePage::SHUTDOWN) {
      display.setColor(DisplayDriver::GREEN);
      display.setTextSize(1);
      if (_shutdown_init) {
        display.drawTextCentered(display.width() / 2, 34, "hibernating...");
      } else {
        display.drawXbm((display.width() - 32) / 2, 18, power_icon, 32, 32);
        display.drawTextCentered(display.width() / 2, 64 - 11, "hibernate:" PRESS_LABEL);
      }
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    if (c == KEY_ENTER && _page == HomePage::FIRST) {
      if (_task->getMsgCount() > 0) {
        _task->gotoMsgPreviewScreen();
      } else {
        _task->showAlert("No messages", 800);
      }
      return true;
    }
    if (_page == HomePage::SEND && send_mode != SEND_MODE_NAV) {
      if (c == KEY_PREV || c == KEY_SELECT) {
        send_mode = SEND_MODE_NAV;
        _task->showAlert("Canceled", 800);
        return true;
      }
      if (c == KEY_LEFT) {
        if (send_mode == SEND_MODE_TARGET && send_target_count > 0) {
          send_target_idx = (send_target_idx + send_target_count - 1) % send_target_count;
        } else if (send_mode == SEND_MODE_MESSAGE) {
          uint8_t count = getSendMessageCount();
          send_msg_idx = (send_msg_idx + count - 1) % count;
        }
        return true;
      }
      if (c == KEY_NEXT || c == KEY_RIGHT) {
        if (send_mode == SEND_MODE_TARGET && send_target_count > 0) {
          send_target_idx = (send_target_idx + 1) % send_target_count;
        } else if (send_mode == SEND_MODE_MESSAGE) {
          send_msg_idx = (send_msg_idx + 1) % getSendMessageCount();
        }
        return true;
      }
    }
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      send_mode = SEND_MODE_NAV;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      send_mode = SEND_MODE_NAV;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SEND) {
      refreshSendTargets();
      if (send_mode == SEND_MODE_NAV) {
        send_mode = SEND_MODE_TARGET;
      } else if (send_mode == SEND_MODE_TARGET) {
        send_mode = SEND_MODE_MESSAGE;
      } else {
        if (send_target_count == 0) {
          _task->showAlert("No target", 1000);
        } else {
          char quick_msg[112];
          if (!buildSendMessage(send_msg_idx, quick_msg, sizeof(quick_msg))) {
            _task->showAlert("No GPS fix", 1000);
            send_mode = SEND_MODE_NAV;
            return true;
          }
          bool sent_flood = false;
          bool sent = the_mesh.sendQuickText(send_targets[send_target_idx], quick_msg, &sent_flood);
          _task->notify(sent ? UIEventType::ack : UIEventType::none);
          _task->showAlert(sent ? (sent_flood ? "Sent flood" : "Sent") : "Send failed", 1000);
        }
        send_mode = SEND_MODE_NAV;
      }
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::SCOPES) {
      uint8_t sent = the_mesh.queryNearbyScopes();
      _task->showAlert(sent ? "Scope scan sent" : "No repeaters", 1000);
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::BLUETOOTH) {
      if (_task->isSerialEnabled()) {  // toggle Bluetooth on/off
        _task->disableSerial();
      } else {
        _task->enableSerial();
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::ADVERT) {
      _task->notify(UIEventType::ack);
      if (the_mesh.advert()) {
        _task->showAlert("Advert sent!", 1000);
      } else {
        _task->showAlert("Advert failed..", 1000);
      }
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
#endif
#if UI_SENSORS_PAGE == 1
    if (c == KEY_ENTER && _page == HomePage::SENSORS) {
      _task->toggleGPS();
      next_sensors_refresh=0;
      return true;
    }
#endif
    if (c == KEY_ENTER && _page == HomePage::SHUTDOWN) {
      _shutdown_init = true;  // need to wait for button to be released
      return true;
    }
    return false;
  }
};

class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;

  struct MsgEntry {
    uint32_t timestamp;
    uint8_t path_len;
    uint8_t pubkey_prefix[6];
    uint8_t reply_channel_idx;
    bool can_reply;
    bool reply_to_channel;
    bool pinned;
    char reply_mention[32];
    char scope[32];
    char origin[62];
    char msg[78];
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  int head = MAX_UNREAD_MSGS - 1; // index of latest unread message
  bool reply_mode = false;
  bool reply_confirm = false;
  uint8_t reply_idx = 0;
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc, SensorManager* sensors)
      : _task(task), _rtc(rtc), _sensors(sensors) {
    num_unread = 0;
    memset(unread, 0, sizeof(unread));
  }

  void addPreview(uint8_t path_len, const char* from_name, const uint8_t* from_pubkey,
                  uint8_t reply_channel_idx, const char* reply_mention, const char* scope, const char* msg) {
    head = (head + 1) % MAX_UNREAD_MSGS;
    if (num_unread < MAX_UNREAD_MSGS) num_unread++;
    reply_mode = false;
    reply_confirm = false;
    reply_idx = 0;

    auto p = &unread[head];
    memset(p, 0, sizeof(*p));
    p->timestamp = _rtc->getCurrentTime();
    p->path_len = path_len;
    p->reply_channel_idx = reply_channel_idx;
    p->reply_to_channel = from_pubkey == nullptr && reply_channel_idx != 0xFF;
    p->can_reply = from_pubkey != nullptr || p->reply_to_channel;
    if (from_pubkey) memcpy(p->pubkey_prefix, from_pubkey, sizeof(p->pubkey_prefix));
    if (reply_mention) StrHelper::strncpy(p->reply_mention, reply_mention, sizeof(p->reply_mention));
    if (scope) StrHelper::strncpy(p->scope, scope, sizeof(p->scope));
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  void syncUnreadCount(int count) {
    if (count <= 0) {
      num_unread = 0;
      memset(unread, 0, sizeof(unread));
      head = MAX_UNREAD_MSGS - 1;
      reply_mode = false;
      reply_confirm = false;
      reply_idx = 0;
      return;
    }
    num_unread = count > MAX_UNREAD_MSGS ? MAX_UNREAD_MSGS : count;
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    if (num_unread <= 0) {
      display.drawRect(0, 11, display.width(), 1);
      display.setCursor(0, 25);
      display.setColor(DisplayDriver::LIGHT);
      display.print("No messages");
#if AUTO_OFF_MILLIS==0
      return 10000;
#else
      return 1000;
#endif
    }

    auto p = &unread[head];

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    char path_line[56];
    char filtered_scope[sizeof(p->scope)];
    display.translateUTF8ToBlocks(filtered_scope, p->scope, sizeof(filtered_scope));
    if (p->path_len == 0xFF) {
      snprintf(path_line, sizeof(path_line), "%sPath: direct S:%s", p->pinned ? "* " : "", filtered_scope);
    } else {
      snprintf(path_line, sizeof(path_line), "%sPath: %u hops S:%s", p->pinned ? "* " : "",
               (unsigned int)p->path_len, filtered_scope);
    }
    display.setCursor(0, 25);
    display.setColor(DisplayDriver::GREEN);
    display.drawTextEllipsized(0, 25, display.width(), path_line);

    if (reply_mode) {
      display.setCursor(0, 38);
      display.setColor(DisplayDriver::LIGHT);
      display.print(reply_confirm ? "Send?" : "Reply:");
      char reply[112];
      buildReplyText(*p, reply, sizeof(reply));
      display.setCursor(0, 50);
      display.printWordWrap(reply, display.width());
#if AUTO_OFF_MILLIS==0
      return 10000;
#else
      return 1000;
#endif
    }

    display.setCursor(0, 38);
    display.setColor(DisplayDriver::LIGHT);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    auto p = &unread[head];
    if (reply_mode && num_unread > 0) {
      uint8_t count = getReplyMessageCount();
      if (c == KEY_NEXT || c == KEY_RIGHT) {
        if (count > 0) reply_idx = (reply_idx + 1) % count;
        reply_confirm = false;
        return true;
      }
      if (c == KEY_LEFT || c == KEY_PREV) {
        reply_mode = false;
        reply_confirm = false;
        _task->showAlert("Canceled", 800);
        return true;
      }
      if (c == KEY_ENTER) {
        if (!reply_confirm) {
          reply_confirm = true;
          _task->showAlert("Confirm send", 800);
          return true;
        }
        char reply[112];
        buildReplyText(*p, reply, sizeof(reply));
        bool sent_flood = false;
        bool sent = false;
        if (p->reply_to_channel) {
          sent = the_mesh.sendQuickChannelReply(p->reply_channel_idx, p->reply_mention, reply);
        } else {
          sent = p->can_reply &&
                 the_mesh.sendQuickReply(p->pubkey_prefix, sizeof(p->pubkey_prefix), reply, &sent_flood);
        }
        _task->notify(sent ? UIEventType::ack : UIEventType::none);
        _task->showAlert(sent ? (p->reply_to_channel ? "Reply channel" : (sent_flood ? "Reply flood" : "Reply sent")) : "Reply failed", 1000);
        reply_mode = false;
        reply_confirm = false;
        if (sent && !p->pinned) {
          removeCurrent();
        }
        return true;
      }
    }

    if (c == KEY_LEFT || c == KEY_PREV) {
      if (num_unread <= 0) return true;
      p->pinned = !p->pinned;
      _task->showAlert(p->pinned ? "Pinned" : "Unpinned", 800);
      return true;
    }

    if (c == KEY_NEXT || c == KEY_RIGHT) {
      if (num_unread <= 0) {
        num_unread = 0;
        _task->gotoHomeScreen();
        return true;
      }
      if (p->pinned) {
        _task->showAlert("Pinned", 800);
        _task->gotoHomeScreen();
        return true;
      }
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
      num_unread--;
      if (num_unread == 0) {
        _task->msgRead(0);
      } else {
        _task->msgRead(num_unread);
      }
      return true;
    }
    if (c == KEY_ENTER) {
      if (num_unread <= 0) return true;
      if (!p->can_reply) {
        _task->showAlert("No reply target", 1000);
        return true;
      }
      reply_mode = true;
      reply_confirm = false;
      reply_idx = 0;
      return true;
    }
    return false;
  }

private:
  void removeCurrent() {
    if (num_unread <= 0) {
      _task->msgRead(0);
      return;
    }

    memset(&unread[head], 0, sizeof(unread[head]));
    num_unread--;
    if (num_unread <= 0) {
      _task->msgRead(0);
      return;
    }

    head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
    _task->msgRead(num_unread);
  }

  void buildReplyText(const MsgEntry& entry, char* dest, size_t dest_size) const {
    if (!dest || dest_size == 0) return;
    uint8_t quick_count = the_mesh.getQuickMessageCount();
    if (reply_idx < quick_count) {
      snprintf(dest, dest_size, "%s", the_mesh.getQuickMessage(reply_idx));
      return;
    }

    if (reply_idx == getGpsReplyIndex()) {
#if ENV_INCLUDE_GPS == 1
      LocationProvider* gps = _sensors ? _sensors->getLocationProvider() : nullptr;
      if (!gps || !gps->isValid()) {
        snprintf(dest, dest_size, "Meine Position ist: kein GPS fix");
        return;
      }
      double lat = ((double)gps->getLatitude()) / 1000000.0;
      double lon = ((double)gps->getLongitude()) / 1000000.0;
      snprintf(dest, dest_size, "Meine Position ist: %.6f, %.6f", lat, lon);
#else
      snprintf(dest, dest_size, "Meine Position ist: GPS nicht verfuegbar");
#endif
      return;
    }

    if (entry.path_len == 0xFF) {
      snprintf(dest, dest_size, "predef sent from M1 Node : received you. hopcount: direct");
    } else {
      snprintf(dest, dest_size, "predef sent from M1 Node : received you. hopcount: %u",
               (unsigned int)entry.path_len);
    }
  }

  uint8_t getGpsReplyIndex() const {
    return the_mesh.getQuickMessageCount();
  }

  uint8_t getReplyMessageCount() const {
    return the_mesh.getQuickMessageCount() + 2;
  }
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(BUTTON_PIN2)
  user_btn2.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeScreen(this, &rtc_clock, sensors, node_prefs);
  msg_preview = new MsgPreviewScreen(this, &rtc_clock, sensors);
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msg_preview) {
    ((MsgPreviewScreen *)msg_preview)->syncUnreadCount(msgcount);
  }
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const uint8_t* from_pubkey,
                    uint8_t reply_channel_idx, const char* reply_mention,
                    const char* scope, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, from_pubkey,
                                                 reply_channel_idx, reply_mention, scope, text);
  if (curr == msg_preview) {
    setCurrScreen(msg_preview);
  }

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

void UITask::gotoMsgPreviewScreen() {
  if (_msgcount > 0) {
    setCurrScreen(msg_preview);
  } else {
    showAlert("No messages", 800);
  }
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  if (user_btn.isPressed()) return true;
#else
  return false;
#endif
#ifdef BUTTON_PIN2
  return user_btn2.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
  bool fast_message_input = curr == msg_preview;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  user_btn.setMultiClickEnabled(!fast_message_input);
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#if defined(BUTTON_PIN2)
  user_btn2.setMultiClickEnabled(!fast_message_input);
  ev = user_btn2.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_PREV);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {

      // show low battery shutdown alert
      // we should only do this for eink displays, which will persist after power loss
      #if defined(THINKNODE_M1) || defined(LILYGO_TECHO)
      if (_display != NULL) {
        _display->startFrame();
        _display->setTextSize(2);
        _display->setColor(DisplayDriver::RED);
        _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
        _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
        _display->endFrame();
      }
      #endif

      shutdown();

    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double click triggered");
  checkDisplayOn(c);
  return KEY_CONTEXT_MENU;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  } 
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}

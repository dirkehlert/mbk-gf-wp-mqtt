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

    // meshcore website
    const char* website = "https://meshcore.io";
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    uint16_t websiteWidth = display.getTextWidth(website);
    display.setCursor((display.width() - websiteWidth) / 2, 22);
    display.print(website);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 35, _version_info);

    display.drawTextCentered(display.width()/2, 44, "Fieldtest fuer");
    display.drawTextCentered(display.width()/2, 57, "peter.kernspecht@web.de");
    display.drawTextCentered(display.width()/2, 70, "by Moorbock");

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
    HEATSTRIP,
    POSITIONS,
    LOAD,
#if ENV_INCLUDE_GPS == 1
    GPS,
#endif
#if UI_SENSORS_PAGE == 1
    SENSORS,
#endif
    Count    // keep as last
  };

  UITask* _task;
  mesh::RTCClock* _rtc;
  SensorManager* _sensors;
  NodePrefs* _node_prefs;
  uint8_t _page;
  bool _shutdown_init;
  bool reset_confirm = false;
  AdvertPath recent[UI_RECENT_LIST_SIZE];
  ScopeInfo scopes[UI_RECENT_LIST_SIZE];
  static const uint8_t TRACK_FAVS = 16;
  static const uint8_t POSITION_NODES = 12;
  FavouriteChatContact favourite_chats[TRACK_FAVS];
  FavouritePositionInfo favourite_positions[POSITION_NODES];
  uint8_t position_count = 0;
  uint8_t position_idx = 0;
  bool track_panel_open = false;
  bool actions_panel_open = false;
  uint8_t action_idx = 0;
  uint8_t load_panel = 0;

  static const uint8_t HEAT_ROWS = 6;
  static const uint8_t HEAT_PATHS = 8;
  static const uint8_t SEND_TARGETS = 48;
  static const uint8_t SEND_MESSAGES = 10; // nine editable messages plus GPS position
  static const uint8_t SEND_SCOPES = 16; // default plus discovered scope names
  static const uint8_t TRACK_DURATIONS = 5;
  static const uint8_t ACTION_COUNT = 4;

  enum SendMode {
    SEND_MODE_NAV,
    SEND_MODE_TARGET,
    SEND_MODE_MESSAGE,
    SEND_MODE_SCOPE
  };

  QuickSendTarget send_targets[SEND_TARGETS];
  ScopeInfo send_scopes[SEND_SCOPES - 1];
  uint8_t send_target_count = 0;
  uint8_t send_target_idx = 0;
  uint8_t send_msg_idx = 0;
  uint8_t send_scope_count = 1;
  uint8_t send_scope_idx = 0;
  uint8_t send_scope_draft_idx = 0;
  char send_scope_name[SCOPE_NAME_SIZE] = "";
  SendMode send_mode = SEND_MODE_NAV;

  enum TrackMode {
    TRACK_MODE_NAV,
    TRACK_MODE_CONTACT,
    TRACK_MODE_DURATION
  };

  TrackMode track_mode = TRACK_MODE_NAV;
  uint8_t track_fav_count = 0;
  uint8_t track_fav_idx = 0;
  uint8_t track_duration_idx = 0;
  const uint16_t track_durations[TRACK_DURATIONS] = { 0, 5, 15, 30, 60 };

  bool isCompact(DisplayDriver& display) const {
    return display.height() <= 64;
  }

  int contentTop(DisplayDriver& display) const {
    return isCompact(display) ? 12 : 20;
  }

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
    int top = contentTop(display);
    display.setCursor(0, top);
    display.print("Load");
    display.setColor(DisplayDriver::LIGHT);
    char line[48];
    snprintf(line, sizeof(line), "Now %s Max %s", now_pct, max_pct);
    display.drawTextEllipsized(0, top + 12, display.width(), line);
    snprintf(line, sizeof(line), "Avg %s / airtime", avg_pct);
    display.drawTextEllipsized(0, top + 23, display.width(), line);

    const int chart_top = isCompact(display) ? top + 34 : 58;
    const int chart_bottom = isCompact(display) ? display.height() - 2 : 94;
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

  void refreshFavouriteChats() {
    track_fav_count = the_mesh.getFavouriteChatContacts(favourite_chats, TRACK_FAVS);
    if (track_fav_count == 0) {
      track_fav_idx = 0;
    } else if (track_fav_idx >= track_fav_count) {
      track_fav_idx = track_fav_count - 1;
    }
  }

  const char* bearingLabel(uint16_t bearing) const {
    static const char* dirs[] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    return dirs[((bearing + 22) % 360) / 45];
  }

  void formatDistance(char* dest, size_t dest_size, float meters) const {
    if (meters < 1000.0f) {
      snprintf(dest, dest_size, "%.0fm", meters);
    } else {
      snprintf(dest, dest_size, "%.2fkm", meters / 1000.0f);
    }
  }

  void formatUtc(char* dest, size_t dest_size, uint32_t utc) const {
    if (utc == 0) {
      snprintf(dest, dest_size, "--:--:--");
      return;
    }
    uint32_t secs = utc % 86400UL;
    snprintf(dest, dest_size, "%02lu:%02lu:%02lu",
             (unsigned long)(secs / 3600UL),
             (unsigned long)((secs / 60UL) % 60UL),
             (unsigned long)(secs % 60UL));
  }

  void renderTrack(DisplayDriver& display) {
    refreshFavouriteChats();

    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    display.drawTextEllipsized(0, 20, display.width(), "Position track");

    char line[64];
    if (track_mode == TRACK_MODE_CONTACT) {
      display.setColor(DisplayDriver::LIGHT);
      if (track_fav_count == 0) {
        display.drawTextEllipsized(0, 42, display.width(), "No fav chat contacts");
      } else {
        char filtered_name[sizeof(favourite_chats[0].name)];
        display.translateUTF8ToBlocks(filtered_name, favourite_chats[track_fav_idx].name, sizeof(filtered_name));
        snprintf(line, sizeof(line), "%u/%u %s",
                 (unsigned int)track_fav_idx + 1,
                 (unsigned int)track_fav_count,
                 filtered_name);
        display.drawTextEllipsized(0, 42, display.width(), line);
      }
      display.setColor(DisplayDriver::GREEN);
      display.drawTextCentered(display.width() / 2, 78, "click: next");
      display.drawTextCentered(display.width() / 2, 94, "hold: time");
      return;
    }

    if (track_mode == TRACK_MODE_DURATION) {
      display.setColor(DisplayDriver::LIGHT);
      if (track_durations[track_duration_idx] == 0) {
        snprintf(line, sizeof(line), "Duration: twice");
      } else {
        snprintf(line, sizeof(line), "Duration: %umin", (unsigned int)track_durations[track_duration_idx]);
      }
      display.drawTextEllipsized(0, 42, display.width(), line);
      display.setColor(DisplayDriver::GREEN);
      display.drawTextCentered(display.width() / 2, 78, "click: next");
      display.drawTextCentered(display.width() / 2, 94, "hold: start");
      return;
    }

    PositionTrackInfo info;
    the_mesh.getPositionTrackInfo(info);
    if (!info.active && !info.has_position) {
      display.setColor(DisplayDriver::LIGHT);
      display.drawTextEllipsized(0, 38, display.width(), "hold: select fav");
      display.drawTextEllipsized(0, 52, display.width(), "tracks every minute");
      return;
    }

    char filtered_name[sizeof(info.target_name)];
    display.translateUTF8ToBlocks(filtered_name, info.target_name, sizeof(filtered_name));
    if (info.twice_mode) {
      snprintf(line, sizeof(line), "%s twice %u/2",
               info.active ? "On" : "Done",
               (unsigned int)info.resp_count);
    } else {
      snprintf(line, sizeof(line), "%s %umin",
               info.active ? "On" : "Done",
               (unsigned int)info.remaining_min);
    }
    display.drawTextEllipsized(0, 34, display.width(), filtered_name);
    display.drawTextEllipsized(0, 47, display.width(), line);

    if (info.has_position) {
      char dist[16];
      formatDistance(dist, sizeof(dist), info.distance_m);
      snprintf(line, sizeof(line), "%s %u deg %s",
               bearingLabel(info.bearing_deg),
               (unsigned int)info.bearing_deg,
               dist);
      display.setColor(DisplayDriver::YELLOW);
      display.drawTextEllipsized(0, 63, display.width(), line);
      display.setColor(DisplayDriver::LIGHT);
      snprintf(line, sizeof(line), "age %us ref:%s",
               (unsigned int)info.age_secs,
               info.ref_is_live_gps ? "gps" : "saved");
      display.drawTextEllipsized(0, 78, display.width(), line);
      char utc[12];
      formatUtc(utc, sizeof(utc), info.last_resp_utc);
      if (info.hop_count == OUT_PATH_UNKNOWN) {
        snprintf(line, sizeof(line), "utc %s h?", utc);
      } else {
        snprintf(line, sizeof(line), "utc %s h%u", utc, (unsigned int)info.hop_count);
      }
      display.drawTextEllipsized(0, 91, display.width(), line);
    } else {
      display.setColor(info.has_error ? DisplayDriver::RED : DisplayDriver::LIGHT);
      display.drawTextEllipsized(0, 68, display.width(), info.waiting ? "waiting target..." : "no target position");
      snprintf(line, sizeof(line), "q%u r%u ng%u %s",
               (unsigned int)info.req_count,
               (unsigned int)info.resp_count,
               (unsigned int)info.no_gps_count,
               info.last_sent_flood ? "flood" : "direct");
      display.drawTextEllipsized(0, 82, display.width(), line);
      if (info.last_resp_len > 0) {
        char utc[12];
        formatUtc(utc, sizeof(utc), info.last_resp_utc);
        if (info.hop_count == OUT_PATH_UNKNOWN) {
          snprintf(line, sizeof(line), "len %u h? utc %s", (unsigned int)info.last_resp_len, utc);
        } else {
          snprintf(line, sizeof(line), "len %u h%u utc %s",
                   (unsigned int)info.last_resp_len,
                   (unsigned int)info.hop_count,
                   utc);
        }
        display.drawTextEllipsized(0, 94, display.width(), line);
      }
    }
  }

  void refreshFavouritePositions() {
    position_count = the_mesh.getFavouritePositions(favourite_positions, POSITION_NODES);
    if (position_count == 0) {
      position_idx = 0;
    } else if (position_idx > position_count) {
      position_idx = position_count;
    }
  }

  void renderPositions(DisplayDriver& display) {
    refreshFavouritePositions();

    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);

    char line[64];
    float max_dist = 100.0f;
    for (uint8_t i = 0; i < position_count; i++) {
      if (favourite_positions[i].distance_m > max_dist) max_dist = favourite_positions[i].distance_m;
    }
    if (position_count == 0) {
      display.setColor(DisplayDriver::LIGHT);
      display.drawTextEllipsized(0, 32, display.width(), "No fav positions");
      display.drawTextEllipsized(0, 46, display.width(), "track or receive adverts");
      return;
    }

    bool overview = position_idx == 0;
    uint8_t selected_idx = overview ? 0 : position_idx - 1;
    float scale_m = max_dist;
    if (!overview) {
      scale_m = favourite_positions[selected_idx].distance_m * 1.25f;
    }
    if (scale_m < 100.0f) scale_m = 100.0f;
    if (overview && scale_m < 1000.0f) scale_m = 1000.0f;

    char scale[16];
    formatDistance(scale, sizeof(scale), scale_m);

    bool compact = isCompact(display);
    const int cx = compact ? 36 : display.width() / 2;
    const int cy = compact ? 35 : 62;
    const int radius = compact ? 21 : 38;
    display.setColor(DisplayDriver::DARK);
    display.drawRect(cx - radius, cy - radius, radius * 2, radius * 2);
    display.drawRect(cx - (radius * 2 / 3), cy - (radius * 2 / 3), radius * 4 / 3, radius * 4 / 3);
    display.drawRect(cx - (radius / 3), cy - (radius / 3), radius * 2 / 3, radius * 2 / 3);
    display.fillRect(cx - 1, cy - 1, 3, 3);
    if (!compact) display.drawTextCentered(cx, cy - radius - 11, "N");

    for (uint8_t i = 0; i < position_count; i++) {
      const FavouritePositionInfo& pos = favourite_positions[i];
      float rel = pos.distance_m / scale_m;
      if (rel > 1.0f) rel = 1.0f;
      double rad = ((double)pos.bearing_deg) * 0.017453292519943295;
      int px = cx + (int)round(sin(rad) * rel * radius);
      int py = cy - (int)round(cos(rad) * rel * radius);
      if (px < cx - radius + 2) px = cx - radius + 2;
      if (px > cx + radius - 2) px = cx + radius - 2;
      if (py < cy - radius + 2) py = cy - radius + 2;
      if (py > cy + radius - 2) py = cy + radius - 2;

      bool selected = !overview && i == selected_idx;
      display.setColor(selected ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
      if (pos.source == FAV_POSITION_SOURCE_TRACK) {
        display.fillRect(px - 2, py - 2, 5, 5);
      } else if (selected) {
        display.drawRect(px - 3, py - 3, 7, 7);
        display.fillRect(px - 1, py - 1, 3, 3);
      } else {
        display.drawRect(px - 2, py - 2, 5, 5);
      }
    }

    if (overview) {
      display.setColor(DisplayDriver::YELLOW);
      snprintf(line, sizeof(line), "Overview %u nodes", (unsigned int)position_count);
      display.drawTextEllipsized(compact ? 74 : 0, compact ? 18 : 105,
                                 compact ? display.width() - 74 : display.width(), line);
      display.setColor(DisplayDriver::LIGHT);
      display.drawTextEllipsized(compact ? 74 : 86, compact ? 31 : 117,
                                 display.width() - (compact ? 74 : 86), scale);
      return;
    }

    const FavouritePositionInfo& selected = favourite_positions[selected_idx];
    char name[sizeof(selected.name)];
    display.translateUTF8ToBlocks(name, selected.name, sizeof(name));
    char dist[16];
    formatDistance(dist, sizeof(dist), selected.distance_m);
    display.setColor(DisplayDriver::YELLOW);
    snprintf(line, sizeof(line), "%c %s",
             selected.source == FAV_POSITION_SOURCE_TRACK ? 'T' : 'A',
             name);
    display.drawTextEllipsized(compact ? 74 : 0, compact ? 18 : 105,
                               compact ? display.width() - 74 : display.width(), line);
    display.setColor(DisplayDriver::LIGHT);
    snprintf(line, sizeof(line), "%s %s", dist, bearingLabel(selected.bearing_deg));
    display.drawTextEllipsized(compact ? 74 : 0, compact ? 31 : 117,
                               compact ? display.width() - 74 : 72, line);
    display.drawTextEllipsized(compact ? 74 : 86, compact ? 44 : 117,
                               display.width() - (compact ? 74 : 86), scale);
  }

  const char* getActionName(uint8_t index) const {
    switch (index) {
      case 0: return "Send advert";
      case 1: return _task->isSerialEnabled() ? "BLE off" : "BLE on";
      case 2: return the_mesh.isPacketForwardEnabled() ? "Pkt fwd off" : "Pkt fwd on";
      case 3: return "Hibernate";
      default: return "";
    }
  }

  void renderActions(DisplayDriver& display) {
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    int top = contentTop(display);
    int row_h = isCompact(display) ? 10 : 16;
    display.drawTextEllipsized(0, top, display.width(), "Actions");
    display.setColor(DisplayDriver::LIGHT);
    for (uint8_t i = 0; i < ACTION_COUNT; i++) {
      char line[40];
      snprintf(line, sizeof(line), "%c %s", i == action_idx ? '>' : ' ', getActionName(i));
      display.setColor(i == action_idx ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
      display.drawTextEllipsized(0, top + 13 + i * row_h, display.width(), line);
    }
  }

  void renderHeardRepeaters(DisplayDriver& display) {
    char tmp[80];
    display.setColor(DisplayDriver::GREEN);
    display.setTextSize(1);
    int top = contentTop(display);
    int row_h = isCompact(display) ? 10 : 12;
    uint8_t max_rows = isCompact(display) ? 4 : 7;
    display.drawTextEllipsized(0, top, display.width(), "Heard repeaters");
    display.setColor(DisplayDriver::LIGHT);
    bool any = false;
    for (uint8_t i = 0; i < max_rows; i++) {
      if (the_mesh.getMonitorLastHopLine(i, tmp, sizeof(tmp))) {
        display.drawTextEllipsized(0, top + 12 + i * row_h, display.width(), tmp);
        any = true;
      }
    }
    if (!any) display.drawTextEllipsized(0, top + 14, display.width(), "No RX hops");
  }

  void renderScopes(DisplayDriver& display) {
    uint8_t count = the_mesh.getScopeInfo(scopes, UI_RECENT_LIST_SIZE);
    display.setColor(DisplayDriver::GREEN);
    display.setTextSize(1);
    int top = contentTop(display);
    int row_h = isCompact(display) ? 10 : 13;
    uint8_t max_rows = isCompact(display) ? 4 : UI_RECENT_LIST_SIZE;
    char header[40];
    if (isCompact(display)) {
      snprintf(header, sizeof(header), "Scopes Q:%u R:%u",
               (unsigned int)the_mesh.getLastScopeScanSent(),
               (unsigned int)the_mesh.getScopeResponseCount());
    } else {
      snprintf(header, sizeof(header), "Scopes D:%u Q:%u R:%u E:%u",
               (unsigned int)the_mesh.getScopeDiscoverResponseCount(),
               (unsigned int)the_mesh.getLastScopeScanSent(),
               (unsigned int)the_mesh.getScopeResponseCount(),
               (unsigned int)the_mesh.getScopeEmptyResponseCount());
    }
    display.drawTextEllipsized(0, top, display.width(), header);
    display.setColor(DisplayDriver::LIGHT);
    if (count == 0) {
      display.drawTextEllipsized(0, top + 14, display.width(), "Longpress to scan");
    } else {
      int y = top + 12;
      for (uint8_t i = 0; i < count && i < max_rows; i++, y += row_h) {
        char line[64];
        char filtered_name[sizeof(scopes[i].name)];
        display.translateUTF8ToBlocks(filtered_name, scopes[i].name, sizeof(filtered_name));
        snprintf(line, sizeof(line), "%s%s (%lu)",
                 (scopes[i].flags & SCOPE_FLAG_DISCOVERED) ? "*" : "",
                 filtered_name,
                 (unsigned long)scopes[i].rx_count);
        display.drawTextEllipsized(0, y, display.width(), line);
      }
    }
  }

  void runSelectedAction() {
    if (action_idx == 0) {
      _task->notify(UIEventType::ack);
      _task->showAlert(the_mesh.advert() ? "Advert sent!" : "Advert failed", 1000);
      actions_panel_open = false;
    } else if (action_idx == 1) {
      if (_task->isSerialEnabled()) {
        _task->disableSerial();
        _task->showAlert("BLE off", 1000);
      } else {
        _task->enableSerial();
        _task->showAlert("BLE on", 1000);
      }
    } else if (action_idx == 2) {
      bool enable = !the_mesh.isPacketForwardEnabled();
      bool ok = the_mesh.setPacketForwardEnabled(enable);
      if (ok) {
        _task->showAlert(enable ? "Pkt fwd on" : "Pkt fwd off", 1000);
      } else {
        _task->showAlert("Freq not allowed", 1000);
      }
    } else if (action_idx == 3) {
      _shutdown_init = true;
      _task->showAlert("hibernating...", 1000);
    }
  }

  void refreshSendTargets() {
    send_target_count = the_mesh.getQuickSendTargets(send_targets, SEND_TARGETS);
    if (send_target_count == 0) {
      send_target_idx = 0;
    } else if (send_target_idx >= send_target_count) {
      send_target_idx = send_target_count - 1;
    }
  }

  void refreshSendScopes() {
    send_scope_count = 1 + the_mesh.getScopeInfo(send_scopes, SEND_SCOPES - 1);
    if (send_scope_count == 0) send_scope_count = 1;
    send_scope_idx = findSendScopeIndex(send_scope_name);
    if (send_scope_draft_idx >= send_scope_count) send_scope_draft_idx = send_scope_idx;
  }

  uint8_t findSendScopeIndex(const char* name) const {
    if (!name || !name[0]) return 0;
    for (uint8_t i = 1; i < send_scope_count; i++) {
      if (strcmp(send_scopes[i - 1].name, name) == 0) return i;
    }
    return 0;
  }

  const char* getSendScopeName(uint8_t scope_idx) const {
    if (scope_idx == 0) return "Default";
    uint8_t idx = scope_idx - 1;
    if (idx >= SEND_SCOPES - 1 || send_scopes[idx].name[0] == 0) return "Default";
    return send_scopes[idx].name;
  }

  const char* getSendScopeName() const {
    return send_scope_name[0] ? send_scope_name : "Default";
  }

  void applySendScope() {
    if (send_scope_name[0] == 0) {
      the_mesh.clearQuickSendScope();
    } else {
      the_mesh.setQuickSendScope(send_scope_name);
    }
  }

  void renderSend(DisplayDriver& display) {
    refreshSendTargets();
    refreshSendScopes();

    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    bool compact = isCompact(display);
    int top = contentTop(display);
    display.setCursor(0, top);
    display.print(send_mode == SEND_MODE_SCOPE ? "Select scope" : "Send");

    char line[64];
    if (send_mode == SEND_MODE_SCOPE) {
      display.setColor(DisplayDriver::LIGHT);
      char filtered_scope[SCOPE_NAME_SIZE];
      display.translateUTF8ToBlocks(filtered_scope, getSendScopeName(send_scope_draft_idx), sizeof(filtered_scope));
      snprintf(line, sizeof(line), "%u/%u %s",
               (unsigned int)send_scope_draft_idx + 1,
               (unsigned int)send_scope_count,
               filtered_scope);
      display.drawTextEllipsized(0, compact ? top + 14 : 42, display.width(), line);
      display.setColor(DisplayDriver::GREEN);
      display.drawTextCentered(display.width() / 2, compact ? top + 28 : 78, "click: next");
      if (!compact) display.drawTextCentered(display.width() / 2, 94, "hold: select");
      return;
    }

    display.setColor(send_mode == SEND_MODE_TARGET ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
    int y_to = compact ? top + 12 : 33;
    int y_msg = compact ? top + 25 : 52;
    int y_text = compact ? top + 37 : 64;
    int y_scope = compact ? top + 49 : 78;
    display.drawTextEllipsized(0, y_to, display.width(), "To");
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
    display.drawTextEllipsized(18, y_to, display.width() - 18, line);

    display.setColor(send_mode == SEND_MODE_MESSAGE ? DisplayDriver::YELLOW : DisplayDriver::LIGHT);
    uint8_t send_msg_count = getSendMessageCount();
    if (send_msg_idx >= send_msg_count) send_msg_idx = send_msg_count - 1;
    snprintf(line, sizeof(line), "Msg %u/%u", (unsigned int)send_msg_idx + 1, (unsigned int)send_msg_count);
    display.drawTextEllipsized(0, y_msg, display.width(), line);
    display.setColor(DisplayDriver::LIGHT);
    display.drawTextEllipsized(0, y_text, display.width(), getSendMessage(send_msg_idx));

    char filtered_scope[SCOPE_NAME_SIZE];
    display.translateUTF8ToBlocks(filtered_scope, getSendScopeName(), sizeof(filtered_scope));
    snprintf(line, sizeof(line), "Scope: %s", filtered_scope);
    display.drawTextEllipsized(0, y_scope, display.width(), line);

    display.setColor(DisplayDriver::GREEN);
    const char* action = "hold: target";
    if (send_mode == SEND_MODE_TARGET) action = "hold: msg";
    else if (send_mode == SEND_MODE_MESSAGE) action = "hold: send";
    if (!compact) display.drawTextCentered(display.width() / 2, 96, action);
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

    bool compact = isCompact(display);
    int iconWidth = compact ? 16 : 24;
    int iconHeight = compact ? 7 : 10;
    int iconX = display.width() - iconWidth - 5; // Position the icon near the top-right corner
    int iconY = 0;
    display.setColor(DisplayDriver::GREEN);

    // battery outline
    display.drawRect(iconX, iconY, iconWidth, iconHeight);

    // battery "cap"
    display.fillRect(iconX + iconWidth, iconY + (iconHeight / 4), compact ? 2 : 3, iconHeight / 2);

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
    bool compact = isCompact(display);
    // node name
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    char filtered_name[sizeof(_node_prefs->node_name)];
    display.translateUTF8ToBlocks(filtered_name, _node_prefs->node_name, sizeof(filtered_name));
    display.drawTextEllipsized(0, 0, compact ? display.width() - 26 : display.width() - 34, filtered_name);

    // battery voltage
    renderBatteryIndicator(display, _task->getBattMilliVolts());

    // curr page indicator
    int y = compact ? 9 : 14;
    int x = display.width() / 2 - (compact ? 4 : 5) * (HomePage::Count-1);
    int dot_step = compact ? 8 : 10;
    for (uint8_t i = 0; i < HomePage::Count; i++, x += dot_step) {
      if (i == _page) {
        display.fillRect(x-1, y-1, 3, 3);
      } else {
        display.fillRect(x, y, 1, 1);
      }
    }

    if (_page == HomePage::FIRST) {
      if (actions_panel_open) {
        renderActions(display);
      } else {
        display.setColor(DisplayDriver::YELLOW);
        display.setTextSize(compact ? 1 : 2);
        sprintf(tmp, "MSG: %d", _task->getMsgCount());
        display.drawTextCentered(display.width() / 2, compact ? 13 : 20, tmp);

        #ifdef WIFI_SSID
          IPAddress ip = WiFi.localIP();
          snprintf(tmp, sizeof(tmp), "IP: %d.%d.%d.%d", ip[0], ip[1], ip[2], ip[3]);
          display.setTextSize(1);
          display.drawTextCentered(display.width() / 2, compact ? 24 : 54, tmp);
        #endif
        if (_task->hasConnection()) {
          display.setColor(DisplayDriver::GREEN);
          display.setTextSize(1);
          display.drawTextCentered(display.width() / 2, compact ? 24 : 43, "< Connected >");

        } else if (the_mesh.getBLEPin() != 0) { // BT pin
          display.setColor(DisplayDriver::RED);
          display.setTextSize(compact ? 1 : 2);
          sprintf(tmp, "Pin:%d", the_mesh.getBLEPin());
          display.drawTextCentered(display.width() / 2, compact ? 24 : 43, tmp);
        }
        display.setTextSize(1);
        display.setColor(DisplayDriver::LIGHT);
        snprintf(tmp, sizeof(tmp), "Noise floor: %d", radio_driver.getNoiseFloor());
        display.drawTextEllipsized(0, compact ? 34 : 62, display.width(), tmp);
        snprintf(tmp, sizeof(tmp), "RX: %lu  SNR: %.1f",
                 (unsigned long)the_mesh.getMonitorRxPackets(), the_mesh.getMonitorLastSnr());
        display.drawTextEllipsized(0, compact ? 45 : 74, display.width(), tmp);
        if (!compact) display.drawTextEllipsized(0, 88, display.width(), "Last path:");
        if (the_mesh.getMonitorLatestPathLine(tmp, sizeof(tmp))) {
          display.drawTextEllipsized(0, compact ? 56 : 100, display.width(), tmp);
        } else {
          display.drawTextEllipsized(0, compact ? 56 : 100, display.width(), "-");
        }
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
    } else if (_page == HomePage::HEATSTRIP) {
      renderHeatstrip(display);
    } else if (_page == HomePage::POSITIONS) {
      if (track_panel_open) {
        renderTrack(display);
      } else {
        renderPositions(display);
      }
    } else if (_page == HomePage::LOAD) {
      if (load_panel == 0) {
        renderLoad(display);
      } else if (load_panel == 1) {
        renderHeardRepeaters(display);
      } else {
        renderScopes(display);
      }
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
    }
    if (_page == HomePage::POSITIONS && track_panel_open) {
      PositionTrackInfo info;
      if (the_mesh.getPositionTrackInfo(info) && info.active) return 10000;
      return 3000;
    }
    if (_page == HomePage::POSITIONS) {
      return 10000;
    }
    if (_page == HomePage::FIRST) {
      return 30000;
    }
    return 5000;   // next render after 5000 ms
  }

  bool handleInput(char c) override {
    if (_page == HomePage::FIRST && actions_panel_open) {
      if (c == KEY_PREV || c == KEY_SELECT) {
        actions_panel_open = false;
        return true;
      }
      if (c == KEY_CONTEXT_MENU || c == KEY_NEXT || c == KEY_RIGHT) {
        action_idx = (action_idx + 1) % ACTION_COUNT;
        return true;
      }
      if (c == KEY_LEFT) {
        action_idx = (action_idx + ACTION_COUNT - 1) % ACTION_COUNT;
        return true;
      }
      if (c == KEY_ENTER) {
        runSelectedAction();
        return true;
      }
    }
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
        } else if (send_mode == SEND_MODE_SCOPE) {
          send_scope_draft_idx = (send_scope_draft_idx + send_scope_count - 1) % send_scope_count;
        }
        return true;
      }
      if (c == KEY_NEXT || c == KEY_RIGHT) {
        if (send_mode == SEND_MODE_TARGET && send_target_count > 0) {
          send_target_idx = (send_target_idx + 1) % send_target_count;
        } else if (send_mode == SEND_MODE_MESSAGE) {
          send_msg_idx = (send_msg_idx + 1) % getSendMessageCount();
        } else if (send_mode == SEND_MODE_SCOPE) {
          send_scope_draft_idx = (send_scope_draft_idx + 1) % send_scope_count;
        }
        return true;
      }
      if (c == KEY_ENTER && send_mode == SEND_MODE_SCOPE) {
        send_scope_idx = send_scope_draft_idx;
        if (send_scope_idx == 0) {
          send_scope_name[0] = 0;
        } else {
          StrHelper::strncpy(send_scope_name, getSendScopeName(send_scope_idx), sizeof(send_scope_name));
        }
        applySendScope();
        send_mode = SEND_MODE_NAV;
        char label[40];
        snprintf(label, sizeof(label), "Scope: %s", getSendScopeName());
        _task->showAlert(label, 1000);
        return true;
      }
      if (c == KEY_CONTEXT_MENU && send_mode == SEND_MODE_SCOPE) {
        send_scope_draft_idx = (send_scope_draft_idx + 1) % send_scope_count;
        return true;
      }
      if (c == KEY_CONTEXT_MENU) {
        if (send_mode == SEND_MODE_TARGET && send_target_count > 0) {
          send_target_idx = (send_target_idx + 1) % send_target_count;
        } else if (send_mode == SEND_MODE_MESSAGE) {
          send_msg_idx = (send_msg_idx + 1) % getSendMessageCount();
        }
        return true;
      }
    }
    if (_page == HomePage::POSITIONS && track_panel_open && track_mode != TRACK_MODE_NAV) {
      if (c == KEY_PREV || c == KEY_SELECT) {
        track_mode = TRACK_MODE_NAV;
        track_panel_open = false;
        return true;
      }
      if (c == KEY_LEFT) {
        if (track_mode == TRACK_MODE_CONTACT && track_fav_count > 0) {
          track_fav_idx = (track_fav_idx + track_fav_count - 1) % track_fav_count;
        } else if (track_mode == TRACK_MODE_DURATION) {
          track_duration_idx = (track_duration_idx + TRACK_DURATIONS - 1) % TRACK_DURATIONS;
        }
        return true;
      }
      if (c == KEY_NEXT || c == KEY_RIGHT || c == KEY_CONTEXT_MENU) {
        if (track_mode == TRACK_MODE_CONTACT && track_fav_count > 0) {
          track_fav_idx = (track_fav_idx + 1) % track_fav_count;
        } else if (track_mode == TRACK_MODE_DURATION) {
          track_duration_idx = (track_duration_idx + 1) % TRACK_DURATIONS;
        }
        return true;
      }
      if (c == KEY_ENTER) {
        if (track_mode == TRACK_MODE_CONTACT) {
          if (track_fav_count == 0) {
            track_mode = TRACK_MODE_NAV;
            _task->showAlert("No favorites", 1000);
          } else {
            track_mode = TRACK_MODE_DURATION;
          }
        } else if (track_mode == TRACK_MODE_DURATION) {
          bool started = the_mesh.startPositionTrack(favourite_chats[track_fav_idx].pubkey_prefix,
                                                     sizeof(favourite_chats[track_fav_idx].pubkey_prefix),
                                                     track_durations[track_duration_idx]);
          track_mode = TRACK_MODE_NAV;
          _task->showAlert(started ? "Tracking started" : "Track failed", 1000);
        }
        return true;
      }
    }
    if (c == KEY_LEFT || c == KEY_PREV) {
      _page = (_page + HomePage::Count - 1) % HomePage::Count;
      send_mode = SEND_MODE_NAV;
      track_mode = TRACK_MODE_NAV;
      track_panel_open = false;
      actions_panel_open = false;
      load_panel = 0;
      reset_confirm = false;
      return true;
    }
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      _page = (_page + 1) % HomePage::Count;
      send_mode = SEND_MODE_NAV;
      track_mode = TRACK_MODE_NAV;
      track_panel_open = false;
      actions_panel_open = false;
      load_panel = 0;
      reset_confirm = false;
      if (_page == HomePage::RECENT) {
        _task->showAlert("Recent adverts", 800);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::SEND) {
      refreshSendTargets();
      refreshSendScopes();
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
          applySendScope();
          bool sent = the_mesh.sendQuickText(send_targets[send_target_idx], quick_msg, &sent_flood);
          _task->notify(sent ? UIEventType::ack : UIEventType::none);
          _task->showAlert(sent ? (sent_flood ? "Sent flood" : "Sent") : "Send failed", 1000);
        }
        send_mode = SEND_MODE_NAV;
      }
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::SEND) {
      refreshSendScopes();
      send_scope_draft_idx = findSendScopeIndex(send_scope_name);
      send_mode = SEND_MODE_SCOPE;
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::POSITIONS && track_panel_open) {
      PositionTrackInfo info;
      the_mesh.getPositionTrackInfo(info);
      if (info.active) {
        the_mesh.stopPositionTrack();
        _task->showAlert("Tracking stopped", 1000);
      } else if (info.has_position || info.target_name[0]) {
        the_mesh.clearPositionTrackResult();
        _task->showAlert("Track cleared", 1000);
      } else {
        refreshFavouriteChats();
        if (track_fav_count == 0) {
          _task->showAlert("No favorites", 1000);
        } else {
          track_mode = TRACK_MODE_CONTACT;
        }
      }
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::POSITIONS) {
      if (track_panel_open) return true;
      refreshFavouritePositions();
      if (position_count == 0) {
        _task->showAlert("No positions", 1000);
      } else {
        position_idx = (position_idx + 1) % (position_count + 1);
      }
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::POSITIONS) {
      track_panel_open = true;
      track_mode = TRACK_MODE_NAV;
      return true;
    }
    if (c == KEY_ENTER && _page == HomePage::LOAD) {
      if (load_panel == 2) {
        uint8_t sent = the_mesh.queryNearbyScopes();
        _task->showAlert(sent ? "Scope scan sent" : "No repeaters", 1000);
      } else if (load_panel == 0) {
        if (!reset_confirm) {
          reset_confirm = true;
          _task->showAlert("Reset stats?", 1500);
        } else {
          the_mesh.resetMonitorLiveStats();
          reset_confirm = false;
          _task->showAlert("Stats reset", 1000);
        }
      }
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::LOAD) {
      load_panel = (load_panel + 1) % 3;
      reset_confirm = false;
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::FIRST) {
      actions_panel_open = true;
      return true;
    }
#if ENV_INCLUDE_GPS == 1
    if (c == KEY_ENTER && _page == HomePage::GPS) {
      _task->toggleGPS();
      return true;
    }
    if (c == KEY_CONTEXT_MENU && _page == HomePage::GPS) {
      LocationProvider* gps = sensors.getLocationProvider();
      if (gps && gps->isValid()) {
        sensors.node_lat = ((double)gps->getLatitude()) / 1000000.0;
        sensors.node_lon = ((double)gps->getLongitude()) / 1000000.0;
        the_mesh.savePrefs();
        _task->showAlert("Position saved", 1000);
      } else {
        _task->showAlert("No GPS fix", 1000);
      }
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
  buzzer.startup();
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
    int ev = analog_btn.check();
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

  PositionTrackInfo track_info;
  bool tracking_active = the_mesh.getPositionTrackInfo(track_info) && track_info.active;
  if (tracking_active && _display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;
    if (track_info.req_count != _track_req_seen || track_info.resp_count != _track_resp_seen) {
      _track_req_seen = track_info.req_count;
      _track_resp_seen = track_info.resp_count;
      _next_refresh = 0;
    }
  }

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
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
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
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(DisplayDriver::RED);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
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
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  bool was_off = _display != NULL && !_display->isOn();
  checkDisplayOn(c);
  return was_off ? 0 : KEY_CONTEXT_MENU;
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

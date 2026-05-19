#include "MyMesh.h"
#include <algorithm>
#if defined(ESP32)
#include <esp_system.h>
#include <esp_sleep.h>
#include <esp_attr.h>
#include <driver/rtc_io.h>
#endif
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#endif

/* ------------------------------ Config -------------------------------- */

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

#ifndef ADVERT_NAME
  #define ADVERT_NAME "repeater"
#endif
#ifndef ADVERT_LAT
  #define ADVERT_LAT 0.0
#endif
#ifndef ADVERT_LON
  #define ADVERT_LON 0.0
#endif

#ifndef ADMIN_PASSWORD
  #define ADMIN_PASSWORD "password"
#endif

#ifndef MQTT_OBSERVER_ENABLED
  #define MQTT_OBSERVER_ENABLED 0
#endif
#ifndef MQTT_OBSERVER_TLS
  #define MQTT_OBSERVER_TLS 1
#endif
#ifndef MQTT_OBSERVER_PORT
  #define MQTT_OBSERVER_PORT 8883
#endif
#ifndef MQTT_OBSERVER_HOST
  #define MQTT_OBSERVER_HOST ""
#endif
#ifndef MQTT_OBSERVER_USERNAME
  #define MQTT_OBSERVER_USERNAME ""
#endif
#ifndef MQTT_OBSERVER_PASSWORD
  #define MQTT_OBSERVER_PASSWORD ""
#endif
#ifndef MQTT_OBSERVER_TOPIC
  #define MQTT_OBSERVER_TOPIC "meshcore/observer"
#endif
#ifndef MQTT_OBSERVER_WIFI_SSID
  #define MQTT_OBSERVER_WIFI_SSID ""
#endif
#ifndef MQTT_OBSERVER_WIFI_PASSWORD
  #define MQTT_OBSERVER_WIFI_PASSWORD ""
#endif
#ifndef OBSERVER_WEB_AP_SSID
  #define OBSERVER_WEB_AP_SSID "MBK-GF-WP"
#endif
#ifndef OBSERVER_WEB_AP_PASSWORD
  #define OBSERVER_WEB_AP_PASSWORD "observer2026"
#endif
#ifndef OBSERVER_WEB_VIEW_RETRY_MS
  #define OBSERVER_WEB_VIEW_RETRY_MS 10000UL
#endif
#ifndef OBSERVER_WEB_VIEW_FALLBACK_MS
  #define OBSERVER_WEB_VIEW_FALLBACK_MS 120000UL
#endif

#if defined(ESP32)
  #define OBSERVER_LOCK() portENTER_CRITICAL(&observer_lock)
  #define OBSERVER_UNLOCK() portEXIT_CRITICAL(&observer_lock)
#else
  #define OBSERVER_LOCK()
  #define OBSERVER_UNLOCK()
#endif

#ifndef SERVER_RESPONSE_DELAY
  #define SERVER_RESPONSE_DELAY 300
#endif

#ifndef TXT_ACK_DELAY
  #define TXT_ACK_DELAY 200
#endif

#define FIRMWARE_VER_LEVEL       2

#define SAVEPOINT_INDEX_FILE     "/sp_index.csv"

#define CLOCK_SYNC_MIN_TIME      1735689600UL  // 2025-01-01T00:00:00Z

#if defined(ESP32)
struct ObserverHealthBreadcrumb {
  uint32_t magic;
  uint32_t seq;
  uint32_t uptime_s;
  uint32_t rx_total;
  uint32_t mqtt_total;
  uint32_t free_heap;
  uint32_t min_heap;
  uint8_t screen;
  uint8_t web_state;
  uint8_t web_active;
  uint8_t web_rejects;
  uint8_t web_last;
  uint16_t web_last_age_s;
  uint32_t crc;
};

static RTC_NOINIT_ATTR ObserverHealthBreadcrumb observer_health_breadcrumb;
static const uint32_t OBSERVER_HEALTH_MAGIC = 0x4D425748UL;  // MBWH
static const unsigned long OBSERVER_HEALTH_UPDATE_MS = 10000;

static uint32_t observerHealthCrc(const ObserverHealthBreadcrumb& item) {
  uint32_t crc = 0xA5C35A5CUL;
  crc ^= item.magic;
  crc ^= item.seq;
  crc ^= item.uptime_s;
  crc ^= item.rx_total;
  crc ^= item.mqtt_total;
  crc ^= item.free_heap;
  crc ^= item.min_heap;
  crc ^= item.screen;
  crc ^= item.web_state;
  crc ^= item.web_active;
  crc ^= item.web_rejects;
  crc ^= item.web_last;
  crc ^= item.web_last_age_s;
  return crc;
}

static bool observerHealthValid(const ObserverHealthBreadcrumb& item) {
  return item.magic == OBSERVER_HEALTH_MAGIC && item.crc == observerHealthCrc(item);
}

static const char* observerResetReasonString(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON: return "PowerOn";
    case ESP_RST_EXT: return "External";
    case ESP_RST_SW: return "Software";
    case ESP_RST_PANIC: return "Panic";
    case ESP_RST_INT_WDT: return "IntWDT";
    case ESP_RST_TASK_WDT: return "TaskWDT";
    case ESP_RST_WDT: return "WDT";
    case ESP_RST_DEEPSLEEP: return "Sleep";
    case ESP_RST_BROWNOUT: return "Brownout";
    case ESP_RST_SDIO: return "SDIO";
    default: return "Unknown";
  }
}
#endif
#define CLOCK_SYNC_MAX_TIME      2051222400UL  // 2035-01-01T00:00:00Z

#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
static const char OBSERVER_WEB_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>MBK GF WP</title>
<style>
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif;margin:0;background:#f5f5f2;color:#111}
main{max-width:720px;margin:0 auto;padding:18px}
h1{font-size:24px;margin:8px 0 2px}
h2{font-size:16px;margin:22px 0 8px}
.muted{color:#555;font-size:13px}
.panel{background:#fff;border:1px solid #ddd;border-radius:8px;padding:14px;margin-top:12px}
button,a.btn{display:inline-block;border:1px solid #111;background:#111;color:#fff;border-radius:6px;padding:10px 12px;margin:4px 4px 4px 0;text-decoration:none;font-size:15px}
button.secondary,a.secondary{background:#fff;color:#111}
input{font:inherit;padding:9px;border:1px solid #aaa;border-radius:6px;width:100%;box-sizing:border-box;margin:4px 0 8px}
table{width:100%;border-collapse:collapse;font-size:14px}
td,th{border-bottom:1px solid #e4e4e0;padding:8px 4px;text-align:left}
canvas{display:block;width:100%;height:148px}
.load-row{display:grid;grid-template-columns:minmax(0,1fr) 76px;gap:10px;align-items:start}
.load-side{font-size:12px;line-height:1.25;color:#222}
.load-side b{display:block;font-size:15px;margin:1px 0 7px;text-align:right}
.load-side span{display:block;text-align:right}
.ok{color:#0a7a29}.err{color:#a40000}
</style>
</head>
<body><main>
<h1>WP Field Monitor</h1>
<div class="muted" id="status">loading...</div>
<div class="muted" id="status2"></div>
<div class="muted" id="nodeTime"></div>

<section class="panel">
<h2>Load</h2>
<div class="load-row">
<canvas id="load"></canvas>
<div class="load-side">
<span>Scale</span><b id="loadScale">-</b>
<span>Now</span><b id="loadNow">-</b>
<span>Avg</span><b id="loadAvg">-</b>
</div>
</div>
<div class="muted" id="loadLegend"></div>
</section>

<section class="panel">
<h2>Heatstrip</h2>
<div id="heat"></div>
</section>

<section class="panel">
<h2>Time</h2>
<button id="timeBtn" onclick="setTime()">Set from this device</button>
<div class="muted" id="time"></div>
</section>

<section class="panel">
<h2>Position</h2>
<button id="posBtn" onclick="setPosition()">Use this device position</button>
<input id="lat" placeholder="Latitude">
<input id="lon" placeholder="Longitude">
<button id="posSaveBtn" class="secondary" onclick="savePosition()">Save entered position</button>
<div class="muted" id="pos"></div>
</section>

<section class="panel">
<h2>Savepoints</h2>
<button onclick="createSavepoint()">Create savepoint</button>
<a class="btn secondary" href="/sp.list.csv">Export list CSV</a>
<table><thead><tr><th>ID</th><th>UTC</th><th>RX</th><th>NF</th><th>CSV</th></tr></thead><tbody id="sp"></tbody></table>
</section>

<div class="muted" id="msg"></div>
</main>
<script>
function msg(t,err){document.getElementById('msg').className=err?'err':'ok';document.getElementById('msg').textContent=t}
async function refresh(){
  const r=await fetch('/api/status'); const s=await r.json();
  document.getElementById('status').textContent=s.node+' '+(s.view_mode?'View':'AP')+' '+location.host;
  document.getElementById('status2').textContent='S: '+s.snr.toFixed(1)+' NF: '+s.nf+' Free: '+Math.floor(s.free_heap/1024)+'k Bat: '+s.batt_mv+'mV';
  const nodeUtc=new Date(s.time*1000).toISOString();
  document.getElementById('nodeTime').textContent='Node UTC: '+nodeUtc;
  document.getElementById('timeBtn').disabled=s.view_mode;
  document.getElementById('posBtn').disabled=s.view_mode;
  document.getElementById('posSaveBtn').disabled=s.view_mode;
  document.getElementById('time').textContent='Node UTC: '+nodeUtc;
  document.getElementById('lat').value=s.lat.toFixed(6);
  document.getElementById('lon').value=s.lon.toFixed(6);
  document.getElementById('pos').textContent=s.lat.toFixed(6)+', '+s.lon.toFixed(6);
  document.getElementById('sp').innerHTML=s.savepoints.map(p=>'<tr><td>'+p.id+'</td><td>'+p.time+'</td><td>'+p.rx+'</td><td>'+p.nf+'</td><td><a href="/sp.csv?id='+p.id+'">CSV</a></td></tr>').join('');
}
function drawLoad(m){
  const c=document.getElementById('load'),ctx=c.getContext('2d'),rect=c.getBoundingClientRect(),dpr=window.devicePixelRatio||1;
  c.width=Math.max(240,Math.floor(rect.width*dpr)); c.height=Math.floor(148*dpr); ctx.setTransform(dpr,0,0,dpr,0,0);
  const w=rect.width,h=148,p=18,chartBottom=h-34;
  ctx.clearRect(0,0,w,h); ctx.fillStyle='#fff'; ctx.fillRect(0,0,w,h); ctx.strokeStyle='#111';
  ctx.beginPath(); ctx.moveTo(p,chartBottom); ctx.lineTo(w-p,chartBottom); ctx.moveTo(p,p); ctx.lineTo(p,chartBottom); ctx.stroke();
  const vals=m.airtime_ms.map(v=>v/600), max=Math.max(1,...vals), bw=(w-2*p)/vals.length-4;
  ctx.fillStyle='#111';
  ctx.font='10px -apple-system,BlinkMacSystemFont,"Segoe UI",sans-serif'; ctx.textAlign='center'; ctx.textBaseline='top';
  vals.forEach((v,i)=>{
    const slot=(w-2*p)/vals.length, bh=v/max*(chartBottom-p-8), x=p+i*slot+2, cx=x+bw/2;
    ctx.fillStyle='#111'; ctx.fillRect(x,chartBottom-bh,bw,bh);
    ctx.fillStyle='#555'; ctx.fillText(String(m.rx[i]),cx,chartBottom+6);
  });
  const now=vals.length?vals[vals.length-1]:0, avg=vals.reduce((a,b)=>a+b,0)/Math.max(1,vals.length);
  document.getElementById('loadScale').textContent='0-'+max.toFixed(1)+'%';
  document.getElementById('loadNow').textContent=now.toFixed(1)+'%';
  document.getElementById('loadAvg').textContent=avg.toFixed(1)+'%';
  document.getElementById('loadLegend').textContent='RX packets / min';
}
function drawHeat(m){
  const rows=m.heat.rows, paths=m.heat.paths;
  let html='<table><thead><tr><th>Rep</th>';
  paths.forEach((p,i)=>html+='<th>P'+(i+1)+'</th>'); html+='<th>PC</th></tr></thead><tbody>';
  rows.forEach(r=>{html+='<tr><td>'+r.rep+'</td>'; r.pos.forEach(v=>{const shade=v==1?'#111':v==2?'#666':v==3?'#aaa':'#eee'; html+='<td style="background:'+shade+'">&nbsp;</td>'}); html+='<td>'+r.pc+'</td></tr>'});
  html+='</tbody></table>'; document.getElementById('heat').innerHTML=html;
}
async function refreshMonitor(){
  const r=await fetch('/api/monitor'); const m=await r.json();
  drawLoad(m); drawHeat(m);
}
async function post(url){const r=await fetch(url,{method:'POST'}); const t=await r.text(); msg(t,!r.ok); await refresh()}
function setTime(){post('/api/time?epoch='+Math.floor(Date.now()/1000))}
function createSavepoint(){post('/api/savepoint')}
function savePosition(){post('/api/position?lat='+encodeURIComponent(lat.value)+'&lon='+encodeURIComponent(lon.value))}
function setPosition(){
  if(!navigator.geolocation){msg('Geolocation not available',true);return}
  navigator.geolocation.getCurrentPosition(p=>{
    lat.value=p.coords.latitude.toFixed(6); lon.value=p.coords.longitude.toFixed(6); savePosition();
  },e=>msg(e.message,true),{enableHighAccuracy:true,timeout:15000,maximumAge:0});
}
let refreshBusy=false;
async function refreshAll(){
  if(refreshBusy) return;
  refreshBusy=true;
  try{await refresh();}
  catch(e){msg(e.message,true)}
  refreshBusy=false;
}
async function monitorTick(){try{await refreshMonitor();}catch(e){msg(e.message,true)}}
refreshAll(); monitorTick(); setInterval(refreshAll,15000); setInterval(monitorTick,60000);
</script></body></html>
)HTML";

static void handleObserverWebPosition(MyMesh* mesh, AsyncWebServerRequest* request) {
  if (!mesh || !request->hasParam("lat") || !request->hasParam("lon")) {
    request->send(400, "text/plain", "missing position");
    return;
  }
  double lat = atof(request->getParam("lat")->value().c_str());
  double lon = atof(request->getParam("lon")->value().c_str());
  if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) {
    request->send(400, "text/plain", "invalid position");
    return;
  }
  mesh->setObserverPosition(lat, lon);
  request->send(200, "text/plain", "position saved");
}
#endif

#define REQ_TYPE_GET_STATUS         0x01 // same as _GET_STATS
#define REQ_TYPE_KEEP_ALIVE         0x02
#define REQ_TYPE_GET_TELEMETRY_DATA 0x03
#define REQ_TYPE_GET_ACCESS_LIST    0x05
#define REQ_TYPE_GET_NEIGHBOURS     0x06
#define REQ_TYPE_GET_OWNER_INFO     0x07     // FIRMWARE_VER_LEVEL >= 2

#define RESP_SERVER_LOGIN_OK        0 // response to ANON_REQ

#define ANON_REQ_TYPE_REGIONS      0x01
#define ANON_REQ_TYPE_OWNER        0x02
#define ANON_REQ_TYPE_BASIC        0x03   // just remote clock

#define CLI_REPLY_DELAY_MILLIS      600

#define LAZY_CONTACTS_WRITE_DELAY    5000

void MyMesh::putNeighbour(const mesh::Identity &id, uint32_t timestamp, float snr) {
#if MAX_NEIGHBOURS // check if neighbours enabled
  // find existing neighbour, else use least recently updated
  uint32_t oldest_timestamp = 0xFFFFFFFF;
  NeighbourInfo *neighbour = &neighbours[0];
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    // if neighbour already known, we should update it
    if (id.matches(neighbours[i].id)) {
      neighbour = &neighbours[i];
      break;
    }

    // otherwise we should update the least recently updated neighbour
    if (neighbours[i].heard_timestamp < oldest_timestamp) {
      neighbour = &neighbours[i];
      oldest_timestamp = neighbour->heard_timestamp;
    }
  }

  // update neighbour info
  neighbour->id = id;
  neighbour->advert_timestamp = timestamp;
  neighbour->heard_timestamp = getRTCClock()->getCurrentTime();
  neighbour->snr = (int8_t)(snr * 4);
#endif
}

uint8_t MyMesh::handleLoginReq(const mesh::Identity& sender, const uint8_t* secret, uint32_t sender_timestamp, const uint8_t* data, bool is_flood) {
  ClientInfo* client = NULL;
  if (data[0] == 0) {   // blank password, just check if sender is in ACL
    client = acl.getClient(sender.pub_key, PUB_KEY_SIZE);
    if (client == NULL) {
    #if MESH_DEBUG
      MESH_DEBUG_PRINTLN("Login, sender not in ACL");
    #endif
    }
  }
  if (client == NULL) {
    uint8_t perms;
    if (strcmp((char *)data, _prefs.password) == 0) { // check for valid admin password
      perms = PERM_ACL_ADMIN;
    } else if (strcmp((char *)data, _prefs.guest_password) == 0) { // check guest password
      perms = PERM_ACL_GUEST;
    } else {
#if MESH_DEBUG
      MESH_DEBUG_PRINTLN("Invalid password: %s", data);
#endif
      return 0;
    }

    client = acl.putClient(sender, 0);  // add to contacts (if not already known)
    if (sender_timestamp <= client->last_timestamp) {
      MESH_DEBUG_PRINTLN("Possible login replay attack!");
      return 0;  // FATAL: client table is full -OR- replay attack
    }

    MESH_DEBUG_PRINTLN("Login success!");
    client->last_timestamp = sender_timestamp;
    client->last_activity = getRTCClock()->getCurrentTime();
    client->permissions &= ~0x03;
    client->permissions |= perms;
    memcpy(client->shared_secret, secret, PUB_KEY_SIZE);

    if (perms != PERM_ACL_GUEST) {   // keep number of FS writes to a minimum
      dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);
    }
  }

  if (is_flood) {
    client->out_path_len = OUT_PATH_UNKNOWN;  // need to rediscover out_path
  }

  uint32_t now = getRTCClock()->getCurrentTimeUnique();
  memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  reply_data[4] = RESP_SERVER_LOGIN_OK;
  reply_data[5] = 0;  // Legacy: was recommended keep-alive interval (secs / 16)
  reply_data[6] = client->isAdmin() ? 1 : 0;
  reply_data[7] = client->permissions;
  getRNG()->random(&reply_data[8], 4);   // random blob to help packet-hash uniqueness
  reply_data[12] = FIRMWARE_VER_LEVEL;  // New field

  return 13;  // reply length
}

uint8_t MyMesh::handleAnonRegionsReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data & 63;
    reply_path_hash_size = (*data >> 6) + 1;
    data++;

    memcpy(reply_path, data, ((uint8_t)reply_path_len) * reply_path_hash_size);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)

    return 8 + region_map.exportNamesTo((char *) &reply_data[8], sizeof(reply_data) - 12, REGION_DENY_FLOOD);   // reply length
  }
  return 0;
}

uint8_t MyMesh::handleAnonOwnerReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data & 63;
    reply_path_hash_size = (*data >> 6) + 1;
    data++;

    memcpy(reply_path, data, ((uint8_t)reply_path_len) * reply_path_hash_size);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)
    sprintf((char *) &reply_data[8], "%s\n%s", _prefs.node_name, _prefs.owner_info);

    return 8 + strlen((char *) &reply_data[8]);   // reply length
  }
  return 0;
}

uint8_t MyMesh::handleAnonClockReq(const mesh::Identity& sender, uint32_t sender_timestamp, const uint8_t* data) {
  if (anon_limiter.allow(rtc_clock.getCurrentTime())) {
    // request data has: {reply-path-len}{reply-path}
    reply_path_len = *data & 63;
    reply_path_hash_size = (*data >> 6) + 1;
    data++;

    memcpy(reply_path, data, ((uint8_t)reply_path_len) * reply_path_hash_size);
    // data += (uint8_t)reply_path_len * reply_path_hash_size;

    memcpy(reply_data, &sender_timestamp, 4);   // prefix with sender_timestamp, like a tag
    uint32_t now = getRTCClock()->getCurrentTime();
    memcpy(&reply_data[4], &now, 4);     // include our clock (for easy clock sync, and packet hash uniqueness)
    reply_data[8] = 0;  // features
#ifdef WITH_RS232_BRIDGE
    reply_data[8] |= 0x01;  // is bridge, type UART
#elif WITH_ESPNOW_BRIDGE
    reply_data[8] |= 0x03;  // is bridge, type ESP-NOW
#endif
    if (_prefs.disable_fwd) {   // is this repeater currently disabled
      reply_data[8] |= 0x80;  // is disabled
    }
    // TODO:  add some kind of moving-window utilisation metric, so can query 'how busy' is this repeater
    return 9;   // reply length
  }
  return 0;
}

int MyMesh::handleRequest(ClientInfo *sender, uint32_t sender_timestamp, uint8_t *payload, size_t payload_len) {
  // uint32_t now = getRTCClock()->getCurrentTimeUnique();
  // memcpy(reply_data, &now, 4);   // response packets always prefixed with timestamp
  memcpy(reply_data, &sender_timestamp, 4); // reflect sender_timestamp back in response packet (kind of like a 'tag')

  if (payload[0] == REQ_TYPE_GET_STATUS) {  // guests can also access this now
    RepeaterStats stats;
    stats.batt_milli_volts = board.getBattMilliVolts();
    stats.curr_tx_queue_len = _mgr->getOutboundTotal();
    stats.noise_floor = (int16_t)_radio->getNoiseFloor();
    stats.last_rssi = (int16_t)radio_driver.getLastRSSI();
    stats.n_packets_recv = radio_driver.getPacketsRecv();
    stats.n_packets_sent = radio_driver.getPacketsSent();
    stats.total_air_time_secs = getTotalAirTime() / 1000;
    stats.total_up_time_secs = uptime_millis / 1000;
    stats.n_sent_flood = getNumSentFlood();
    stats.n_sent_direct = getNumSentDirect();
    stats.n_recv_flood = getNumRecvFlood();
    stats.n_recv_direct = getNumRecvDirect();
    stats.err_events = _err_flags;
    stats.last_snr = (int16_t)(radio_driver.getLastSNR() * 4);
    stats.n_direct_dups = ((SimpleMeshTables *)getTables())->getNumDirectDups();
    stats.n_flood_dups = ((SimpleMeshTables *)getTables())->getNumFloodDups();
    stats.total_rx_air_time_secs = getReceiveAirTime() / 1000;
    stats.n_recv_errors = radio_driver.getPacketsRecvErrors();
    memcpy(&reply_data[4], &stats, sizeof(stats));

    return 4 + sizeof(stats); //  reply_len
  }
  if (payload[0] == REQ_TYPE_GET_TELEMETRY_DATA) {
    uint8_t perm_mask = ~(payload[1]); // NEW: first reserved byte (of 4), is now inverse mask to apply to permissions

    telemetry.reset();
    telemetry.addVoltage(TELEM_CHANNEL_SELF, (float)board.getBattMilliVolts() / 1000.0f);

    // query other sensors -- target specific
    if ((sender->permissions & PERM_ACL_ROLE_MASK) == PERM_ACL_GUEST) {
      perm_mask = 0x00;  // just base telemetry allowed
    }
    sensors.querySensors(perm_mask, telemetry);

	// This default temperature will be overridden by external sensors (if any)
    float temperature = board.getMCUTemperature();
    if(!isnan(temperature)) { // Supported boards with built-in temperature sensor. ESP32-C3 may return NAN
      telemetry.addTemperature(TELEM_CHANNEL_SELF, temperature); // Built-in MCU Temperature
    }

    uint8_t tlen = telemetry.getSize();
    memcpy(&reply_data[4], telemetry.getBuffer(), tlen);
    return 4 + tlen; // reply_len
  }
  if (payload[0] == REQ_TYPE_GET_ACCESS_LIST && sender->isAdmin()) {
    uint8_t res1 = payload[1];   // reserved for future  (extra query params)
    uint8_t res2 = payload[2];
    if (res1 == 0 && res2 == 0) {
      uint8_t ofs = 4;
      for (int i = 0; i < acl.getNumClients() && ofs + 7 <= sizeof(reply_data) - 4; i++) {
        auto c = acl.getClientByIdx(i);
        if (c->permissions == 0) continue;  // skip deleted entries
        memcpy(&reply_data[ofs], c->id.pub_key, 6); ofs += 6;  // just 6-byte pub_key prefix
        reply_data[ofs++] = c->permissions;
      }
      return ofs;
    }
  }
  if (payload[0] == REQ_TYPE_GET_NEIGHBOURS) {
    uint8_t request_version = payload[1];
    if (request_version == 0) {

      // reply data offset (after response sender_timestamp/tag)
      int reply_offset = 4;

      // get request params
      uint8_t count = payload[2]; // how many neighbours to fetch (0-255)
      uint16_t offset;
      memcpy(&offset, &payload[3], 2); // offset from start of neighbours list (0-65535)
      uint8_t order_by = payload[5]; // how to order neighbours. 0=newest_to_oldest, 1=oldest_to_newest, 2=strongest_to_weakest, 3=weakest_to_strongest
      uint8_t pubkey_prefix_length = payload[6]; // how many bytes of neighbour pub key we want
      // we also send a 4 byte random blob in payload[7...10] to help packet uniqueness

      MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS count=%d, offset=%d, order_by=%d, pubkey_prefix_length=%d", count, offset, order_by, pubkey_prefix_length);

      // clamp pub key prefix length to max pub key length
      if(pubkey_prefix_length > PUB_KEY_SIZE){
        pubkey_prefix_length = PUB_KEY_SIZE;
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS invalid pubkey_prefix_length=%d clamping to %d", pubkey_prefix_length, PUB_KEY_SIZE);
      }

      // create copy of neighbours list, skipping empty entries so we can sort it separately from main list
      int16_t neighbours_count = 0;
#if MAX_NEIGHBOURS
      NeighbourInfo* sorted_neighbours[MAX_NEIGHBOURS];
      for (int i = 0; i < MAX_NEIGHBOURS; i++) {
        auto neighbour = &neighbours[i];
        if (neighbour->heard_timestamp > 0) {
          sorted_neighbours[neighbours_count] = neighbour;
          neighbours_count++;
        }
      }

      // sort neighbours based on order
      if (order_by == 0) {
        // sort by newest to oldest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting newest to oldest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->heard_timestamp > b->heard_timestamp; // desc
        });
      } else if (order_by == 1) {
        // sort by oldest to newest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting oldest to newest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->heard_timestamp < b->heard_timestamp; // asc
        });
      } else if (order_by == 2) {
        // sort by strongest to weakest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting strongest to weakest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->snr > b->snr; // desc
        });
      } else if (order_by == 3) {
        // sort by weakest to strongest
        MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS sorting weakest to strongest");
        std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
          return a->snr < b->snr; // asc
        });
      }
#endif

      // build results buffer
      int results_count = 0;
      int results_offset = 0;
      uint8_t results_buffer[130];
      for(int index = 0; index < count && index + offset < neighbours_count; index++){
        
        // stop if we can't fit another entry in results
        int entry_size = pubkey_prefix_length + 4 + 1;
        if(results_offset + entry_size > sizeof(results_buffer)){
          MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS no more entries can fit in results buffer");
          break;
        }

#if MAX_NEIGHBOURS
        // add next neighbour to results
        auto neighbour = sorted_neighbours[index + offset];
        uint32_t heard_seconds_ago = getRTCClock()->getCurrentTime() - neighbour->heard_timestamp;
        memcpy(&results_buffer[results_offset], neighbour->id.pub_key, pubkey_prefix_length); results_offset += pubkey_prefix_length;
        memcpy(&results_buffer[results_offset], &heard_seconds_ago, 4); results_offset += 4;
        memcpy(&results_buffer[results_offset], &neighbour->snr, 1); results_offset += 1;
        results_count++;
#endif

      }

      // build reply
      MESH_DEBUG_PRINTLN("REQ_TYPE_GET_NEIGHBOURS neighbours_count=%d results_count=%d", neighbours_count, results_count);
      memcpy(&reply_data[reply_offset], &neighbours_count, 2); reply_offset += 2;
      memcpy(&reply_data[reply_offset], &results_count, 2); reply_offset += 2;
      memcpy(&reply_data[reply_offset], &results_buffer, results_offset); reply_offset += results_offset;

      return reply_offset;
    }
  } else if (payload[0] == REQ_TYPE_GET_OWNER_INFO) {
    sprintf((char *) &reply_data[4], "%s\n%s\n%s", FIRMWARE_VERSION, _prefs.node_name, _prefs.owner_info);
    return 4 + strlen((char *) &reply_data[4]);
  }
  return 0; // unknown command
}

mesh::Packet *MyMesh::createSelfAdvert() {
  uint8_t app_data[MAX_ADVERT_DATA_SIZE];
  uint8_t app_data_len = _cli.buildAdvertData(ADV_TYPE_REPEATER, app_data);

  return createAdvert(self_id, app_data, app_data_len);
}

File MyMesh::openAppend(const char *fname) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return _fs->open(fname, FILE_O_WRITE);
#elif defined(RP2040_PLATFORM)
  return _fs->open(fname, "a");
#else
  return _fs->open(fname, "a", true);
#endif
}

static uint8_t max_loop_minimal[] =  { 0, /* 1-byte */  4, /* 2-byte */  2, /* 3-byte */  1 };
static uint8_t max_loop_moderate[] = { 0, /* 1-byte */  2, /* 2-byte */  1, /* 3-byte */  1 };
static uint8_t max_loop_strict[] =   { 0, /* 1-byte */  1, /* 2-byte */  1, /* 3-byte */  1 };

bool MyMesh::isLooped(const mesh::Packet* packet, const uint8_t max_counters[]) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t n = 0;
  const uint8_t* path = packet->path;
  while (hash_count > 0) {      // count how many times this node is already in the path
    if (self_id.isHashMatch(path, hash_size)) n++;
    hash_count--;
    path += hash_size;
  }
  return n >= max_counters[hash_size];
}

void MyMesh::sendFloodReply(mesh::Packet* packet, unsigned long delay_millis, uint8_t path_hash_size) {
  if (recv_pkt_region && !recv_pkt_region->isWildcard()) {  // if _request_ packet scope is known, send reply with same scope
    TransportKey scope;
    if (region_map.getTransportKeysFor(*recv_pkt_region, &scope, 1) > 0) {
      sendFloodScoped(scope, packet, delay_millis, path_hash_size);
    } else {
      sendFlood(packet, delay_millis, path_hash_size);  // send un-scoped
    }
  } else {
    sendFlood(packet, delay_millis, path_hash_size);  // send un-scoped
  }
}

bool MyMesh::allowPacketForward(const mesh::Packet *packet) {
  if (_prefs.disable_fwd) return false;
  if (packet->isRouteFlood() && packet->getPathHashCount() >= _prefs.flood_max) return false;
  if (packet->isRouteFlood() && recv_pkt_region == NULL) {
    MESH_DEBUG_PRINTLN("allowPacketForward: unknown transport code, or wildcard not allowed for FLOOD packet");
    return false;
  }
  if (packet->isRouteFlood() && _prefs.loop_detect != LOOP_DETECT_OFF) {
    const uint8_t* maximums;
    if (_prefs.loop_detect == LOOP_DETECT_MINIMAL) {
      maximums = max_loop_minimal;
    } else if (_prefs.loop_detect == LOOP_DETECT_MODERATE) {
      maximums = max_loop_moderate;
    } else {
      maximums = max_loop_strict;
    }
    if (isLooped(packet, maximums)) {
      MESH_DEBUG_PRINTLN("allowPacketForward: FLOOD packet loop detected!");
      return false;
    }
  }
  return true;
}

const char *MyMesh::getLogDateTime() {
  static char tmp[32];
  uint32_t now = getRTCClock()->getCurrentTime();
  DateTime dt = DateTime(now);
  sprintf(tmp, "%02d:%02d:%02d - %d/%d/%d U", dt.hour(), dt.minute(), dt.second(), dt.day(), dt.month(),
          dt.year());
  return tmp;
}

void MyMesh::logRxRaw(float snr, float rssi, const uint8_t raw[], int len) {
#if MESH_PACKET_LOGGING
  Serial.print(getLogDateTime());
  Serial.print(" RAW: ");
  mesh::Utils::printHex(Serial, raw, len);
  Serial.println();
#endif
}

void MyMesh::logRx(mesh::Packet *pkt, int len, float score) {
  OBSERVER_LOCK();
  observer_rx_packets++;
  OBSERVER_UNLOCK();
  int snr_x4 = (int)(_radio->getLastSNR() * 4);
  if (snr_x4 > 127) snr_x4 = 127;
  if (snr_x4 < -128) snr_x4 = -128;
  rememberObserverPath(pkt);
  rememberObserverLastHop(pkt, (int8_t)snr_x4);
#ifdef WITH_BRIDGE
  if (_prefs.bridge_pkt_src == 1) {
    bridge.sendPacket(pkt);
  }
#endif
#ifdef WITH_MQTT_OBSERVER
  if (mqtt_observer.sendPacket(pkt, true, (int)_radio->getLastRSSI(), snr_x4)) {
    OBSERVER_LOCK();
    observer_mqtt_published++;
    OBSERVER_UNLOCK();
  }
#endif

  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": RX, len=%d (type=%d, route=%s, payload_len=%d) SNR=%d RSSI=%d score=%d", len,
               pkt->getPayloadType(), pkt->isRouteDirect() ? "D" : "F", pkt->payload_len,
               (int)_radio->getLastSNR(), (int)_radio->getLastRSSI(), (int)(score * 1000));

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}

void MyMesh::observeClockSyncSample(const mesh::Identity& id, uint32_t timestamp) {
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  if (observer_clock_sync_done) return;
  if (timestamp < CLOCK_SYNC_MIN_TIME || timestamp > CLOCK_SYNC_MAX_TIME) return;

  ObserverClockSyncSample& slot = observer_clock_sync_samples[observer_clock_sync_next];
  slot.used = true;
  slot.timestamp = timestamp;
  memcpy(slot.pub_key, id.pub_key, PUB_KEY_SIZE);
  observer_clock_sync_next = (observer_clock_sync_next + 1) % OBSERVER_CLOCK_SYNC_SAMPLES;
  if (observer_clock_sync_count < OBSERVER_CLOCK_SYNC_SAMPLES) observer_clock_sync_count++;

  if (observer_clock_sync_count < OBSERVER_CLOCK_SYNC_REQUIRED) return;

  uint8_t distinct = 0;
  uint32_t values[OBSERVER_CLOCK_SYNC_SAMPLES];
  uint8_t value_count = 0;

  for (uint8_t i = 0; i < OBSERVER_CLOCK_SYNC_SAMPLES; i++) {
    if (!observer_clock_sync_samples[i].used) continue;
    values[value_count++] = observer_clock_sync_samples[i].timestamp;

    bool seen = false;
    for (uint8_t j = 0; j < i; j++) {
      if (!observer_clock_sync_samples[j].used) continue;
      if (memcmp(observer_clock_sync_samples[i].pub_key, observer_clock_sync_samples[j].pub_key, PUB_KEY_SIZE) == 0) {
        seen = true;
        break;
      }
    }
    if (!seen) distinct++;
  }

  if (value_count < OBSERVER_CLOCK_SYNC_REQUIRED || distinct < OBSERVER_CLOCK_SYNC_DISTINCT) return;

  std::sort(values, values + value_count);
  uint32_t candidate = values[value_count / 2] + 1;
  uint32_t current = getRTCClock()->getCurrentTime();
  if (candidate > current + 60) {
    getRTCClock()->setCurrentTime(candidate);
    observer_clock_synced_at = candidate;
    observer_clock_synced = true;
  }
  observer_clock_sync_done = true;
#else
  (void)id;
  (void)timestamp;
#endif
}

void MyMesh::logTx(mesh::Packet *pkt, int len) {
#ifdef WITH_BRIDGE
  if (_prefs.bridge_pkt_src == 0) {
    bridge.sendPacket(pkt);
  }
#endif
#ifdef WITH_MQTT_OBSERVER
  mqtt_observer.sendPacket(pkt, false, 0, (int)(pkt->getSNR() * 4));
#endif

  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX, len=%d (type=%d, route=%s, payload_len=%d)", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);

      if (pkt->getPayloadType() == PAYLOAD_TYPE_PATH || pkt->getPayloadType() == PAYLOAD_TYPE_REQ ||
          pkt->getPayloadType() == PAYLOAD_TYPE_RESPONSE || pkt->getPayloadType() == PAYLOAD_TYPE_TXT_MSG) {
        f.printf(" [%02X -> %02X]\n", (uint32_t)pkt->payload[1], (uint32_t)pkt->payload[0]);
      } else {
        f.printf("\n");
      }
      f.close();
    }
  }
}

void MyMesh::logTxFail(mesh::Packet *pkt, int len) {
  if (_logging) {
    File f = openAppend(PACKET_LOG_FILE);
    if (f) {
      f.print(getLogDateTime());
      f.printf(": TX FAIL!, len=%d (type=%d, route=%s, payload_len=%d)\n", len, pkt->getPayloadType(),
               pkt->isRouteDirect() ? "D" : "F", pkt->payload_len);
      f.close();
    }
  }
}

int MyMesh::calcRxDelay(float score, uint32_t air_time) const {
  if (_prefs.rx_delay_base <= 0.0f) return 0;
  return (int)((pow(_prefs.rx_delay_base, 0.85f - score) - 1.0) * air_time);
}

uint32_t MyMesh::getRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}
uint32_t MyMesh::getDirectRetransmitDelay(const mesh::Packet *packet) {
  uint32_t t = (_radio->getEstAirtimeFor(packet->getPathByteLen() + packet->payload_len + 2) * _prefs.direct_tx_delay_factor);
  return getRNG()->nextInt(0, 5*t + 1);
}

bool MyMesh::filterRecvFloodPacket(mesh::Packet* pkt) {
  // just try to determine region for packet (apply later in allowPacketForward())
  if (pkt->getRouteType() == ROUTE_TYPE_TRANSPORT_FLOOD) {
    recv_pkt_region = region_map.findMatch(pkt, REGION_DENY_FLOOD);
  } else if (pkt->getRouteType() == ROUTE_TYPE_FLOOD) {
    if (region_map.getWildcard().flags & REGION_DENY_FLOOD) {
      recv_pkt_region = NULL;
    } else {
      recv_pkt_region =  &region_map.getWildcard();
    }
  } else {
    recv_pkt_region = NULL;
  }
  // do normal processing
  return false;
}

void MyMesh::onAnonDataRecv(mesh::Packet *packet, const uint8_t *secret, const mesh::Identity &sender,
                            uint8_t *data, size_t len) {
  if (packet->getPayloadType() == PAYLOAD_TYPE_ANON_REQ) { // received an initial request by a possible admin
                                                           // client (unknown at this stage)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    data[len] = 0;  // ensure null terminator
    uint8_t reply_len;

    reply_path_len = -1;
    if (data[4] == 0 || data[4] >= ' ') {   // is password, ie. a login request
      reply_len = handleLoginReq(sender, secret, timestamp, &data[4], packet->isRouteFlood());
    } else if (data[4] == ANON_REQ_TYPE_REGIONS && packet->isRouteDirect()) {
      reply_len = handleAnonRegionsReq(sender, timestamp, &data[5]);
    } else if (data[4] == ANON_REQ_TYPE_OWNER && packet->isRouteDirect()) {
      reply_len = handleAnonOwnerReq(sender, timestamp, &data[5]);
    } else if (data[4] == ANON_REQ_TYPE_BASIC && packet->isRouteDirect()) {
      reply_len = handleAnonClockReq(sender, timestamp, &data[5]);
    } else {
      reply_len = 0;  // unknown/invalid request type
    }

    if (reply_len == 0) return;   // invalid request

    if (packet->isRouteFlood()) {
      // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
      mesh::Packet* path = createPathReturn(sender, secret, packet->path, packet->path_len,
                                            PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
      if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
    } else if (reply_path_len < 0) {
      mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret, reply_data, reply_len);
      if (reply) sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
    } else {
      mesh::Packet* reply = createDatagram(PAYLOAD_TYPE_RESPONSE, sender, secret, reply_data, reply_len);
      uint8_t path_len = ((reply_path_hash_size - 1) << 6) | (reply_path_len & 63);
      if (reply) sendDirect(reply, reply_path,  path_len, SERVER_RESPONSE_DELAY);
    }
  }
}

int MyMesh::searchPeersByHash(const uint8_t *hash) {
  int n = 0;
  for (int i = 0; i < acl.getNumClients(); i++) {
    if (acl.getClientByIdx(i)->id.isHashMatch(hash)) {
      matching_peer_indexes[n++] = i; // store the INDEXES of matching contacts (for subsequent 'peer' methods)
    }
  }
  return n;
}

void MyMesh::getPeerSharedSecret(uint8_t *dest_secret, int peer_idx) {
  int i = matching_peer_indexes[peer_idx];
  if (i >= 0 && i < acl.getNumClients()) {
    // lookup pre-calculated shared_secret
    memcpy(dest_secret, acl.getClientByIdx(i)->shared_secret, PUB_KEY_SIZE);
  } else {
    MESH_DEBUG_PRINTLN("getPeerSharedSecret: Invalid peer idx: %d", i);
  }
}

static bool isShare(const mesh::Packet *packet) {
  if (packet->hasTransportCodes()) {
    return packet->transport_codes[0] == 0 && packet->transport_codes[1] == 0;  // codes { 0, 0 } means 'send to nowhere'
  }
  return false;
}

void MyMesh::onAdvertRecv(mesh::Packet *packet, const mesh::Identity &id, uint32_t timestamp,
                          const uint8_t *app_data, size_t app_data_len) {
  mesh::Mesh::onAdvertRecv(packet, id, timestamp, app_data, app_data_len); // chain to super impl
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  observeClockSyncSample(id, timestamp);
#endif

  // if this a zero hop advert (and not via 'Share'), add it to neighbours
  if (packet->path_len == 0 && !isShare(packet)) {
    AdvertDataParser parser(app_data, app_data_len);
    if (parser.isValid() && parser.getType() == ADV_TYPE_REPEATER) { // just keep neigbouring Repeaters
      putNeighbour(id, timestamp, packet->getSNR());
    }
  }
}

void MyMesh::onPeerDataRecv(mesh::Packet *packet, uint8_t type, int sender_idx, const uint8_t *secret,
                            uint8_t *data, size_t len) {
  int i = matching_peer_indexes[sender_idx];
  if (i < 0 || i >= acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
    MESH_DEBUG_PRINTLN("onPeerDataRecv: invalid peer idx: %d", i);
    return;
  }
  ClientInfo* client = acl.getClientByIdx(i);

  if (type == PAYLOAD_TYPE_REQ) { // request (from a Known admin client!)
    uint32_t timestamp;
    memcpy(&timestamp, data, 4);

    if (timestamp > client->last_timestamp) { // prevent replay attacks
      int reply_len = handleRequest(client, timestamp, &data[4], len - 4);
      if (reply_len == 0) return; // invalid command

      client->last_timestamp = timestamp;
      client->last_activity = getRTCClock()->getCurrentTime();

      if (packet->isRouteFlood()) {
        // let this sender know path TO here, so they can use sendDirect(), and ALSO encode the response
        mesh::Packet *path = createPathReturn(client->id, secret, packet->path, packet->path_len,
                                              PAYLOAD_TYPE_RESPONSE, reply_data, reply_len);
        if (path) sendFloodReply(path, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
      } else {
        mesh::Packet *reply =
            createDatagram(PAYLOAD_TYPE_RESPONSE, client->id, secret, reply_data, reply_len);
        if (reply) {
          if (client->out_path_len != OUT_PATH_UNKNOWN) { // we have an out_path, so send DIRECT
            sendDirect(reply, client->out_path, client->out_path_len, SERVER_RESPONSE_DELAY);
          } else {
            sendFloodReply(reply, SERVER_RESPONSE_DELAY, packet->getPathHashSize());
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  } else if (type == PAYLOAD_TYPE_TXT_MSG && len > 5 && client->isAdmin()) { // a CLI command
    uint32_t sender_timestamp;
    memcpy(&sender_timestamp, data, 4); // timestamp (by sender's RTC clock - which could be wrong)
    uint8_t flags = (data[4] >> 2);        // message attempt number, and other flags

    if (!(flags == TXT_TYPE_PLAIN || flags == TXT_TYPE_CLI_DATA)) {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: unsupported text type received: flags=%02x", (uint32_t)flags);
    } else if (sender_timestamp >= client->last_timestamp) { // prevent replay attacks
      bool is_retry = (sender_timestamp == client->last_timestamp);
      client->last_timestamp = sender_timestamp;
      client->last_activity = getRTCClock()->getCurrentTime();

      // len can be > original length, but 'text' will be padded with zeroes
      data[len] = 0; // need to make a C string again, with null terminator

      if (flags == TXT_TYPE_PLAIN) { // for legacy CLI, send Acks
        uint32_t ack_hash; // calc truncated hash of the message timestamp + text + sender pub_key, to prove
                           // to sender that we got it
        mesh::Utils::sha256((uint8_t *)&ack_hash, 4, data, 5 + strlen((char *)&data[5]), client->id.pub_key,
                            PUB_KEY_SIZE);

        mesh::Packet *ack = createAck(ack_hash);
        if (ack) {
          if (client->out_path_len == OUT_PATH_UNKNOWN) {
            sendFloodReply(ack, TXT_ACK_DELAY, packet->getPathHashSize());
          } else {
            sendDirect(ack, client->out_path, client->out_path_len, TXT_ACK_DELAY);
          }
        }
      }

      uint8_t temp[166];
      char *command = (char *)&data[5];
      char *reply = (char *)&temp[5];
      if (is_retry) {
        *reply = 0;
      } else {
        handleCommand(sender_timestamp, command, reply);
      }
      int text_len = strlen(reply);
      if (text_len > 0) {
        uint32_t timestamp = getRTCClock()->getCurrentTimeUnique();
        if (timestamp == sender_timestamp) {
          // WORKAROUND: the two timestamps need to be different, in the CLI view
          timestamp++;
        }
        memcpy(temp, &timestamp, 4);        // mostly an extra blob to help make packet_hash unique
        temp[4] = (TXT_TYPE_CLI_DATA << 2); // NOTE: legacy was: TXT_TYPE_PLAIN

        auto reply = createDatagram(PAYLOAD_TYPE_TXT_MSG, client->id, secret, temp, 5 + text_len);
        if (reply) {
          if (client->out_path_len == OUT_PATH_UNKNOWN) {
            sendFloodReply(reply, CLI_REPLY_DELAY_MILLIS, packet->getPathHashSize());
          } else {
            sendDirect(reply, client->out_path, client->out_path_len, CLI_REPLY_DELAY_MILLIS);
          }
        }
      }
    } else {
      MESH_DEBUG_PRINTLN("onPeerDataRecv: possible replay attack detected");
    }
  }
}

bool MyMesh::onPeerPathRecv(mesh::Packet *packet, int sender_idx, const uint8_t *secret, uint8_t *path,
                            uint8_t path_len, uint8_t extra_type, uint8_t *extra, uint8_t extra_len) {
  // TODO: prevent replay attacks
  int i = matching_peer_indexes[sender_idx];

  if (i >= 0 && i < acl.getNumClients()) { // get from our known_clients table (sender SHOULD already be known in this context)
    MESH_DEBUG_PRINTLN("PATH to client, path_len=%d", (uint32_t)path_len);
    auto client = acl.getClientByIdx(i);

    // store a copy of path, for sendDirect()
    client->out_path_len = mesh::Packet::copyPath(client->out_path, path, path_len);
    client->last_activity = getRTCClock()->getCurrentTime();
  } else {
    MESH_DEBUG_PRINTLN("onPeerPathRecv: invalid peer idx: %d", i);
  }

  // NOTE: no reciprocal path send!!
  return false;
}

#define CTL_TYPE_NODE_DISCOVER_REQ   0x80
#define CTL_TYPE_NODE_DISCOVER_RESP  0x90

void MyMesh::onControlDataRecv(mesh::Packet* packet) {
  uint8_t type = packet->payload[0] & 0xF0;    // just test upper 4 bits
  if (type == CTL_TYPE_NODE_DISCOVER_REQ && packet->payload_len >= 6
      && !_prefs.disable_fwd && discover_limiter.allow(rtc_clock.getCurrentTime())
  ) {
    int i = 1;
    uint8_t  filter = packet->payload[i++];
    uint32_t tag;
    memcpy(&tag, &packet->payload[i], 4); i += 4;
    uint32_t since;
    if (packet->payload_len >= i+4) {   // optional since field
      memcpy(&since, &packet->payload[i], 4); i += 4;
    } else {
      since = 0;
    }

    if ((filter & (1 << ADV_TYPE_REPEATER)) != 0 && _prefs.discovery_mod_timestamp >= since) {
      bool prefix_only = packet->payload[0] & 1;
      uint8_t data[6 + PUB_KEY_SIZE];
      data[0] = CTL_TYPE_NODE_DISCOVER_RESP | ADV_TYPE_REPEATER;   // low 4-bits for node type
      data[1] = packet->_snr;   // let sender know the inbound SNR ( x 4)
      memcpy(&data[2], &tag, 4);     // include tag from request, for client to match to
      memcpy(&data[6], self_id.pub_key, PUB_KEY_SIZE);
      auto resp = createControlData(data, prefix_only ? 6 + 8 : 6 + PUB_KEY_SIZE);
      if (resp) {
        sendZeroHop(resp, getRetransmitDelay(resp)*4);  // apply random delay (widened x4), as multiple nodes can respond to this
      }
    }
  } else if (type == CTL_TYPE_NODE_DISCOVER_RESP && packet->payload_len >= 6) {
    uint8_t node_type = packet->payload[0] & 0x0F;
    if (node_type != ADV_TYPE_REPEATER) {
      return;
    }
    if (packet->payload_len < 6 + PUB_KEY_SIZE) {
      MESH_DEBUG_PRINTLN("onControlDataRecv: DISCOVER_RESP pubkey too short: %d", (uint32_t)packet->payload_len);
      return;
    }

    if (pending_discover_tag == 0 || millisHasNowPassed(pending_discover_until)) {
      pending_discover_tag = 0;
      return;
    }
    uint32_t tag;
    memcpy(&tag, &packet->payload[2], 4);
    if (tag != pending_discover_tag) {
      return;
    }

    mesh::Identity id(&packet->payload[6]);
    if (id.matches(self_id)) {
      return;
    }
    putNeighbour(id, rtc_clock.getCurrentTime(), packet->getSNR());
  }
}

void MyMesh::sendNodeDiscoverReq() {
  uint8_t data[10];
  data[0] = CTL_TYPE_NODE_DISCOVER_REQ; // prefix_only=0
  data[1] = (1 << ADV_TYPE_REPEATER);
  getRNG()->random(&data[2], 4); // tag
  memcpy(&pending_discover_tag, &data[2], 4);
  pending_discover_until = futureMillis(60000);
  uint32_t since = 0;
  memcpy(&data[6], &since, 4);

  auto pkt = createControlData(data, sizeof(data));
  if (pkt) {
    sendZeroHop(pkt);
  }
}

MyMesh::MyMesh(mesh::MainBoard &board, mesh::Radio &radio, mesh::MillisecondClock &ms, mesh::RNG &rng,
               mesh::RTCClock &rtc, mesh::MeshTables &tables)
    : mesh::Mesh(radio, ms, rng, rtc, *new StaticPoolPacketManager(32), tables),
      region_map(key_store), temp_map(key_store),
      _cli(board, rtc, sensors, region_map, acl, &_prefs, this),
      telemetry(MAX_PACKET_PAYLOAD - 4),
      discover_limiter(4, 120),  // max 4 every 2 minutes
      anon_limiter(4, 180)   // max 4 every 3 minutes
#if defined(WITH_RS232_BRIDGE)
      , bridge(&_prefs, WITH_RS232_BRIDGE, _mgr, &rtc)
#endif
#if defined(WITH_ESPNOW_BRIDGE)
      , bridge(&_prefs, _mgr, &rtc)
#endif
#if defined(WITH_MQTT_OBSERVER)
      , mqtt_observer(&_prefs, &self_id)
#endif
{
  _board = &board;
  last_millis = 0;
  uptime_millis = 0;
  next_local_advert = next_flood_advert = 0;
  dirty_contacts_expiry = 0;
  set_radio_at = revert_radio_at = 0;
  observer_rx_packets = 0;
  observer_mqtt_published = 0;
  observer_path_next = 0;
  observer_next_health_at = 10000;
  observer_health_screen = 0;
  observer_prev_health_valid = false;
  observer_prev_health_screen = 0;
  observer_prev_health_web_state = 0;
  observer_prev_health_web_active = 0;
  observer_prev_health_web_rejects = 0;
  observer_prev_health_web_last = 0;
  observer_prev_health_web_last_age_s = 0;
  observer_prev_health_uptime_s = 0;
  observer_prev_health_rx = 0;
  observer_prev_health_free_heap = 0;
  observer_prev_health_min_heap = 0;
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  observer_web_server = nullptr;
  observer_web_ap_running = false;
  observer_web_sta_running = false;
  observer_web_sta_next_attempt = 0;
  observer_web_sta_started_at = 0;
  observer_web_next_rollover = 60000;
  observer_web_prev_rx_total = 0;
  observer_web_prev_air_ms = 0;
  observer_web_bin_index = 0;
  observer_web_request_busy = false;
  observer_web_active_handler = 0;
  observer_web_last_handler = 0;
  observer_web_last_at = 0;
  observer_web_rejects = 0;
  memset(observer_web_rx_bins, 0, sizeof(observer_web_rx_bins));
  memset(observer_web_air_bins, 0, sizeof(observer_web_air_bins));
#endif
#if defined(ESP32)
  if (observerHealthValid(observer_health_breadcrumb)) {
    observer_prev_health_valid = true;
    observer_prev_health_screen = observer_health_breadcrumb.screen;
    observer_prev_health_web_state = observer_health_breadcrumb.web_state;
    observer_prev_health_web_active = observer_health_breadcrumb.web_active;
    observer_prev_health_web_rejects = observer_health_breadcrumb.web_rejects;
    observer_prev_health_web_last = observer_health_breadcrumb.web_last;
    observer_prev_health_web_last_age_s = observer_health_breadcrumb.web_last_age_s;
    observer_prev_health_uptime_s = observer_health_breadcrumb.uptime_s;
    observer_prev_health_rx = observer_health_breadcrumb.rx_total;
    observer_prev_health_free_heap = observer_health_breadcrumb.free_heap;
    observer_prev_health_min_heap = observer_health_breadcrumb.min_heap;
  }
#endif
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  observer_clock_sync_next = 0;
  observer_clock_sync_count = 0;
  observer_clock_synced = false;
  observer_clock_sync_done = false;
  observer_clock_synced_at = 0;
#endif
  memset(observer_paths, 0, sizeof(observer_paths));
  memset(observer_last_hops, 0, sizeof(observer_last_hops));
#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  memset(observer_clock_sync_samples, 0, sizeof(observer_clock_sync_samples));
#endif
  _logging = false;
  region_load_active = false;

#if MAX_NEIGHBOURS
  memset(neighbours, 0, sizeof(neighbours));
#endif

  // defaults
  memset(&_prefs, 0, sizeof(_prefs));
  _prefs.airtime_factor = 1.0;
  _prefs.rx_delay_base = 0.0f;   // turn off by default, was 10.0;
  _prefs.tx_delay_factor = 0.5f; // was 0.25f
  _prefs.direct_tx_delay_factor = 0.3f; // was 0.2
  StrHelper::strncpy(_prefs.node_name, ADVERT_NAME, sizeof(_prefs.node_name));
  _prefs.node_lat = ADVERT_LAT;
  _prefs.node_lon = ADVERT_LON;
  StrHelper::strncpy(_prefs.password, ADMIN_PASSWORD, sizeof(_prefs.password));
  _prefs.freq = LORA_FREQ;
  _prefs.sf = LORA_SF;
  _prefs.bw = LORA_BW;
  _prefs.cr = LORA_CR;
  _prefs.tx_power_dbm = LORA_TX_POWER;
  _prefs.advert_interval = 1;        // default to 2 minutes for NEW installs
  _prefs.flood_advert_interval = 12; // 12 hours
  _prefs.flood_max = 64;
  _prefs.interference_threshold = 0; // disabled

  // bridge defaults
  _prefs.bridge_enabled = 1;    // enabled
  _prefs.bridge_delay   = 500;  // milliseconds
  _prefs.bridge_pkt_src = 0;    // logTx
  _prefs.bridge_baud = 115200;  // baud rate
  _prefs.bridge_channel = 1;    // channel 1

  StrHelper::strncpy(_prefs.bridge_secret, "LVSITANOS", sizeof(_prefs.bridge_secret));

#ifdef WITH_MQTT_OBSERVER
  _prefs.disable_fwd = 1;
  _prefs.mqtt_enabled = MQTT_OBSERVER_ENABLED;
  _prefs.mqtt_tls = MQTT_OBSERVER_TLS;
  _prefs.mqtt_port = MQTT_OBSERVER_PORT;
  StrHelper::strncpy(_prefs.wifi_ssid, MQTT_OBSERVER_WIFI_SSID, sizeof(_prefs.wifi_ssid));
  StrHelper::strncpy(_prefs.wifi_password, MQTT_OBSERVER_WIFI_PASSWORD, sizeof(_prefs.wifi_password));
  StrHelper::strncpy(_prefs.mqtt_host, MQTT_OBSERVER_HOST, sizeof(_prefs.mqtt_host));
  StrHelper::strncpy(_prefs.mqtt_username, MQTT_OBSERVER_USERNAME, sizeof(_prefs.mqtt_username));
  StrHelper::strncpy(_prefs.mqtt_password, MQTT_OBSERVER_PASSWORD, sizeof(_prefs.mqtt_password));
  StrHelper::strncpy(_prefs.mqtt_topic, MQTT_OBSERVER_TOPIC, sizeof(_prefs.mqtt_topic));
#endif

  // GPS defaults
  _prefs.gps_enabled = 0;
  _prefs.gps_interval = 0;
  _prefs.advert_loc_policy = ADVERT_LOC_PREFS;

  _prefs.adc_multiplier = 0.0f; // 0.0f means use default board multiplier

#if defined(USE_SX1262) || defined(USE_SX1268)
#ifdef SX126X_RX_BOOSTED_GAIN
  _prefs.rx_boosted_gain = SX126X_RX_BOOSTED_GAIN;
#else
  _prefs.rx_boosted_gain = 1; // enabled by default;
#endif
#endif

  pending_discover_tag = 0;
  pending_discover_until = 0;

  memset(default_scope.key, 0, sizeof(default_scope.key));
}

void MyMesh::begin(FILESYSTEM *fs) {
  mesh::Mesh::begin();
  _fs = fs;
  // load persisted prefs
  _cli.loadPrefs(_fs);
  acl.load(_fs, self_id);
  // TODO: key_store.begin();
  region_map.load(_fs);

  // establish default-scope
  {
    RegionEntry* r = region_map.getDefaultRegion();
    if (r) {
      region_map.getTransportKeysFor(*r, &default_scope, 1);
    } else {
#ifdef DEFAULT_FLOOD_SCOPE_NAME
      r = region_map.findByName(DEFAULT_FLOOD_SCOPE_NAME);
      if (r == NULL) {
        r = region_map.putRegion(DEFAULT_FLOOD_SCOPE_NAME, 0);  // auto-create the default scope region
        if (r) { r->flags = 0; }   // Allow-flood
      }
      if (r) {
        region_map.setDefaultRegion(r);
        region_map.getTransportKeysFor(*r, &default_scope, 1);
      }
#endif
    }
  }

#if defined(WITH_BRIDGE)
  if (_prefs.bridge_enabled) {
    bridge.begin();
  }
#endif

#if defined(WITH_MQTT_OBSERVER)
  if (_prefs.mqtt_enabled) {
    mqtt_observer.begin();
  }
#endif

  radio_set_params(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
  radio_set_tx_power(_prefs.tx_power_dbm);

  radio_driver.setRxBoostedGainMode(_prefs.rx_boosted_gain);
  MESH_DEBUG_PRINTLN("RX Boosted Gain Mode: %s",
                     radio_driver.getRxBoostedGainMode() ? "Enabled" : "Disabled");

  updateAdvertTimer();
  updateFloodAdvertTimer();

  board.setAdcMultiplier(_prefs.adc_multiplier);

#if ENV_INCLUDE_GPS == 1
  applyGpsPrefs();
#endif

#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32) && defined(OBSERVER_WEB_VIEW_DEFAULT_ON)
  if (!startObserverWebStaView(nullptr, 0)) {
    startObserverWebAp(nullptr, 0);
  }
#elif defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32) && defined(OBSERVER_WEB_AP_DEFAULT_ON)
  startObserverWebAp(nullptr, 0);
#endif
}

void MyMesh::sendFloodScoped(const TransportKey& scope, mesh::Packet* pkt, uint32_t delay_millis, uint8_t path_hash_size) {
  if (scope.isNull()) {
    sendFlood(pkt, delay_millis, path_hash_size);
  } else {
    uint16_t codes[2];
    codes[0] = scope.calcTransportCode(pkt);
    codes[1] = 0;  // REVISIT: set to 'home' Region, for sender/return region?
    sendFlood(pkt, codes, delay_millis, path_hash_size);
  }
}

const char* MyMesh::getObserverMqttStatus() {
#ifdef WITH_MQTT_OBSERVER
  if (!_prefs.mqtt_enabled) return "off";
  if (!mqtt_observer.isRunning()) return "stopped";
  if (!mqtt_observer.isWifiConnected()) return "wifi...";
  if (!mqtt_observer.isMqttConnected()) return "mqtt...";
  return "ok";
#else
  return "n/a";
#endif
}

const char* MyMesh::getObserverMqttLastError() const {
#ifdef WITH_MQTT_OBSERVER
  const char* err = mqtt_observer.lastError();
  return err && err[0] ? err : "none";
#else
  return "n/a";
#endif
}

uint32_t MyMesh::getObserverMqttPublishFailures() const {
#ifdef WITH_MQTT_OBSERVER
  return mqtt_observer.publishFailures();
#else
  return 0;
#endif
}

uint32_t MyMesh::getObserverMqttWifiFailures() const {
#ifdef WITH_MQTT_OBSERVER
  return mqtt_observer.wifiFailures();
#else
  return 0;
#endif
}

uint32_t MyMesh::getObserverMqttConnectFailures() const {
#ifdef WITH_MQTT_OBSERVER
  return mqtt_observer.mqttFailures();
#else
  return 0;
#endif
}

int MyMesh::getObserverMqttState() const {
#ifdef WITH_MQTT_OBSERVER
  return mqtt_observer.lastMqttState();
#else
  return 0;
#endif
}

void MyMesh::setObserverMqttEnabled(bool enabled) {
#ifdef WITH_MQTT_OBSERVER
  if (_prefs.mqtt_enabled == enabled && mqtt_observer.isRunning() == enabled) return;
  if (enabled) {
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
    if (observer_web_ap_running) stopObserverWebAp();
    if (observer_web_sta_running) stopObserverWebStaView();
#endif
  }
  _prefs.mqtt_enabled = enabled ? 1 : 0;
  _cli.savePrefs(_fs);
  if (enabled) {
    mqtt_observer.begin();
  } else {
    mqtt_observer.end();
  }
#endif
}

bool MyMesh::toggleObserverMqttEnabled() {
#ifdef WITH_MQTT_OBSERVER
  bool enabled = !_prefs.mqtt_enabled;
  setObserverMqttEnabled(enabled);
  return enabled;
#else
  return false;
#endif
}

void MyMesh::rememberObserverPath(const mesh::Packet* packet) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  uint8_t display_hops = min((uint8_t)OBSERVER_PATH_DISPLAY_HOPS, hash_count);
  char text[OBSERVER_PATH_TEXT_SIZE];
  char key[OBSERVER_PATH_KEY_SIZE];
  int key_written = snprintf(key, sizeof(key), "%u:", (unsigned int)hash_size);
  size_t key_pos = key_written > 0 ? (size_t)key_written : 0;
  if (hash_count == 0) {
    snprintf(text, sizeof(text), "-");
  } else {
    size_t pos = 0;
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
    text[pos] = 0;
  }

  const char* hex = "0123456789ABCDEF";
  for (uint8_t n = 0; n < display_hops && key_pos + 2 < sizeof(key); n++) {
    uint8_t i = hash_count - n;
    const uint8_t* hash = &packet->path[(i - 1) * hash_size];
    for (uint8_t j = 0; j < hash_size && key_pos + 2 < sizeof(key); j++) {
      key[key_pos++] = hex[hash[j] >> 4];
      key[key_pos++] = hex[hash[j] & 0x0F];
    }
  }
  key[key_pos] = 0;

  unsigned long now = millis();
  OBSERVER_LOCK();
  for (uint8_t i = 0; i < OBSERVER_PATH_HISTORY_SIZE; i++) {
    ObserverPathInfo& existing = observer_paths[i];
    if (existing.seen_at == 0) continue;
    if (strcmp(existing.key, key) == 0) {
      existing.seen_at = now;
      if (existing.count < 0xFFFF) existing.count++;
      OBSERVER_UNLOCK();
      return;
    }
  }

  int slot = -1;
  unsigned long oldest_seen = 0xFFFFFFFF;
  uint16_t lowest_count = 0xFFFF;
  for (uint8_t i = 0; i < OBSERVER_PATH_HISTORY_SIZE; i++) {
    ObserverPathInfo& item = observer_paths[i];
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

  ObserverPathInfo& item = observer_paths[slot >= 0 ? slot : 0];
  item.seen_at = now;
  item.count = 1;
  StrHelper::strncpy(item.text, text, sizeof(item.text));
  StrHelper::strncpy(item.key, key, sizeof(item.key));
  OBSERVER_UNLOCK();
}

static void formatSnrX4(char* dest, size_t dest_size, int8_t snr_x4) {
  int value = snr_x4;
  int abs_value = abs(value);
  int whole = abs_value / 4;
  int frac = (abs_value % 4) * 25;
  snprintf(dest, dest_size, "%s%d.%02d", value < 0 ? "-" : "", whole, frac);
}

static void formatAge(char* dest, size_t dest_size, unsigned long age_secs) {
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

static bool observerPathContainsToken(const char* path_text, const char* token) {
  if (!path_text || !token || token[0] == 0) return false;
  size_t token_len = strlen(token);
  const char* p = path_text;
  while (*p) {
    while (*p == ' ') p++;
    const char* start = p;
    while (*p && *p != ' ') p++;
    if ((size_t)(p - start) == token_len && strncmp(start, token, token_len) == 0) {
      return true;
    }
  }
  return false;
}

static bool readLine(File& file, char* dest, size_t dest_size) {
  if (!dest || dest_size == 0 || !file.available()) return false;
  size_t pos = 0;
  while (file.available()) {
    int c = file.read();
    if (c < 0) break;
    if (c == '\r') continue;
    if (c == '\n') break;
    if (pos + 1 < dest_size) dest[pos++] = (char)c;
  }
  dest[pos] = 0;
  return pos > 0 || file.available();
}

bool MyMesh::getObserverPathLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  uint8_t found = 0;
  unsigned long now = millis();
  ObserverPathInfo paths[OBSERVER_PATH_HISTORY_SIZE];
  OBSERVER_LOCK();
  memcpy(paths, observer_paths, sizeof(paths));
  OBSERVER_UNLOCK();
  bool selected[OBSERVER_PATH_HISTORY_SIZE];
  memset(selected, 0, sizeof(selected));

  for (uint8_t i = 0; i < OBSERVER_PATH_HISTORY_SIZE; i++) {
    int best = -1;
    uint16_t best_count = 0;
    unsigned long best_seen = 0;

    for (uint8_t j = 0; j < OBSERVER_PATH_HISTORY_SIZE; j++) {
      const ObserverPathInfo& item = paths[j];
      if (item.seen_at == 0) continue;
      if (selected[j]) continue;

      if (item.count > best_count || (item.count == best_count && item.seen_at > best_seen)) {
        best = j;
        best_count = item.count;
        best_seen = item.seen_at;
      }
    }

    if (best < 0) {
      return false;
    }

    selected[best] = true;
    if (found == index) {
      const ObserverPathInfo& item = paths[best];
      char count_col[6];
      char age_col[5];
      unsigned long age_secs = (now - item.seen_at) / 1000;
      if (item.count > 999) {
        snprintf(count_col, sizeof(count_col), "999+");
      } else {
        snprintf(count_col, sizeof(count_col), "%u", (unsigned int)item.count);
      }
      formatAge(age_col, sizeof(age_col), age_secs);
      snprintf(dest, dest_size, "%3s %s %s", count_col, item.text, age_col);
      return true;
    }
    found++;
  }
  return false;
}

bool MyMesh::getObserverLatestPathLine(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  int best = -1;
  unsigned long best_seen = 0;
  ObserverPathInfo paths[OBSERVER_PATH_HISTORY_SIZE];
  OBSERVER_LOCK();
  memcpy(paths, observer_paths, sizeof(paths));
  OBSERVER_UNLOCK();
  for (uint8_t i = 0; i < OBSERVER_PATH_HISTORY_SIZE; i++) {
    const ObserverPathInfo& item = paths[i];
    if (item.seen_at == 0) continue;
    if (item.seen_at > best_seen) {
      best = i;
      best_seen = item.seen_at;
    }
  }

  if (best < 0) return false;

  const ObserverPathInfo& item = paths[best];
  char age_col[5];
  unsigned long age_secs = (millis() - item.seen_at) / 1000;
  formatAge(age_col, sizeof(age_col), age_secs);
  snprintf(dest, dest_size, "%s %s", age_col, item.text);
  return true;
}

void MyMesh::rememberObserverLastHop(const mesh::Packet* packet, int8_t snr_x4) {
  uint8_t hash_size = packet->getPathHashSize();
  uint8_t hash_count = packet->getPathHashCount();
  char text[OBSERVER_LAST_HOP_TEXT_SIZE];
  char key[OBSERVER_LAST_HOP_TEXT_SIZE];
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
  OBSERVER_LOCK();
  for (uint8_t i = 0; i < OBSERVER_LAST_HOP_HISTORY_SIZE; i++) {
    ObserverLastHopInfo& item = observer_last_hops[i];
    if (item.seen_at == 0) continue;
    if (strcmp(item.key, key) == 0) {
      item.seen_at = now;
      item.last_snr = snr_x4;
      if (snr_x4 > item.max_snr) item.max_snr = snr_x4;
      OBSERVER_UNLOCK();
      return;
    }
  }

  int slot = -1;
  unsigned long oldest_seen = 0xFFFFFFFF;
  for (uint8_t i = 0; i < OBSERVER_LAST_HOP_HISTORY_SIZE; i++) {
    ObserverLastHopInfo& item = observer_last_hops[i];
    if (item.seen_at == 0) {
      slot = i;
      break;
    }
    if (item.seen_at < oldest_seen) {
      slot = i;
      oldest_seen = item.seen_at;
    }
  }

  ObserverLastHopInfo& item = observer_last_hops[slot >= 0 ? slot : 0];
  item.seen_at = now;
  item.last_snr = snr_x4;
  item.max_snr = snr_x4;
  StrHelper::strncpy(item.text, text, sizeof(item.text));
  StrHelper::strncpy(item.key, key, sizeof(item.key));
  OBSERVER_UNLOCK();
}

bool MyMesh::getObserverLastHopLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return false;
  dest[0] = 0;

  uint8_t found = 0;
  unsigned long now = millis();
  ObserverLastHopInfo last_hops[OBSERVER_LAST_HOP_HISTORY_SIZE];
  ObserverPathInfo paths[OBSERVER_PATH_HISTORY_SIZE];
  OBSERVER_LOCK();
  memcpy(last_hops, observer_last_hops, sizeof(last_hops));
  memcpy(paths, observer_paths, sizeof(paths));
  OBSERVER_UNLOCK();
  bool selected[OBSERVER_LAST_HOP_HISTORY_SIZE];
  memset(selected, 0, sizeof(selected));

  for (uint8_t i = 0; i < OBSERVER_LAST_HOP_HISTORY_SIZE; i++) {
    int best = -1;
    unsigned long best_seen = 0;

    for (uint8_t j = 0; j < OBSERVER_LAST_HOP_HISTORY_SIZE; j++) {
      const ObserverLastHopInfo& item = last_hops[j];
      if (item.seen_at == 0) continue;
      if (selected[j]) continue;

      if (item.seen_at > best_seen) {
        best = j;
        best_seen = item.seen_at;
      }
    }

    if (best < 0) return false;

    selected[best] = true;
    if (found == index) {
      const ObserverLastHopInfo& item = last_hops[best];
      char age_col[5];
      char max_col[8];
      char last_col[8];
      uint8_t path_count = 0;
      unsigned long age_secs = (now - item.seen_at) / 1000;
      formatAge(age_col, sizeof(age_col), age_secs);
      formatSnrX4(max_col, sizeof(max_col), item.max_snr);
      formatSnrX4(last_col, sizeof(last_col), item.last_snr);
      for (uint8_t k = 0; k < OBSERVER_PATH_HISTORY_SIZE; k++) {
        const ObserverPathInfo& path = paths[k];
        if (path.seen_at == 0) continue;
        if (observerPathContainsToken(path.text, item.text)) path_count++;
      }
      snprintf(dest, dest_size, "%6s %4s %6s %6s %2u", item.text, age_col, max_col, last_col,
               (unsigned int)path_count);
      return true;
    }
    found++;
  }
  return false;
}

void MyMesh::getObserverDiagLine(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;

#if defined(ESP32)
  snprintf(dest, dest_size, "Boot:%s H:%luk/%luk",
           observerResetReasonString(esp_reset_reason()),
           (unsigned long)(ESP.getFreeHeap() / 1024),
           (unsigned long)(ESP.getMinFreeHeap() / 1024));
#else
  snprintf(dest, dest_size, "Boot:n/a Up:%lus", (unsigned long)(millis() / 1000));
#endif
}

void MyMesh::getObserverHealthLine(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;

  if (!observer_prev_health_valid) {
#if defined(ESP32)
    snprintf(dest, dest_size, "Prev:none H:%luk/%luk",
             (unsigned long)(ESP.getFreeHeap() / 1024),
             (unsigned long)(ESP.getMinFreeHeap() / 1024));
#else
    snprintf(dest, dest_size, "Prev:none");
#endif
    return;
  }

  uint32_t up = observer_prev_health_uptime_s;
  char up_col[8];
  if (up < 180) {
    snprintf(up_col, sizeof(up_col), "%lus", (unsigned long)up);
  } else if (up < 3600) {
    snprintf(up_col, sizeof(up_col), "%lum", (unsigned long)(up / 60));
  } else {
    snprintf(up_col, sizeof(up_col), "%luh", (unsigned long)(up / 3600));
  }

  const char* web_state = "off";
  if (observer_prev_health_web_state & 0x02) {
    web_state = (observer_prev_health_web_state & 0x04) ? "View+" : "View-";
  } else if (observer_prev_health_web_state & 0x01) {
    web_state = "AP";
  }

  char web_extra[24] = "";
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  if (observer_prev_health_web_active || observer_prev_health_web_rejects) {
    snprintf(web_extra, sizeof(web_extra), " Q:%u/%u",
             (unsigned int)observer_prev_health_web_active,
             (unsigned int)observer_prev_health_web_rejects);
  } else if (observer_prev_health_web_last && observer_prev_health_web_last_age_s < 600) {
    snprintf(web_extra, sizeof(web_extra), " L:%u/%us",
             (unsigned int)observer_prev_health_web_last,
             (unsigned int)observer_prev_health_web_last_age_s);
  }
#endif

  snprintf(dest, dest_size, "Prev:S%u %s W:%s%s H:%luk/%luk RX:%lu",
           (unsigned int)observer_prev_health_screen,
           up_col,
           web_state,
           web_extra,
           (unsigned long)(observer_prev_health_free_heap / 1024),
           (unsigned long)(observer_prev_health_min_heap / 1024),
           (unsigned long)observer_prev_health_rx);
}

void MyMesh::setObserverHealthScreen(uint8_t screen) {
  observer_health_screen = screen;
}

void MyMesh::updateObserverHealth() {
#if defined(ESP32)
  unsigned long now = millis();
  if ((long)(now - observer_next_health_at) < 0) return;
  observer_next_health_at = now + OBSERVER_HEALTH_UPDATE_MS;

  ObserverHealthBreadcrumb item;
  item.magic = OBSERVER_HEALTH_MAGIC;
  item.seq = observerHealthValid(observer_health_breadcrumb) ? observer_health_breadcrumb.seq + 1 : 1;
  item.uptime_s = now / 1000;
  item.rx_total = observer_rx_packets;
  item.mqtt_total = observer_mqtt_published;
  item.free_heap = ESP.getFreeHeap();
  item.min_heap = ESP.getMinFreeHeap();
  item.screen = observer_health_screen;
  item.web_state = 0;
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  if (observer_web_ap_running) item.web_state |= 0x01;
  if (observer_web_sta_running) item.web_state |= 0x02;
  if (WiFi.status() == WL_CONNECTED) item.web_state |= 0x04;
  if (WiFi.getMode() & WIFI_MODE_AP) item.web_state |= 0x08;
  if (WiFi.getMode() & WIFI_MODE_STA) item.web_state |= 0x10;
  OBSERVER_LOCK();
  item.web_active = observer_web_active_handler;
  item.web_rejects = observer_web_rejects;
  item.web_last = observer_web_last_handler;
  item.web_last_age_s = observer_web_last_at
      ? (uint16_t)min(65535UL, (now - observer_web_last_at) / 1000UL)
      : 65535;
  OBSERVER_UNLOCK();
#else
  item.web_active = 0;
  item.web_rejects = 0;
  item.web_last = 0;
  item.web_last_age_s = 65535;
#endif
  item.crc = observerHealthCrc(item);
  observer_health_breadcrumb = item;
#endif
}

#ifdef ENABLE_OBSERVER_SAVEPOINTS
static void formatSavepointFilename(char* dest, size_t dest_size, uint16_t id) {
  snprintf(dest, dest_size, "/sp%04u.csv", (unsigned int)id);
}

bool MyMesh::getObserverSavepointLine(uint8_t index, char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0 || !_fs) return false;
  dest[0] = 0;

  File file = _fs->open(SAVEPOINT_INDEX_FILE);
  if (!file) return false;

  char line[OBSERVER_SAVEPOINT_LINE_SIZE];
  uint8_t found = 0;
  while (readLine(file, line, sizeof(line))) {
    if (line[0] == 0) continue;
    if (found == index) {
      unsigned int id = 0;
      unsigned long ts = 0;
      unsigned long rx = 0;
      int nf = 0;
      if (sscanf(line, "%u,%lu,%lu,%d", &id, &ts, &rx, &nf) == 4) {
        DateTime dt((uint32_t)ts);
        snprintf(dest, dest_size, "%04u %02d:%02d RX:%lu NF:%d",
                 id, dt.hour(), dt.minute(), rx, nf);
      } else {
        StrHelper::strncpy(dest, line, dest_size);
      }
      file.close();
      return true;
    }
    found++;
  }

  file.close();
  return false;
}
#endif

void MyMesh::getObserverClockSyncStatus(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;
  uint32_t current = getRTCClock()->getCurrentTime();
  DateTime now_dt(current);

#ifdef ENABLE_OBSERVER_CLOCK_SYNC
  if (observer_clock_synced) {
    DateTime dt(observer_clock_synced_at);
    snprintf(dest, dest_size, "CLK %02d:%02d UTC mesh %02d:%02d",
             now_dt.hour(), now_dt.minute(), dt.hour(), dt.minute());
    return;
  }

  if (current >= CLOCK_SYNC_MIN_TIME) {
    snprintf(dest, dest_size, "CLK %02d:%02d UTC set", now_dt.hour(), now_dt.minute());
    return;
  }

  uint8_t distinct = 0;
  for (uint8_t i = 0; i < OBSERVER_CLOCK_SYNC_SAMPLES; i++) {
    if (!observer_clock_sync_samples[i].used) continue;
    bool seen = false;
    for (uint8_t j = 0; j < i; j++) {
      if (!observer_clock_sync_samples[j].used) continue;
      if (memcmp(observer_clock_sync_samples[i].pub_key, observer_clock_sync_samples[j].pub_key, PUB_KEY_SIZE) == 0) {
        seen = true;
        break;
      }
    }
    if (!seen) distinct++;
  }

  snprintf(dest, dest_size, "CLK %02d:%02d UTC wait %u/%u n%u/%u",
           now_dt.hour(),
           now_dt.minute(),
           (unsigned int)observer_clock_sync_count,
           (unsigned int)OBSERVER_CLOCK_SYNC_REQUIRED,
           (unsigned int)distinct,
           (unsigned int)OBSERVER_CLOCK_SYNC_DISTINCT);
#else
  snprintf(dest, dest_size, "CLK %02d:%02d UTC no sync", now_dt.hour(), now_dt.minute());
#endif
}

#ifdef ENABLE_OBSERVER_SAVEPOINTS
bool MyMesh::createObserverSavepoint(const uint16_t* activity_bins, const uint16_t* airtime_bins, uint8_t bin_count,
                                     uint8_t newest_bin, char* status, size_t status_size) {
  if (status && status_size) status[0] = 0;
  if (!_fs) return false;

  uint16_t next_id = 1;
  uint8_t count = 0;
  File index = _fs->open(SAVEPOINT_INDEX_FILE);
  if (index) {
    char line[OBSERVER_SAVEPOINT_LINE_SIZE];
    while (readLine(index, line, sizeof(line))) {
      unsigned int id = 0;
      if (sscanf(line, "%u,", &id) == 1) {
        if (id >= next_id) next_id = id + 1;
        count++;
      }
    }
    index.close();
  }

  if (count >= OBSERVER_SAVEPOINT_MAX) {
    index = _fs->open(SAVEPOINT_INDEX_FILE);
    if (index) {
      char line[OBSERVER_SAVEPOINT_LINE_SIZE];
      if (readLine(index, line, sizeof(line))) {
        unsigned int old_id = 0;
        if (sscanf(line, "%u,", &old_id) == 1) {
          char old_name[16];
          formatSavepointFilename(old_name, sizeof(old_name), (uint16_t)old_id);
          _fs->remove(old_name);
        }
      }
      File tmp = _fs->open("/sp_index.tmp", "w");
      while (tmp && readLine(index, line, sizeof(line))) {
        tmp.println(line);
      }
      index.close();
      if (tmp) tmp.close();
      _fs->remove(SAVEPOINT_INDEX_FILE);
      _fs->rename("/sp_index.tmp", SAVEPOINT_INDEX_FILE);
    }
  }

  char filename[16];
  formatSavepointFilename(filename, sizeof(filename), next_id);
  File file = _fs->open(filename, "w");
  if (!file) {
    if (status && status_size) snprintf(status, status_size, "SP save failed");
    return false;
  }

  uint32_t now = getRTCClock()->getCurrentTime();
  DateTime dt(now);
  uint32_t rx_total = observer_rx_packets;
  uint32_t mqtt_total = observer_mqtt_published;
  int noise_floor = getObserverNoiseFloor();
  int snr_x4 = (int)(getObserverLastSnr() * 4.0f);

  file.println("section,key,value");
  file.printf("meta,id,%u\n", (unsigned int)next_id);
  file.printf("meta,timestamp,%lu\n", (unsigned long)now);
  file.printf("meta,datetime_utc,%04d-%02d-%02dT%02d:%02d:%02dZ\n",
              dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second());
  char clock_sync[32];
  getObserverClockSyncStatus(clock_sync, sizeof(clock_sync));
  file.printf("meta,clock_sync,%s\n", clock_sync);
  file.printf("node,name,%s\n", _prefs.node_name);
  file.printf("node,lat,%.6f\n", _prefs.node_lat);
  file.printf("node,lon,%.6f\n", _prefs.node_lon);
  file.printf("radio,freq,%.3f\n", _prefs.freq);
  file.printf("radio,sf,%u\n", (unsigned int)_prefs.sf);
  file.printf("radio,bw,%.2f\n", _prefs.bw);
  file.printf("radio,cr,%u\n", (unsigned int)_prefs.cr);
  file.printf("radio,noise_floor,%d\n", noise_floor);
  file.printf("radio,last_snr_x4,%d\n", snr_x4);
  file.printf("power,battery_mv,%u\n", (unsigned int)getObserverBattMilliVolts());
#ifdef ESP32
  file.printf("system,free_heap,%lu\n", (unsigned long)ESP.getFreeHeap());
#endif
  file.printf("counter,rx_total,%lu\n", (unsigned long)rx_total);
  file.printf("counter,mqtt_total,%lu\n", (unsigned long)mqtt_total);
  file.printf("mqtt,enabled,%u\n", (unsigned int)_prefs.mqtt_enabled);
  file.printf("mqtt,status,%s\n", getObserverMqttStatus());
  file.printf("mqtt,host,%s\n", _prefs.mqtt_host);
  file.printf("mqtt,publish_failures,%lu\n", (unsigned long)getObserverMqttPublishFailures());

  if (activity_bins && bin_count > 0) {
    for (uint8_t i = 0; i < bin_count; i++) {
      uint8_t idx = (newest_bin + bin_count - i) % bin_count;
      file.printf("hist,%u,%u\n", (unsigned int)i, (unsigned int)activity_bins[idx]);
    }
  }
  if (airtime_bins && bin_count > 0) {
    for (uint8_t i = 0; i < bin_count; i++) {
      uint8_t idx = (newest_bin + bin_count - i) % bin_count;
      file.printf("airtime_ms,%u,%u\n", (unsigned int)i, (unsigned int)airtime_bins[idx]);
    }
  }

  char line[96];
  for (uint8_t i = 0; i < OBSERVER_PATH_HISTORY_SIZE; i++) {
    if (getObserverPathLine(i, line, sizeof(line))) {
      file.printf("path,%u,%s\n", (unsigned int)i, line);
    }
  }
  for (uint8_t i = 0; i < OBSERVER_LAST_HOP_HISTORY_SIZE; i++) {
    if (getObserverLastHopLine(i, line, sizeof(line))) {
      file.printf("heard,%u,%s\n", (unsigned int)i, line);
    }
  }
  file.close();

  index = _fs->open(SAVEPOINT_INDEX_FILE, "a");
  if (!index) {
    if (status && status_size) snprintf(status, status_size, "SP index failed");
    return false;
  }
  index.printf("%u,%lu,%lu,%d\n", (unsigned int)next_id, (unsigned long)now, (unsigned long)rx_total, noise_floor);
  index.close();

  if (status && status_size) snprintf(status, status_size, "SP %u saved", (unsigned int)next_id);
  return true;
}
#endif

#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
String MyMesh::buildObserverSavepointListCsv() const {
  String body = "id,datetime_utc,rx_total,noise_floor\n";
  if (!_fs) return body;

  File f = _fs->open(SAVEPOINT_INDEX_FILE);
  if (!f) return body;

  char line[OBSERVER_SAVEPOINT_LINE_SIZE];
  while (readLine(f, line, sizeof(line))) {
    unsigned int id = 0;
    unsigned long ts = 0;
    unsigned long rx = 0;
    int nf = 0;
    if (sscanf(line, "%u,%lu,%lu,%d", &id, &ts, &rx, &nf) != 4) continue;
    DateTime dt((uint32_t)ts);
    char row[72];
    snprintf(row, sizeof(row), "%u,%04d-%02d-%02dT%02d:%02d:%02dZ,%lu,%d\n",
             id, dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), dt.second(), rx, nf);
    body += row;
  }
  f.close();
  return body;
}

void MyMesh::updateObserverWebMetrics() {
  unsigned long now = millis();
  OBSERVER_LOCK();
  while ((long)(now - observer_web_next_rollover) >= 0) {
    observer_web_bin_index = (observer_web_bin_index + 1) % 12;
    observer_web_rx_bins[observer_web_bin_index] = 0;
    observer_web_air_bins[observer_web_bin_index] = 0;
    observer_web_next_rollover += 60000UL;
  }

  if (observer_rx_packets != observer_web_prev_rx_total) {
    uint32_t delta = observer_rx_packets - observer_web_prev_rx_total;
    uint32_t value = (uint32_t)observer_web_rx_bins[observer_web_bin_index] + delta;
    observer_web_rx_bins[observer_web_bin_index] = value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
    observer_web_prev_rx_total = observer_rx_packets;
  }

  uint32_t air_ms = getReceiveAirTime();
  if (air_ms != observer_web_prev_air_ms) {
    uint32_t delta = air_ms - observer_web_prev_air_ms;
    uint32_t value = (uint32_t)observer_web_air_bins[observer_web_bin_index] + delta;
    observer_web_air_bins[observer_web_bin_index] = value > UINT16_MAX ? UINT16_MAX : (uint16_t)value;
    observer_web_prev_air_ms = air_ms;
  }
  OBSERVER_UNLOCK();
}

static void appendJsonString(String& body, const char* text);

String MyMesh::buildObserverWebStatusJson() const {
  String body = "{\"node\":";
  body.reserve(768);
  appendJsonString(body, _prefs.node_name);
  body += ",\"time\":";
  body += String((unsigned long)getRTCClock()->getCurrentTime());
  body += ",\"lat\":";
  body += String(_prefs.node_lat, 6);
  body += ",\"lon\":";
  body += String(_prefs.node_lon, 6);
  body += ",\"view_mode\":";
  body += observer_web_sta_running ? "true" : "false";
  body += ",\"snr\":";
  body += String(getObserverLastSnr(), 1);
  body += ",\"nf\":";
  body += String(getObserverNoiseFloor());
  body += ",\"free_heap\":";
  body += String((unsigned long)ESP.getFreeHeap());
  body += ",\"batt_mv\":";
  body += String((unsigned int)getObserverBattMilliVolts());
  body += ",\"savepoints\":[";

  bool first = true;
  File f = _fs ? _fs->open(SAVEPOINT_INDEX_FILE) : File();
  if (f) {
    char line[OBSERVER_SAVEPOINT_LINE_SIZE];
    while (readLine(f, line, sizeof(line))) {
      unsigned int id = 0;
      unsigned long ts = 0;
      unsigned long rx = 0;
      int nf = 0;
      if (sscanf(line, "%u,%lu,%lu,%d", &id, &ts, &rx, &nf) != 4) continue;
      DateTime dt((uint32_t)ts);
      char row[128];
      snprintf(row, sizeof(row), "%s{\"id\":%u,\"time\":\"%04d-%02d-%02d %02d:%02d\",\"rx\":%lu,\"nf\":%d}",
               first ? "" : ",", id, dt.year(), dt.month(), dt.day(), dt.hour(), dt.minute(), rx, nf);
      body += row;
      first = false;
    }
    f.close();
  }

  body += "]}";
  return body;
}

static void appendJsonString(String& body, const char* text) {
  body += "\"";
  if (text) {
    while (*text) {
      unsigned char c = (unsigned char)*text++;
      if (c == '"' || c == '\\') {
        body += "\\";
        body += (char)c;
      } else if (c == '\n') {
        body += "\\n";
      } else if (c == '\r') {
        body += "\\r";
      } else if (c == '\t') {
        body += "\\t";
      } else if (c < 0x20) {
        char esc[7];
        snprintf(esc, sizeof(esc), "\\u%04x", (unsigned int)c);
        body += esc;
      } else {
        body += (char)c;
      }
    }
  }
  body += "\"";
}

String MyMesh::buildObserverWebMonitorJson() const {
  uint16_t rx_bins[12];
  uint16_t air_bins[12];
  uint8_t bin_index;
  OBSERVER_LOCK();
  memcpy(rx_bins, observer_web_rx_bins, sizeof(rx_bins));
  memcpy(air_bins, observer_web_air_bins, sizeof(air_bins));
  bin_index = observer_web_bin_index;
  OBSERVER_UNLOCK();

  String body = "{\"rx\":[";
  body.reserve(1536);
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t idx = (bin_index + i + 1) % 12;
    if (i) body += ",";
    body += String((unsigned int)rx_bins[idx]);
  }
  body += "],\"airtime_ms\":[";
  for (uint8_t i = 0; i < 12; i++) {
    uint8_t idx = (bin_index + i + 1) % 12;
    if (i) body += ",";
    body += String((unsigned int)air_bins[idx]);
  }
  body += "],\"paths\":[";
  char line[96];
  uint8_t path_count = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (!getObserverPathLine(i, line, sizeof(line))) continue;
    if (path_count++) body += ",";
    appendJsonString(body, line);
  }
  body += "],\"heards\":[";
  uint8_t heard_count = 0;
  for (uint8_t i = 0; i < 10; i++) {
    if (!getObserverLastHopLine(i, line, sizeof(line))) continue;
    if (heard_count++) body += ",";
    appendJsonString(body, line);
  }
  body += "],\"heat\":{\"paths\":[";

  char path_text[8][32];
  memset(path_text, 0, sizeof(path_text));
  path_count = 0;
  for (uint8_t i = 0; i < 8; i++) {
    if (!getObserverPathLine(i, line, sizeof(line))) continue;
    char work[96];
    snprintf(work, sizeof(work), "%s", line);
    char* cursor = work;
    char* count = strtok(cursor, " ");
    (void)count;
    char path[32] = "";
    char* token = nullptr;
    while ((token = strtok(nullptr, " ")) != nullptr) {
      size_t len = strlen(token);
      if (strcmp(token, ">999") == 0 || (len > 1 && (token[len - 1] == 's' || token[len - 1] == 'm'))) break;
      if (strcmp(token, "-") != 0) {
        if (path[0]) strncat(path, " ", sizeof(path) - strlen(path) - 1);
        strncat(path, token, sizeof(path) - strlen(path) - 1);
      }
    }
    if (!path[0]) continue;
    snprintf(path_text[path_count], sizeof(path_text[path_count]), "%s", path);
    if (path_count) body += ",";
    appendJsonString(body, path_text[path_count]);
    path_count++;
  }

  struct WebHeatRow {
    char rep[8];
    uint8_t pos[8];
    uint8_t pc;
  };
  WebHeatRow rows[10];
  uint8_t row_count = 0;
  memset(rows, 0, sizeof(rows));
  for (uint8_t p = 0; p < path_count; p++) {
    char work[32];
    snprintf(work, sizeof(work), "%s", path_text[p]);
    char* token = strtok(work, " ");
    uint8_t pos = 1;
    while (token) {
      int row = -1;
      for (uint8_t r = 0; r < row_count; r++) {
        if (strcmp(rows[r].rep, token) == 0) {
          row = r;
          break;
        }
      }
      if (row < 0 && row_count < 10) {
        row = row_count++;
        snprintf(rows[row].rep, sizeof(rows[row].rep), "%s", token);
      }
      if (row >= 0) {
        rows[row].pos[p] = pos;
      }
      if (pos < 3) pos++;
      token = strtok(nullptr, " ");
    }
  }
  for (uint8_t r = 0; r < row_count; r++) {
    uint8_t pc = 0;
    for (uint8_t p = 0; p < path_count; p++) {
      if (rows[r].pos[p]) pc++;
    }
    rows[r].pc = pc;
  }
  for (uint8_t i = 0; i < row_count; i++) {
    for (uint8_t j = i + 1; j < row_count; j++) {
      if (rows[j].pc > rows[i].pc) {
        WebHeatRow tmp = rows[i];
        rows[i] = rows[j];
        rows[j] = tmp;
      }
    }
  }

  body += "],\"rows\":[";
  for (uint8_t r = 0; r < row_count; r++) {
    if (r) body += ",";
    body += "{\"rep\":";
    appendJsonString(body, rows[r].rep);
    body += ",\"pc\":";
    body += String((unsigned int)rows[r].pc);
    body += ",\"pos\":[";
    for (uint8_t p = 0; p < path_count; p++) {
      if (p) body += ",";
      body += String((unsigned int)rows[r].pos[p]);
    }
    body += "]}";
  }
  body += "]}}";
  return body;
}

bool MyMesh::beginObserverWebRequest(uint8_t handler, AsyncWebServerRequest* request) {
  bool busy = false;
  OBSERVER_LOCK();
  if (observer_web_request_busy) {
    busy = true;
    if (observer_web_rejects < 255) observer_web_rejects++;
  } else {
    observer_web_request_busy = true;
    observer_web_active_handler = handler;
    observer_web_last_handler = handler;
    observer_web_last_at = millis();
  }
  OBSERVER_UNLOCK();

  observer_next_health_at = 0;
  updateObserverHealth();

  if (busy) {
    if (request) request->send(429, "text/plain", "busy");
    return false;
  }
  return true;
}

void MyMesh::endObserverWebRequest() {
  OBSERVER_LOCK();
  observer_web_request_busy = false;
  observer_web_active_handler = 0;
  OBSERVER_UNLOCK();
  observer_next_health_at = 0;
  updateObserverHealth();
}

void MyMesh::setupObserverWebRoutes() {
  if (!observer_web_server) return;

  observer_web_server->on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send_P(200, "text/html", OBSERVER_WEB_HTML);
  });

  observer_web_server->on("/api/status", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!beginObserverWebRequest(2, request)) return;
    String body = buildObserverWebStatusJson();
    request->send(200, "application/json", body);
    endObserverWebRequest();
  });

  observer_web_server->on("/api/monitor", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!beginObserverWebRequest(3, request)) return;
    static String body;
    static unsigned long body_at = 0;
    unsigned long now = millis();
    if (!body.length() || (long)(now - body_at) >= 55000L) {
      body = buildObserverWebMonitorJson();
      body_at = now;
    }
    endObserverWebRequest();
    request->send(200, "application/json", body);
  });

  observer_web_server->on("/api/time", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (observer_web_sta_running) {
      request->send(403, "text/plain", "view mode");
      return;
    }
    if (!request->hasParam("epoch")) {
      request->send(400, "text/plain", "missing epoch");
      return;
    }
    uint32_t epoch = (uint32_t)strtoul(request->getParam("epoch")->value().c_str(), nullptr, 10);
    if (epoch < CLOCK_SYNC_MIN_TIME || epoch > CLOCK_SYNC_MAX_TIME) {
      request->send(400, "text/plain", "invalid epoch");
      return;
    }
    getRTCClock()->setCurrentTime(epoch);
    request->send(200, "text/plain", "time set");
  });

  observer_web_server->on("/api/position", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (observer_web_sta_running) {
      request->send(403, "text/plain", "view mode");
      return;
    }
    handleObserverWebPosition(this, request);
  });

#ifdef ENABLE_OBSERVER_SAVEPOINTS
  observer_web_server->on("/api/savepoint", HTTP_POST, [this](AsyncWebServerRequest* request) {
    if (!beginObserverWebRequest(4, request)) return;
    char status[32];
    updateObserverWebMetrics();
    uint16_t rx_bins[12];
    uint16_t air_bins[12];
    uint8_t bin_index;
    OBSERVER_LOCK();
    memcpy(rx_bins, observer_web_rx_bins, sizeof(rx_bins));
    memcpy(air_bins, observer_web_air_bins, sizeof(air_bins));
    bin_index = observer_web_bin_index;
    OBSERVER_UNLOCK();
    bool ok = createObserverSavepoint(rx_bins, air_bins, 12, bin_index, status, sizeof(status));
    request->send(ok ? 200 : 500, "text/plain", status[0] ? status : (ok ? "SP saved" : "SP save failed"));
    endObserverWebRequest();
  });
#endif

  observer_web_server->on("/sp.list.csv", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!beginObserverWebRequest(5, request)) return;
    String body = buildObserverSavepointListCsv();
    request->send(200, "text/csv", body);
    endObserverWebRequest();
  });

  observer_web_server->on("/sp.csv", HTTP_GET, [this](AsyncWebServerRequest* request) {
    if (!beginObserverWebRequest(6, request)) return;
    if (!request->hasParam("id")) {
      endObserverWebRequest();
      request->send(400, "text/plain", "missing id");
      return;
    }
    uint16_t id = (uint16_t)atoi(request->getParam("id")->value().c_str());
    char filename[16];
    formatSavepointFilename(filename, sizeof(filename), id);
    if (!_fs || !_fs->exists(filename)) {
      endObserverWebRequest();
      request->send(404, "text/plain", "savepoint not found");
      return;
    }
    char download_name[20];
    snprintf(download_name, sizeof(download_name), "sp%04u.csv", (unsigned int)id);
    AsyncWebServerResponse* response = request->beginResponse(*_fs, filename, "text/csv", true);
    response->addHeader("Content-Disposition", String("attachment; filename=\"") + download_name + "\"");
    request->send(response);
    endObserverWebRequest();
  });
}

bool MyMesh::startObserverWebAp(char* status, size_t status_size) {
  if (observer_web_ap_running) {
    if (status && status_size) snprintf(status, status_size, "AP http://%s", WiFi.softAPIP().toString().c_str());
    return true;
  }
  if (observer_web_sta_running) {
    stopObserverWebStaView();
  }

#ifdef WITH_MQTT_OBSERVER
  mqtt_observer.end();
#endif
  if (observer_web_server) {
    observer_web_server->end();
  }
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(OBSERVER_WEB_AP_SSID, OBSERVER_WEB_AP_PASSWORD);
  if (!ok) {
    if (status && status_size) snprintf(status, status_size, "AP start failed");
    return false;
  }

  if (!observer_web_server) {
    observer_web_server = new AsyncWebServer(80);
    setupObserverWebRoutes();
  }
  delay(50);
  observer_web_server->begin();
  observer_web_ap_running = true;
  observer_web_sta_running = false;
  observer_web_sta_next_attempt = 0;
  observer_web_sta_started_at = 0;
  if (status && status_size) snprintf(status, status_size, "AP http://%s", WiFi.softAPIP().toString().c_str());
  return true;
}

void MyMesh::stopObserverWebAp() {
  if (!observer_web_ap_running) return;
  if (observer_web_server) {
    observer_web_server->end();
  }
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  observer_web_ap_running = false;
#ifdef WITH_MQTT_OBSERVER
  if (_prefs.mqtt_enabled) mqtt_observer.begin();
#endif
}

bool MyMesh::startObserverWebStaView(char* status, size_t status_size) {
  if (observer_web_sta_running) {
    IPAddress ip = WiFi.localIP();
    if (status && status_size) snprintf(status, status_size, "View http://%s", ip.toString().c_str());
    return true;
  }

#ifdef WITH_MQTT_OBSERVER
  if (_prefs.mqtt_enabled || mqtt_observer.isRunning()) {
    _prefs.mqtt_enabled = 0;
    _cli.savePrefs(_fs);
    mqtt_observer.end();
  }
#endif
  if (observer_web_ap_running) {
    stopObserverWebAp();
  }

  if (!_prefs.wifi_ssid[0]) {
    if (status && status_size) snprintf(status, status_size, "View no WiFi");
    return false;
  }

  if (observer_web_server) {
    observer_web_server->end();
  }
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);
  WiFi.mode(WIFI_STA);
  WiFi.begin(_prefs.wifi_ssid, _prefs.wifi_password);
  if (!observer_web_server) {
    observer_web_server = new AsyncWebServer(80);
    setupObserverWebRoutes();
  }
  delay(50);
  observer_web_server->begin();
  observer_web_sta_running = true;
  observer_web_ap_running = false;
  observer_web_sta_started_at = millis();
  observer_web_sta_next_attempt = millis() + OBSERVER_WEB_VIEW_RETRY_MS;
  if (status && status_size) snprintf(status, status_size, "View connecting");
  return true;
}

void MyMesh::stopObserverWebStaView() {
  if (!observer_web_sta_running) return;
  if (observer_web_server) {
    observer_web_server->end();
  }
  WiFi.disconnect(true);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  observer_web_sta_running = false;
  observer_web_sta_next_attempt = 0;
  observer_web_sta_started_at = 0;
}

void MyMesh::setObserverPosition(double lat, double lon) {
  _prefs.node_lat = lat;
  _prefs.node_lon = lon;
  savePrefs();
}

bool MyMesh::toggleObserverWebAp(char* status, size_t status_size) {
  if (observer_web_ap_running) {
    stopObserverWebAp();
    if (status && status_size) snprintf(status, status_size, "AP stopped");
    return false;
  }
  startObserverWebAp(status, status_size);
  return observer_web_ap_running;
}

bool MyMesh::toggleObserverWebStaView(char* status, size_t status_size) {
  if (observer_web_sta_running) {
    stopObserverWebStaView();
    if (status && status_size) snprintf(status, status_size, "View stopped");
    return false;
  }
  startObserverWebStaView(status, status_size);
  return observer_web_sta_running;
}

void MyMesh::getObserverWebApLine(char* dest, size_t dest_size) const {
  if (!dest || dest_size == 0) return;
  if (observer_web_sta_running) {
    if (WiFi.status() == WL_CONNECTED) {
      snprintf(dest, dest_size, "View:on %s", WiFi.localIP().toString().c_str());
    } else {
      snprintf(dest, dest_size, "View:wifi...");
    }
    return;
  }
  if (!observer_web_ap_running) {
    snprintf(dest, dest_size, "AP:off");
    return;
  }
  snprintf(dest, dest_size, "AP:on C:%u %s",
           (unsigned int)WiFi.softAPgetStationNum(),
           WiFi.softAPIP().toString().c_str());
}
#endif

void MyMesh::resetObserverLiveStats() {
  OBSERVER_LOCK();
  observer_rx_packets = 0;
  observer_mqtt_published = 0;
  memset(observer_paths, 0, sizeof(observer_paths));
  memset(observer_last_hops, 0, sizeof(observer_last_hops));
  OBSERVER_UNLOCK();
}

void MyMesh::hibernate() {
#if defined(WITH_MQTT_OBSERVER)
  mqtt_observer.end();
#endif
  radio_driver.powerOff();

#if defined(ESP32) && defined(PIN_USER_BTN)
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_ON);

  rtc_gpio_pullup_en((gpio_num_t)PIN_USER_BTN);
  rtc_gpio_pulldown_dis((gpio_num_t)PIN_USER_BTN);
  rtc_gpio_set_direction((gpio_num_t)PIN_USER_BTN, RTC_GPIO_MODE_INPUT_ONLY);

#if defined(ESP_EXT1_WAKEUP_ANY_LOW)
  esp_sleep_enable_ext1_wakeup(1ULL << PIN_USER_BTN, ESP_EXT1_WAKEUP_ANY_LOW);
#else
  esp_sleep_enable_ext0_wakeup((gpio_num_t)PIN_USER_BTN, 0);
#endif
  esp_deep_sleep_start();
#else
  if (_board) _board->sleep(0);
#endif
}

void MyMesh::applyTempRadioParams(float freq, float bw, uint8_t sf, uint8_t cr, int timeout_mins) {
  set_radio_at = futureMillis(2000); // give CLI reply some time to be sent back, before applying temp radio params
  pending_freq = freq;
  pending_bw = bw;
  pending_sf = sf;
  pending_cr = cr;

  revert_radio_at = futureMillis(2000 + timeout_mins * 60 * 1000); // schedule when to revert radio params
}

bool MyMesh::formatFileSystem() {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  return InternalFS.format();
#elif defined(RP2040_PLATFORM)
  return LittleFS.format();
#elif defined(ESP32)
  return SPIFFS.format();
#else
#error "need to implement file system erase"
  return false;
#endif
}

void MyMesh::sendSelfAdvertisement(int delay_millis, bool flood) {
  mesh::Packet *pkt = createSelfAdvert();
  if (pkt) {
    if (flood) {
      sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);
    } else {
      sendZeroHop(pkt, delay_millis);
    }
  } else {
    MESH_DEBUG_PRINTLN("ERROR: unable to create advertisement packet!");
  }
}

void MyMesh::updateAdvertTimer() {
  if (_prefs.advert_interval > 0) { // schedule local advert timer
    next_local_advert = futureMillis(((uint32_t)_prefs.advert_interval) * 2 * 60 * 1000);
  } else {
    next_local_advert = 0; // stop the timer
  }
}

void MyMesh::updateFloodAdvertTimer() {
  if (_prefs.flood_advert_interval > 0) { // schedule flood advert timer
    next_flood_advert = futureMillis(((uint32_t)_prefs.flood_advert_interval) * 60 * 60 * 1000);
  } else {
    next_flood_advert = 0; // stop the timer
  }
}

void MyMesh::dumpLogFile() {
#if defined(RP2040_PLATFORM)
  File f = _fs->open(PACKET_LOG_FILE, "r");
#else
  File f = _fs->open(PACKET_LOG_FILE);
#endif
  if (f) {
    while (f.available()) {
      int c = f.read();
      if (c < 0) break;
      Serial.print((char)c);
    }
    f.close();
  }
}

void MyMesh::setTxPower(int8_t power_dbm) {
  radio_set_tx_power(power_dbm);
}

#if defined(USE_SX1262) || defined(USE_SX1268)
void MyMesh::setRxBoostedGain(bool enable) {
  radio_driver.setRxBoostedGainMode(enable);
}
#endif

void MyMesh::formatNeighborsReply(char *reply) {
  char *dp = reply;

#if MAX_NEIGHBOURS
  // create copy of neighbours list, skipping empty entries so we can sort it separately from main list
  int16_t neighbours_count = 0;
  NeighbourInfo* sorted_neighbours[MAX_NEIGHBOURS];
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    auto neighbour = &neighbours[i];
    if (neighbour->heard_timestamp > 0) {
      sorted_neighbours[neighbours_count] = neighbour;
      neighbours_count++;
    }
  }

  // sort neighbours newest to oldest
  std::sort(sorted_neighbours, sorted_neighbours + neighbours_count, [](const NeighbourInfo* a, const NeighbourInfo* b) {
    return a->heard_timestamp > b->heard_timestamp; // desc
  });

  for (int i = 0; i < neighbours_count && dp - reply < 134; i++) {
    NeighbourInfo *neighbour = sorted_neighbours[i];

    // add new line if not first item
    if (i > 0) *dp++ = '\n';

    char hex[10];
    // get 4 bytes of neighbour id as hex
    mesh::Utils::toHex(hex, neighbour->id.pub_key, 4);

    // add next neighbour
    uint32_t secs_ago = getRTCClock()->getCurrentTime() - neighbour->heard_timestamp;
    sprintf(dp, "%s:%d:%d", hex, secs_ago, neighbour->snr);
    while (*dp)
      dp++; // find end of string
  }
#endif
  if (dp == reply) { // no neighbours, need empty response
    strcpy(dp, "-none-");
    dp += 6;
  }
  *dp = 0; // null terminator
}

void MyMesh::removeNeighbor(const uint8_t *pubkey, int key_len) {
#if MAX_NEIGHBOURS
  for (int i = 0; i < MAX_NEIGHBOURS; i++) {
    NeighbourInfo *neighbour = &neighbours[i];
    if (memcmp(neighbour->id.pub_key, pubkey, key_len) == 0) {
      neighbours[i] = NeighbourInfo(); // clear neighbour entry
    }
  }
#endif
}

void MyMesh::startRegionsLoad() {
  temp_map.resetFrom(region_map);   // rebuild regions in a temp instance
  memset(load_stack, 0, sizeof(load_stack));
  load_stack[0] = &temp_map.getWildcard();
  region_load_active = true;
}

bool MyMesh::saveRegions() {
  return region_map.save(_fs);
}

void MyMesh::onDefaultRegionChanged(const RegionEntry* r) {
  if (r) {
    region_map.getTransportKeysFor(*r, &default_scope, 1);
  } else {
    memset(default_scope.key, 0, sizeof(default_scope.key));
  }
}

void MyMesh::formatStatsReply(char *reply) {
  StatsFormatHelper::formatCoreStats(reply, board, *_ms, _err_flags, _mgr);
}

void MyMesh::formatRadioStatsReply(char *reply) {
  StatsFormatHelper::formatRadioStats(reply, _radio, radio_driver, getTotalAirTime(), getReceiveAirTime());
}

void MyMesh::formatPacketStatsReply(char *reply) {
  StatsFormatHelper::formatPacketStats(reply, radio_driver, getNumSentFlood(), getNumSentDirect(), 
                                       getNumRecvFlood(), getNumRecvDirect());
}

void MyMesh::saveIdentity(const mesh::LocalIdentity &new_id) {
#if defined(NRF52_PLATFORM) || defined(STM32_PLATFORM)
  IdentityStore store(*_fs, "");
#elif defined(ESP32)
  IdentityStore store(*_fs, "/identity");
#elif defined(RP2040_PLATFORM)
  IdentityStore store(*_fs, "/identity");
#else
#error "need to define saveIdentity()"
#endif
  store.save("_main", new_id);
}

void MyMesh::clearStats() {
  radio_driver.resetStats();
  resetStats();
  ((SimpleMeshTables *)getTables())->resetStats();
}

void MyMesh::handleCommand(uint32_t sender_timestamp, char *command, char *reply) {
  if (region_load_active) {
    if (StrHelper::isBlank(command)) {  // empty/blank line, signal to terminate 'load' operation
      region_map = temp_map;  // copy over the temp instance as new current map
      region_load_active = false;

      sprintf(reply, "OK - loaded %d regions", region_map.getCount());
    } else {
      char *np = command;
      while (*np == ' ') np++;   // skip indent
      int indent = np - command;

      char *ep = np;
      while (RegionMap::is_name_char(*ep)) ep++;
      if (*ep) { *ep++ = 0; }  // set null terminator for end of name

      while (*ep && *ep != 'F') ep++;  // look for (optional) flags

      if (indent > 0 && indent < 8 && strlen(np) > 0) {
        auto parent = load_stack[indent - 1];
        if (parent) {
          auto old = region_map.findByName(np);
          auto nw = temp_map.putRegion(np, parent->id, old ? old->id : 0);  // carry-over the current ID (if name already exists)
          if (nw) {
            nw->flags = old ? old->flags : (*ep == 'F' ? 0 : REGION_DENY_FLOOD);   // carry-over flags from curr

            load_stack[indent] = nw;  // keep pointers to parent regions, to resolve parent_id's
          }
        }
      }
      reply[0] = 0;
    }
    return;
  }

  while (*command == ' ') command++; // skip leading spaces

  if (strlen(command) > 4 && command[2] == '|') { // optional prefix (for companion radio CLI)
    memcpy(reply, command, 3);                    // reflect the prefix back
    reply += 3;
    command += 3;
  }

  // handle ACL related commands
  if (memcmp(command, "setperm ", 8) == 0) {   // format:  setperm {pubkey-hex} {permissions-int8}
    char* hex = &command[8];
    char* sp = strchr(hex, ' ');   // look for separator char
    if (sp == NULL) {
      strcpy(reply, "Err - bad params");
    } else {
      *sp++ = 0;   // replace space with null terminator

      uint8_t pubkey[PUB_KEY_SIZE];
      int hex_len = min(sp - hex, PUB_KEY_SIZE*2);
      if (mesh::Utils::fromHex(pubkey, hex_len / 2, hex)) {
        uint8_t perms = atoi(sp);
        if (acl.applyPermissions(self_id, pubkey, hex_len / 2, perms)) {
          dirty_contacts_expiry = futureMillis(LAZY_CONTACTS_WRITE_DELAY);   // trigger acl.save()
          strcpy(reply, "OK");
        } else {
          strcpy(reply, "Err - invalid params");
        }
      } else {
        strcpy(reply, "Err - bad pubkey");
      }
    }
  } else if (sender_timestamp == 0 && strcmp(command, "get acl") == 0) {
    Serial.println("ACL:");
    for (int i = 0; i < acl.getNumClients(); i++) {
      auto c = acl.getClientByIdx(i);
      if (c->permissions == 0) continue;  // skip deleted (or guest) entries

      Serial.printf("%02X ", c->permissions);
      mesh::Utils::printHex(Serial, c->id.pub_key, PUB_KEY_SIZE);
      Serial.printf("\n");
    }
    reply[0] = 0;
  }
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  else if (strcmp(command, "web.ap on") == 0 || strcmp(command, "web.ap") == 0) {
    startObserverWebAp(reply, 160);
  } else if (strcmp(command, "web.ap off") == 0) {
    stopObserverWebAp();
    strcpy(reply, "AP stopped");
  } else if (strcmp(command, "web.ap status") == 0) {
    if (observer_web_ap_running) {
      snprintf(reply, 160, "AP http://%s ssid:%s", WiFi.softAPIP().toString().c_str(), OBSERVER_WEB_AP_SSID);
    } else {
      strcpy(reply, "AP stopped");
    }
  } else if (strcmp(command, "web.view on") == 0 || strcmp(command, "web.view") == 0) {
    startObserverWebStaView(reply, 160);
  } else if (strcmp(command, "web.view off") == 0) {
    stopObserverWebStaView();
    strcpy(reply, "View stopped");
  } else if (strcmp(command, "web.view status") == 0) {
    if (observer_web_sta_running) {
      if (WiFi.status() == WL_CONNECTED) {
        snprintf(reply, 160, "View http://%s mqtt:off", WiFi.localIP().toString().c_str());
      } else {
        strcpy(reply, "View connecting mqtt:off");
      }
    } else {
      strcpy(reply, "View stopped");
    }
  }
#endif
#ifdef WITH_MQTT_OBSERVER
  else if (strcmp(command, "mqtt on") == 0) {
    setObserverMqttEnabled(true);
    strcpy(reply, "MQTT on");
  } else if (strcmp(command, "mqtt off") == 0) {
    setObserverMqttEnabled(false);
    strcpy(reply, "MQTT off");
  } else if (strcmp(command, "mqtt status") == 0) {
    snprintf(reply, 160, "MQTT:%s running:%u host:%s", getObserverMqttStatus(),
             (unsigned int)mqtt_observer.isRunning(), _prefs.mqtt_host);
  }
#endif
#ifdef ENABLE_OBSERVER_SAVEPOINTS
  else if (strcmp(command, "sp.list") == 0) {
    File f = _fs->open(SAVEPOINT_INDEX_FILE);
    if (!f) {
      strcpy(reply, "SP none");
    } else {
      char *dp = reply;
      char line[OBSERVER_SAVEPOINT_LINE_SIZE];
      uint8_t count = 0;
      while (readLine(f, line, sizeof(line))) {
        unsigned int id = 0;
        unsigned long ts = 0;
        unsigned long rx = 0;
        int nf = 0;
        if (sscanf(line, "%u,%lu,%lu,%d", &id, &ts, &rx, &nf) != 4) continue;

        DateTime dt((uint32_t)ts);
        int written = snprintf(dp, 160 - (dp - reply), "%s%u,%02d%02d,%lu,%d",
                               count ? "\n" : "",
                               id,
                               dt.hour(),
                               dt.minute(),
                               rx,
                               nf);
        if (written < 0 || written >= 160 - (dp - reply)) break;
        dp += written;
        count++;
      }
      f.close();
      if (count == 0) strcpy(reply, "SP none");
    }
  } else if (memcmp(command, "sp.show ", 8) == 0) {
    char* arg = &command[8];
    uint16_t id = (uint16_t)atoi(arg);
    char* sp = strchr(arg, ' ');
    char filename[16];
    formatSavepointFilename(filename, sizeof(filename), id);
    File f = _fs->open(filename);
    if (!f) {
      strcpy(reply, "Err - SP not found");
    } else if (sp == NULL) {
      char datetime[6] = "--:--";
      char rx[12] = "?";
      char mqtt[12] = "?";
      char nf[8] = "?";
      char snr[8] = "?";
      char batt[8] = "?";
      char path[40] = "";
      char heard[40] = "";
      char line[96];

      while (readLine(f, line, sizeof(line))) {
        if (memcmp(line, "meta,datetime_utc,", 18) == 0) {
          char* t = strchr(&line[18], 'T');
          if (t && strlen(t) >= 6) {
            datetime[0] = t[1];
            datetime[1] = t[2];
            datetime[2] = ':';
            datetime[3] = t[4];
            datetime[4] = t[5];
            datetime[5] = 0;
          }
        } else if (memcmp(line, "counter,rx_total,", 17) == 0) {
          StrHelper::strncpy(rx, &line[17], sizeof(rx));
        } else if (memcmp(line, "counter,mqtt_total,", 19) == 0) {
          StrHelper::strncpy(mqtt, &line[19], sizeof(mqtt));
        } else if (memcmp(line, "radio,noise_floor,", 18) == 0) {
          StrHelper::strncpy(nf, &line[18], sizeof(nf));
        } else if (memcmp(line, "radio,last_snr_x4,", 19) == 0) {
          int snr_x4 = atoi(&line[19]);
          formatSnrX4(snr, sizeof(snr), snr_x4);
        } else if (memcmp(line, "power,battery_mv,", 17) == 0) {
          StrHelper::strncpy(batt, &line[17], sizeof(batt));
        } else if (path[0] == 0 && memcmp(line, "path,", 5) == 0) {
          char* value = strchr(&line[5], ',');
          if (value) StrHelper::strncpy(path, value + 1, sizeof(path));
        } else if (heard[0] == 0 && memcmp(line, "heard,", 6) == 0) {
          char* value = strchr(&line[6], ',');
          if (value) StrHelper::strncpy(heard, value + 1, sizeof(heard));
        }
      }
      f.close();

      snprintf(reply, 160, "SP%u %s rx=%s mqtt=%s nf=%s snr=%s bat=%s\nP %s\nH %s",
               (unsigned int)id,
               datetime,
               rx,
               mqtt,
               nf,
               snr,
               batt,
               path[0] ? path : "-",
               heard[0] ? heard : "-");
    } else {
      uint8_t page = (uint8_t)atoi(sp + 1);
      char *dp = reply;
      char line[96];
      uint8_t line_no = 0;
      uint8_t emitted = 0;
      uint8_t page_start = page * 4;
      while (readLine(f, line, sizeof(line))) {
        if (line_no++ < page_start) continue;
        if (emitted >= 4) break;
        int written = snprintf(dp, 160 - (dp - reply), "%s%s", emitted ? "\n" : "", line);
        if (written < 0 || written >= 160 - (dp - reply)) break;
        dp += written;
        emitted++;
      }
      f.close();
      if (emitted == 0) {
        strcpy(reply, "SP EOF");
      }
    }
  } else if (memcmp(command, "sp.delete ", 10) == 0) {
    uint16_t delete_id = (uint16_t)atoi(&command[10]);
    char filename[16];
    formatSavepointFilename(filename, sizeof(filename), delete_id);
    bool removed_file = _fs->remove(filename);
    File in = _fs->open(SAVEPOINT_INDEX_FILE);
    File out = _fs->open("/sp_index.tmp", "w");
    bool removed_index = false;
    if (in && out) {
      char line[OBSERVER_SAVEPOINT_LINE_SIZE];
      while (readLine(in, line, sizeof(line))) {
        unsigned int id = 0;
        if (sscanf(line, "%u,", &id) == 1 && id == delete_id) {
          removed_index = true;
          continue;
        }
        out.println(line);
      }
    }
    if (in) in.close();
    if (out) out.close();
    _fs->remove(SAVEPOINT_INDEX_FILE);
    _fs->rename("/sp_index.tmp", SAVEPOINT_INDEX_FILE);
    strcpy(reply, (removed_file || removed_index) ? "OK - SP deleted" : "Err - SP not found");
  } else if (strcmp(command, "sp.clear") == 0) {
    File f = _fs->open(SAVEPOINT_INDEX_FILE);
    if (f) {
      char line[OBSERVER_SAVEPOINT_LINE_SIZE];
      while (readLine(f, line, sizeof(line))) {
        unsigned int id = 0;
        if (sscanf(line, "%u,", &id) == 1) {
          char filename[16];
          formatSavepointFilename(filename, sizeof(filename), (uint16_t)id);
          _fs->remove(filename);
        }
      }
      f.close();
    }
    _fs->remove(SAVEPOINT_INDEX_FILE);
    strcpy(reply, "OK - SP cleared");
  }
#endif
  else if (memcmp(command, "discover.neighbors", 18) == 0) {
    const char* sub = command + 18;
    while (*sub == ' ') sub++;
    if (*sub != 0) {
      strcpy(reply, "Err - discover.neighbors has no options");
    } else {
      sendNodeDiscoverReq();
      strcpy(reply, "OK - Discover sent");
    }
  } else if (strcmp(command, "diag") == 0) {
    char diag[80];
    char health[80];
    getObserverDiagLine(diag, sizeof(diag));
    getObserverHealthLine(health, sizeof(health));
    snprintf(reply, 160, "%s Up:%lus RX:%lu MQTT:%lu\n%s",
             diag,
             (unsigned long)(millis() / 1000),
             (unsigned long)observer_rx_packets,
             (unsigned long)observer_mqtt_published,
             health);
  } else{
    _cli.handleCommand(sender_timestamp, command, reply);  // common CLI commands
  }
}

void MyMesh::loop() {
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  updateObserverWebMetrics();
  if (observer_web_sta_running && WiFi.status() != WL_CONNECTED && (long)(millis() - observer_web_sta_next_attempt) >= 0) {
    unsigned long now = millis();
    if (observer_web_sta_started_at && (long)(now - observer_web_sta_started_at) >= (long)OBSERVER_WEB_VIEW_FALLBACK_MS) {
      startObserverWebAp(nullptr, 0);
    } else {
      WiFi.disconnect(false);
      WiFi.mode(WIFI_STA);
      WiFi.begin(_prefs.wifi_ssid, _prefs.wifi_password);
      observer_web_sta_next_attempt = now + OBSERVER_WEB_VIEW_RETRY_MS;
    }
  }
#endif
#ifdef WITH_BRIDGE
  bridge.loop();
#endif
#ifdef WITH_MQTT_OBSERVER
  mqtt_observer.loop();
#endif

  mesh::Mesh::loop();

  if (next_flood_advert && millisHasNowPassed(next_flood_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    uint32_t delay_millis = 0;
    if (pkt) sendFloodScoped(default_scope, pkt, delay_millis, _prefs.path_hash_mode + 1);

    updateFloodAdvertTimer(); // schedule next flood advert
    updateAdvertTimer();      // also schedule local advert (so they don't overlap)
  } else if (next_local_advert && millisHasNowPassed(next_local_advert)) {
    mesh::Packet *pkt = createSelfAdvert();
    if (pkt) sendZeroHop(pkt);

    updateAdvertTimer(); // schedule next local advert
  }

  if (set_radio_at && millisHasNowPassed(set_radio_at)) { // apply pending (temporary) radio params
    set_radio_at = 0;                                     // clear timer
    radio_set_params(pending_freq, pending_bw, pending_sf, pending_cr);
    MESH_DEBUG_PRINTLN("Temp radio params");
  }

  if (revert_radio_at && millisHasNowPassed(revert_radio_at)) { // revert radio params to orig
    revert_radio_at = 0;                                        // clear timer
    radio_set_params(_prefs.freq, _prefs.bw, _prefs.sf, _prefs.cr);
    MESH_DEBUG_PRINTLN("Radio params restored");
  }

  // is pending dirty contacts write needed?
  if (dirty_contacts_expiry && millisHasNowPassed(dirty_contacts_expiry)) {
    acl.save(_fs);
    dirty_contacts_expiry = 0;
  }

  // update uptime
  uint32_t now = millis();
  uptime_millis += now - last_millis;
  last_millis = now;
  updateObserverHealth();
}

// To check if there is pending work
bool MyMesh::hasPendingWork() const {
#if defined(WITH_BRIDGE)
  if (bridge.isRunning()) return true;  // bridge needs WiFi radio, can't sleep
#endif
#if defined(ENABLE_OBSERVER_WEB_AP) && defined(ESP32)
  if (observer_web_ap_running || observer_web_sta_running) return true;
#endif
#if defined(WITH_MQTT_OBSERVER)
  if (mqtt_observer.isRunning()) return true;
#endif
  return _mgr->getOutboundTotal() > 0;
}

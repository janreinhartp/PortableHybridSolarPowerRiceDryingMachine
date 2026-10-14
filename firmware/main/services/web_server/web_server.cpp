#include "web_server.h"

#include <cstdio>
#include <cstring>
#include <sys/stat.h>

#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "logger.h"
#include "process_controller.h"
#include "safety_manager.h"
#include "settings_manager.h"

static const char *TAG = "web_server";

/* Default SoftAP credentials (WPA2). Change via future settings if needed. */
static const char *AP_PASS = "ricedryer";
static const int AP_CHANNEL = 1;
static const int AP_MAX_CONN = 4;

static system_status_t *s_status = nullptr;
static httpd_handle_t s_httpd = nullptr;
static bool s_running = false;
static char s_ssid[32] = {};

static const char *DASHBOARD_HTML = R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8"/>
<meta name="viewport" content="width=device-width,initial-scale=1"/>
<title>Rice Dryer</title>
<style>
:root{--bg:#0f1419;--card:#1a2332;--text:#e8eef5;--muted:#8b9bb0;--accent:#3d9bfd;--ok:#3dd68c;--warn:#f5a524;--fault:#f04438}
*{box-sizing:border-box}body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:var(--bg);color:var(--text)}
header{padding:16px 20px;border-bottom:1px solid #2a3544;display:flex;justify-content:space-between;align-items:center;flex-wrap:wrap;gap:8px}
h1{margin:0;font-size:1.25rem}h2{margin:0 0 10px;font-size:1rem;color:var(--muted);font-weight:600}
.grid{display:grid;gap:12px;padding:16px;grid-template-columns:repeat(auto-fit,minmax(240px,1fr))}
.card{background:var(--card);border-radius:10px;padding:14px}
.row{display:flex;justify-content:space-between;gap:8px;padding:4px 0;border-bottom:1px solid #243041;font-size:.95rem}
.row:last-child{border-bottom:0}.k{color:var(--muted)}.v{font-variant-numeric:tabular-nums}
.pill{display:inline-block;padding:4px 10px;border-radius:999px;font-size:.85rem;font-weight:600}
.idle,.complete{background:#243041}.drying,.cooling,.discharging,.precheck{background:#1e3a5f;color:var(--accent)}
.fault{background:#3a1a1a;color:var(--fault)}.ok{color:var(--ok)}.bad{color:var(--fault)}
table{width:100%;border-collapse:collapse;font-size:.85rem}th,td{text-align:left;padding:6px 4px;border-bottom:1px solid #243041}
a{color:var(--accent)}footer{padding:12px 20px;color:var(--muted);font-size:.8rem}
</style>
</head>
<body>
<header>
  <h1>Rice Dryer Monitor</h1>
  <span id="statePill" class="pill idle">—</span>
</header>
<div class="grid">
  <section class="card"><h2>Status</h2><div id="status"></div></section>
  <section class="card"><h2>Sensors</h2><div id="sensors"></div></section>
  <section class="card"><h2>Actuators</h2><div id="actuators"></div></section>
  <section class="card"><h2>Batch</h2><div id="batch"></div></section>
  <section class="card"><h2>Settings</h2><div id="settings"></div></section>
  <section class="card"><h2>Faults</h2><div id="faults"></div></section>
  <section class="card" style="grid-column:1/-1"><h2>History</h2><div id="history"></div></section>
</div>
<footer>Read-only local dashboard · http://192.168.4.1 · refreshes every 2 s</footer>
<script>
const onOff=v=>v?'ON':'OFF';
const okBad=(ok,t)=>`<span class="${ok?'ok':'bad'}">${t}</span>`;
function rows(obj){return Object.entries(obj).map(([k,v])=>`<div class="row"><span class="k">${k}</span><span class="v">${v}</span></div>`).join('')}
async function j(url){const r=await fetch(url);if(!r.ok)throw new Error(url);return r.json()}
async function refresh(){
  try{
    const [st,se,ac,ba,set,hi,fa]=await Promise.all([
      j('/api/status'),j('/api/sensors'),j('/api/actuators'),j('/api/batch'),
      j('/api/settings'),j('/api/history'),j('/api/faults')]);
    const pill=document.getElementById('statePill');
    pill.textContent=st.state||'—';
    pill.className='pill '+String(st.state||'idle').toLowerCase();
    document.getElementById('status').innerHTML=rows({
      State:st.state,Batch:st.batch_id||'—',Moisture:(st.moisture??'—')+' %',
      Temperature:(st.temperature??'—')+' °C',Humidity:(st.humidity??'—')+' %',
      Heater:onOff(st.heater),Fan:onOff(st.fan),Elevator:onOff(st.elevator),
      WiFi:st.wifi_ssid||'—',SD:okBad(st.sd_ok,st.sd_ok?'Ok':'Fail'),
      Logger:okBad(st.logger_ok,st.logger_ok?'Ok':'Fail')});
    document.getElementById('sensors').innerHTML=rows({
      'Hot air °C':se.hot_air_temperature, 'Hot air RH %':se.hot_air_humidity,
      'Chamber °C':se.chamber_temperature,'Chamber RH %':se.chamber_humidity,
      'Moisture %':se.moisture,EC:se.ec,
      'Hot air OK':okBad(se.hot_air_ok,se.hot_air_ok),
      'Chamber OK':okBad(se.chamber_ok,se.chamber_ok),
      'Moisture OK':okBad(se.moisture_ok,se.moisture_ok)});
    document.getElementById('actuators').innerHTML=rows({
      Elevator:onOff(ac.elevator),Heater:onOff(ac.heater),Fan:onOff(ac.fan),
      'Door open':onOff(ac.door_open),'Door close':onOff(ac.door_close)});
    document.getElementById('batch').innerHTML=rows({
      ID:ba.batch_id||'—',State:ba.state,Interrupted:ba.interrupted?'Yes':'No',
      'Target moisture %':ba.target_moisture,'Initial %':ba.initial_moisture,
      'Final %':ba.final_moisture,'Drying s':ba.drying_s,'Cooling s':ba.cooling_s,
      'Discharge s':ba.discharge_s,Started:ba.started||'—',Ended:ba.ended||'—'});
    document.getElementById('settings').innerHTML=rows({
      'Target moisture %':set.target_moisture,'Drying °C':set.drying_temperature,
      'Hysteresis °C':set.temperature_hysteresis,'Cooling °C':set.cooling_temperature,
      Circulation:set.circulation_mode,'Interval s':set.circulation_interval_s,
      'Run s':set.circulation_run_time_s,'Discharge s':set.discharge_time_s});
    const alarms=(fa.alarms||[]);
    document.getElementById('faults').innerHTML=alarms.length?
      '<table><tr><th>Time</th><th>Code</th><th>State</th></tr>'+
      alarms.map(a=>`<tr><td>${a.timestamp||''}</td><td>${a.code||''}</td><td>${a.state||''}</td></tr>`).join('')+
      '</table>':('<div class="row"><span class="k">Active</span><span class="v">'+
      (fa.active||'NONE')+'</span></div>');
    const hist=hi.batches||[];
    document.getElementById('history').innerHTML=hist.length?
      '<table><tr><th>Batch</th><th>Result</th><th>Moisture</th><th>Export</th></tr>'+
      hist.map(b=>{
        const file=encodeURIComponent('batches/'+b.batch_id+'.csv');
        return `<tr><td>${b.batch_id||''}</td><td>${b.completion||''}</td>`+
          `<td>${b.initial_moisture} → ${b.final_moisture} %</td>`+
          `<td><a href="/api/export/${file}">CSV</a></td></tr>`;
      }).join('')+'</table>':'<span class="k">No batches yet</span>';
  }catch(e){console.warn(e)}
}
refresh();setInterval(refresh,2000);
</script>
</body>
</html>)HTML";

static esp_err_t send_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, json, HTTPD_RESP_USE_STRLEN);
}

static const system_status_t *status_snap(void)
{
    return s_status;
}

static esp_err_t handle_root(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    return httpd_resp_send(req, DASHBOARD_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t handle_api_status(httpd_req_t *req)
{
    const system_status_t *st = status_snap();
    if (st == nullptr) {
        return send_json(req, "{\"error\":\"unavailable\"}");
    }
    char buf[768];
    snprintf(buf, sizeof(buf),
             "{\"state\":\"%s\",\"batch_id\":\"%s\",\"moisture\":%.2f,"
             "\"temperature\":%.2f,\"humidity\":%.2f,"
             "\"heater\":%s,\"fan\":%s,\"elevator\":%s,"
             "\"wifi_ssid\":\"%s\",\"sd_ok\":%s,\"logger_ok\":%s,"
             "\"fault\":%u,\"heating_permitted\":%s}",
             process_controller_state_name((dryer_state_t)st->dryer_state),
             st->batch_id[0] ? st->batch_id : "",
             (double)st->moisture_pct, (double)st->chamber_temp_c,
             (double)st->chamber_rh, st->relay_heater ? "true" : "false",
             st->relay_fan ? "true" : "false",
             st->relay_elevator ? "true" : "false", s_ssid,
             st->sd_ok ? "true" : "false", st->logger_ok ? "true" : "false",
             (unsigned)st->active_fault,
             st->heating_permitted ? "true" : "false");
    return send_json(req, buf);
}

static esp_err_t handle_api_sensors(httpd_req_t *req)
{
    const system_status_t *st = status_snap();
    if (st == nullptr) {
        return send_json(req, "{\"error\":\"unavailable\"}");
    }
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"hot_air_temperature\":%.2f,\"hot_air_humidity\":%.2f,"
             "\"chamber_temperature\":%.2f,\"chamber_humidity\":%.2f,"
             "\"moisture\":%.2f,\"ec\":%.2f,"
             "\"hot_air_ok\":%s,\"chamber_ok\":%s,\"moisture_ok\":%s,"
             "\"ec_ok\":%s,\"ads1115_ok\":%s}",
             (double)st->hotair_temp_c, (double)st->hotair_rh,
             (double)st->chamber_temp_c, (double)st->chamber_rh,
             (double)st->moisture_pct, (double)st->ec_value,
             st->hotair_ok ? "true" : "false",
             st->chamber_ok ? "true" : "false",
             st->moisture_ok ? "true" : "false",
             st->ec_ok ? "true" : "false",
             st->ads1115_ok ? "true" : "false");
    return send_json(req, buf);
}

static esp_err_t handle_api_actuators(httpd_req_t *req)
{
    const system_status_t *st = status_snap();
    if (st == nullptr) {
        return send_json(req, "{\"error\":\"unavailable\"}");
    }
    char buf[256];
    snprintf(buf, sizeof(buf),
             "{\"elevator\":%s,\"heater\":%s,\"fan\":%s,"
             "\"door_open\":%s,\"door_close\":%s}",
             st->relay_elevator ? "true" : "false",
             st->relay_heater ? "true" : "false",
             st->relay_fan ? "true" : "false",
             st->relay_door_open ? "true" : "false",
             st->relay_door_close ? "true" : "false");
    return send_json(req, buf);
}

static esp_err_t handle_api_batch(httpd_req_t *req)
{
    const system_status_t *st = status_snap();
    if (st == nullptr) {
        return send_json(req, "{\"error\":\"unavailable\"}");
    }
    char buf[768];
    snprintf(buf, sizeof(buf),
             "{\"batch_id\":\"%s\",\"state\":\"%s\",\"interrupted\":%s,"
             "\"target_moisture\":%.2f,\"initial_moisture\":%.2f,"
             "\"final_moisture\":%.2f,\"drying_s\":%u,\"cooling_s\":%u,"
             "\"discharge_s\":%u,\"started\":\"%s\",\"ended\":\"%s\","
             "\"max_chamber_temp\":%.2f,\"avg_chamber_temp\":%.2f}",
             st->batch_id[0] ? st->batch_id : "",
             process_controller_state_name((dryer_state_t)st->dryer_state),
             st->batch_interrupted ? "true" : "false",
             (double)st->target_moisture_pct, (double)st->initial_moisture_pct,
             (double)st->final_moisture_pct, (unsigned)st->drying_elapsed_s,
             (unsigned)st->cooling_elapsed_s, (unsigned)st->discharge_elapsed_s,
             st->batch_started, st->batch_ended, (double)st->max_chamber_temp_c,
             (double)st->avg_chamber_temp_c);
    return send_json(req, buf);
}

static esp_err_t handle_api_settings(httpd_req_t *req)
{
    dryer_settings_t cfg {};
    settings_manager_get(&cfg);
    char buf[512];
    snprintf(buf, sizeof(buf),
             "{\"target_moisture\":%.2f,\"drying_temperature\":%.2f,"
             "\"temperature_hysteresis\":%.2f,\"cooling_temperature\":%.2f,"
             "\"circulation_mode\":\"%s\",\"circulation_interval_s\":%u,"
             "\"circulation_run_time_s\":%u,\"discharge_time_s\":%u,"
             "\"min_drying_time_s\":%u,\"max_drying_time_s\":%u,"
             "\"max_hot_air_temperature\":%.2f}",
             (double)cfg.target_moisture_pct, (double)cfg.drying_temperature_c,
             (double)cfg.temperature_hysteresis_c, (double)cfg.cooling_temperature_c,
             cfg.circulation_mode == CIRCULATION_MODE_CONTINUOUS ? "continuous"
                                                                 : "interval",
             (unsigned)cfg.circulation_interval_s,
             (unsigned)cfg.circulation_run_time_s, (unsigned)cfg.discharge_time_s,
             (unsigned)cfg.min_drying_time_s, (unsigned)cfg.max_drying_time_s,
             (double)cfg.max_hot_air_temperature_c);
    return send_json(req, buf);
}

static esp_err_t handle_api_history(httpd_req_t *req)
{
    logger_summary_t hist[LOGGER_HISTORY_MAX] {};
    const size_t n = logger_copy_history(hist, LOGGER_HISTORY_MAX);
    /* Bound: ~200 bytes × 8 ≈ 1.6 KB */
    char buf[2048];
    size_t off = 0;
    int wr = snprintf(buf + off, sizeof(buf) - off, "{\"batches\":[");
    if (wr < 0 || (size_t)wr >= sizeof(buf) - off) {
        return send_json(req, "{\"batches\":[]}");
    }
    off += (size_t)wr;

    for (size_t i = 0; i < n; ++i) {
        const logger_summary_t *s = &hist[i];
        wr = snprintf(buf + off, sizeof(buf) - off,
                      "%s{\"batch_id\":\"%s\",\"start_time\":\"%s\","
                      "\"end_time\":\"%s\",\"completion\":\"%s\","
                      "\"initial_moisture\":%.2f,\"final_moisture\":%.2f,"
                      "\"target_moisture\":%.2f,\"fault\":\"%s\"}",
                      i ? "," : "", s->batch_id, s->start_time, s->end_time,
                      s->completion, (double)s->initial_moisture,
                      (double)s->final_moisture, (double)s->target_moisture,
                      s->fault);
        if (wr < 0 || (size_t)wr >= sizeof(buf) - off) {
            break;
        }
        off += (size_t)wr;
    }
    snprintf(buf + off, sizeof(buf) - off, "]}");
    return send_json(req, buf);
}

static esp_err_t handle_api_faults(httpd_req_t *req)
{
    const system_status_t *st = status_snap();
    logger_alarm_t alarms[LOGGER_ALARM_MAX] {};
    const size_t n = logger_copy_alarms(alarms, LOGGER_ALARM_MAX);
    const fault_code_t active =
        st ? (fault_code_t)st->active_fault : FAULT_NONE;

    char buf[2048];
    size_t off = 0;
    int wr = snprintf(buf + off, sizeof(buf) - off,
                      "{\"active\":\"%s\",\"alarms\":[",
                      safety_manager_fault_name(active));
    if (wr < 0 || (size_t)wr >= sizeof(buf) - off) {
        return send_json(req, "{\"active\":\"NONE\",\"alarms\":[]}");
    }
    off += (size_t)wr;

    for (size_t i = 0; i < n; ++i) {
        const logger_alarm_t *a = &alarms[i];
        wr = snprintf(buf + off, sizeof(buf) - off,
                      "%s{\"timestamp\":\"%s\",\"state\":\"%s\","
                      "\"code\":\"%s\",\"detail\":\"%s\"}",
                      i ? "," : "", a->timestamp, a->state, a->code, a->detail);
        if (wr < 0 || (size_t)wr >= sizeof(buf) - off) {
            break;
        }
        off += (size_t)wr;
    }
    snprintf(buf + off, sizeof(buf) - off, "]}");
    return send_json(req, buf);
}

/** Reject path traversal and keep export under /sdcard. */
static bool export_path_ok(const char *rel, char *abs_out, size_t abs_len)
{
    if (rel == nullptr || rel[0] == '\0' || abs_out == nullptr) {
        return false;
    }
    if (strstr(rel, "..") != nullptr || rel[0] == '/' || rel[0] == '\\') {
        return false;
    }
    for (const char *p = rel; *p; ++p) {
        const char c = *p;
        const bool ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                        (c >= '0' && c <= '9') || c == '-' || c == '_' ||
                        c == '/' || c == '.';
        if (!ok) {
            return false;
        }
    }
    int n = snprintf(abs_out, abs_len, "/sdcard/%s", rel);
    return n > 0 && (size_t)n < abs_len;
}

static esp_err_t handle_api_export(httpd_req_t *req)
{
    /* URI: /api/export/<relative path> */
    const char *uri = req->uri;
    const char *prefix = "/api/export/";
    if (strncmp(uri, prefix, strlen(prefix)) != 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "bad path");
        return ESP_FAIL;
    }
    const char *rel = uri + strlen(prefix);

    char decoded[96];
    /* Minimal percent-decode for '/' (%2F) and common safe chars. */
    size_t di = 0;
    for (size_t i = 0; rel[i] != '\0' && di + 1 < sizeof(decoded); ++i) {
        if (rel[i] == '%' && rel[i + 1] && rel[i + 2]) {
            char hex[3] = {rel[i + 1], rel[i + 2], 0};
            decoded[di++] = (char)strtol(hex, nullptr, 16);
            i += 2;
        } else {
            decoded[di++] = rel[i];
        }
    }
    decoded[di] = '\0';

    char abs_path[128];
    if (!export_path_ok(decoded, abs_path, sizeof(abs_path))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid file");
        return ESP_FAIL;
    }

    struct stat st {};
    if (stat(abs_path, &st) != 0 || !S_ISREG(st.st_mode)) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "not found");
        return ESP_FAIL;
    }

    FILE *f = fopen(abs_path, "rb");
    if (f == nullptr) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "open failed");
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "text/csv");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    char disposition[128];
    const char *base = strrchr(decoded, '/');
    base = base ? base + 1 : decoded;
    snprintf(disposition, sizeof(disposition),
             "attachment; filename=\"%s\"", base);
    httpd_resp_set_hdr(req, "Content-Disposition", disposition);

    char chunk[1024];
    esp_err_t err = ESP_OK;
    while (true) {
        const size_t n = fread(chunk, 1, sizeof(chunk), f);
        if (n == 0) {
            break;
        }
        if (httpd_resp_send_chunk(req, chunk, n) != ESP_OK) {
            err = ESP_FAIL;
            break;
        }
    }
    fclose(f);
    httpd_resp_send_chunk(req, nullptr, 0);
    return err;
}

static esp_err_t reject_non_get(httpd_req_t *req)
{
    httpd_resp_set_status(req, "405 Method Not Allowed");
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, "{\"error\":\"read-only\"}", HTTPD_RESP_USE_STRLEN);
}

static void build_ssid(void)
{
    uint8_t mac[6] = {};
    esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    snprintf(s_ssid, sizeof(s_ssid), "RiceDryer-%02X%02X", mac[4], mac[5]);
}

static esp_err_t start_softap(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        return err;
    }

    build_ssid();

    wifi_config_t wifi_config = {};
    strncpy((char *)wifi_config.ap.ssid, s_ssid, sizeof(wifi_config.ap.ssid) - 1);
    wifi_config.ap.ssid_len = strlen(s_ssid);
    strncpy((char *)wifi_config.ap.password, AP_PASS,
            sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.channel = AP_CHANNEL;
    wifi_config.ap.max_connection = AP_MAX_CONN;
    wifi_config.ap.authmode = WIFI_AUTH_WPA2_PSK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP SSID=%s pass=%s IP=192.168.4.1", s_ssid, AP_PASS);
    return ESP_OK;
}

static esp_err_t start_httpd(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 16;
    config.stack_size = 8192;

    esp_err_t err = httpd_start(&s_httpd, &config);
    if (err != ESP_OK) {
        return err;
    }

    const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = handle_root, .user_ctx = nullptr},
        {.uri = "/api/status",
         .method = HTTP_GET,
         .handler = handle_api_status,
         .user_ctx = nullptr},
        {.uri = "/api/sensors",
         .method = HTTP_GET,
         .handler = handle_api_sensors,
         .user_ctx = nullptr},
        {.uri = "/api/actuators",
         .method = HTTP_GET,
         .handler = handle_api_actuators,
         .user_ctx = nullptr},
        {.uri = "/api/batch",
         .method = HTTP_GET,
         .handler = handle_api_batch,
         .user_ctx = nullptr},
        {.uri = "/api/settings",
         .method = HTTP_GET,
         .handler = handle_api_settings,
         .user_ctx = nullptr},
        {.uri = "/api/history",
         .method = HTTP_GET,
         .handler = handle_api_history,
         .user_ctx = nullptr},
        {.uri = "/api/faults",
         .method = HTTP_GET,
         .handler = handle_api_faults,
         .user_ctx = nullptr},
        {.uri = "/api/export/*",
         .method = HTTP_GET,
         .handler = handle_api_export,
         .user_ctx = nullptr},
        /* Explicitly reject writes if a client probes common verbs on root. */
        {.uri = "/*",
         .method = HTTP_POST,
         .handler = reject_non_get,
         .user_ctx = nullptr},
        {.uri = "/*",
         .method = HTTP_PUT,
         .handler = reject_non_get,
         .user_ctx = nullptr},
        {.uri = "/*",
         .method = HTTP_DELETE,
         .handler = reject_non_get,
         .user_ctx = nullptr},
    };

    for (size_t i = 0; i < sizeof(routes) / sizeof(routes[0]); ++i) {
        ESP_ERROR_CHECK(httpd_register_uri_handler(s_httpd, &routes[i]));
    }
    return ESP_OK;
}

esp_err_t web_server_start(system_status_t *status)
{
    if (s_running) {
        return ESP_OK;
    }
    if (status == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    s_status = status;

    esp_err_t err = start_softap();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "SoftAP failed: %s", esp_err_to_name(err));
        return err;
    }

    err = start_httpd();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP server failed: %s", esp_err_to_name(err));
        return err;
    }

    s_running = true;
    ESP_LOGI(TAG, "Phase 8 web dashboard ready at http://192.168.4.1");
    return ESP_OK;
}

bool web_server_is_running(void)
{
    return s_running;
}

void web_server_copy_ssid(char *out, size_t out_len)
{
    if (out == nullptr || out_len == 0) {
        return;
    }
    strncpy(out, s_ssid, out_len - 1);
    out[out_len - 1] = '\0';
}

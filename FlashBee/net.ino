// -- Network: WiFi, OTA, MQTT, Home Assistant discovery, remote log and remote commands --------------------
// Nothing here may block loop() for long (the task watchdog reboots after 8 s, the UI runs at ~20 fps):
//   - WiFi.begin() runs once per attempt; an attempt that fails after WIFI_ATTEMPT_MS backs off 5 s doubling to 60 s.
//   - MQTT: TCP connect <= 1.5 s, CONNACK wait <= 2 s (PubSubClient socket timeout), backoff 5 s doubling to 60 s.
//   - A hostname (not an IP) is resolved in a small task, so a dead DNS server cannot stall the loop.
// With no SSID, or `wifi off`, no radio is started and none of this costs anything.
// Secrets (wPass, otaPass, mPass) are never printed, logged, published or drawn: only "set" / "not set".

// ---------------------------------------------------------------------------------------------------------
// LogPrint: Serial plus a small line ring for <pfx>/log
// ---------------------------------------------------------------------------------------------------------
void LogPrint::pushLine() {
  cur[curLen] = '\0';
  if (count >= LOG_RING_N) { head = (head + 1) % LOG_RING_N; count--; dropped++; }   // full: drop the oldest, never block
  strlcpy(ring[(head + count) % LOG_RING_N], cur, LOG_LINE_MAX);
  count++;
  curLen = 0;
}

size_t LogPrint::write(const uint8_t* buf, size_t n) {
  Serial.write(buf, n);
  if (!toRing) { curLen = 0; return n; }
  for (size_t i = 0; i < n; i++) {
    char c = (char)buf[i];
    if (c == '\r') continue;
    if (c == '\n') { if (curLen) pushLine(); continue; }
    if (curLen < LOG_LINE_MAX - 1) cur[curLen++] = c;
  }
  return n;
}

size_t LogPrint::write(uint8_t c) { return write(&c, 1); }

bool LogPrint::takeLine(char* out, size_t n) {
  if (count == 0) return false;
  strlcpy(out, ring[head], n);
  head = (head + 1) % LOG_RING_N;
  count--;
  return true;
}

// ---------------------------------------------------------------------------------------------------------
// Settings (NVS)
// ---------------------------------------------------------------------------------------------------------
static void netBuildTopics() {
  snprintf(tState, sizeof(tState), "%s/state", ncfg.mPfx);
  snprintf(tStrike, sizeof(tStrike), "%s/strike", ncfg.mPfx);
  snprintf(tLog, sizeof(tLog), "%s/log", ncfg.mPfx);
}

static void netLoadStr(const char* key, char* dst, size_t cap) {
  dst[0] = '\0';
  if (!prefsOk) return;
  size_t n = prefs.getString(key, dst, cap);
  if (n == 0 || n >= cap) dst[0] = '\0';
  dst[cap - 1] = '\0';
}

// Once from setup(), after loadPrefs(): device id, settings, topics. Starts nothing.
void netInit() {
  uint8_t mac[6];
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(devId, sizeof(devId), "%02x%02x%02x", mac[3], mac[4], mac[5]);
  snprintf(devHost, sizeof(devHost), "flashbee-%s", devId);
  memset(&ncfg, 0, sizeof(ncfg));
  netLoadStr("wSsid", ncfg.ssid, sizeof(ncfg.ssid));
  netLoadStr("wPass", ncfg.pass, sizeof(ncfg.pass));
  netLoadStr("otaPass", ncfg.otaPass, sizeof(ncfg.otaPass));
  netLoadStr("mHost", ncfg.mHost, sizeof(ncfg.mHost));
  netLoadStr("mUser", ncfg.mUser, sizeof(ncfg.mUser));
  netLoadStr("mPass", ncfg.mPass, sizeof(ncfg.mPass));
  netLoadStr("mPfx", ncfg.mPfx, sizeof(ncfg.mPfx));
  if (!ncfg.mPfx[0]) snprintf(ncfg.mPfx, sizeof(ncfg.mPfx), "flashbee/%s", devId);
  ncfg.mPort = prefsOk ? prefs.getUShort("mPort", 1883) : 1883;
  if (ncfg.mPort == 0) ncfg.mPort = 1883;
  ncfg.haDisc = prefsOk ? prefs.getUChar("haDisc", 1) : 1;
  ncfg.net = prefsOk ? prefs.getUChar("net", 1) : 1;
  netBuildTopics();
  netMtx = xSemaphoreCreateMutex();
  netCmdQ = xQueueCreate(MQTT_CMD_Q, CON_LINE_MAX + 1);
  mqttSnapshot();
  LOG.printf("[NET] id %s host %s, WiFi %s (SSID %s), MQTT %s\n", devId, devHost, ncfg.net ? "on" : "off",
             ncfg.ssid[0] ? "set" : "not set", ncfg.mHost[0] ? "host set" : "no host");
}

bool netWanted() { return ncfg.net && ncfg.ssid[0]; }

// Store a string setting (RAM and NVS). Empty value removes the key. false = too long.
static bool netSetStr(const char* key, char* dst, size_t cap, const char* val) {
  if (strlen(val) >= cap) { LOG.printf("[CON] err value too long (max %u)\n", (unsigned)(cap - 1)); return false; }
  strlcpy(dst, val, cap);
  if (prefsOk) { if (dst[0]) prefs.putString(key, dst); else prefs.remove(key); }
  return true;
}

static bool prefixOk(const char* v) {                // MQTT topic prefix: letters, digits and / _ . - only
  if (!*v || v[0] == '/' || v[strlen(v) - 1] == '/') return false;
  for (const char* c = v; *c; c++) if (!isalnum((unsigned char)*c) && !strchr("/_.-", *c)) return false;
  return true;
}

// ---------------------------------------------------------------------------------------------------------
// Remote commands: queue (callback and test console) -> loop -> the serial console parser
// ---------------------------------------------------------------------------------------------------------
static void mqttEnqueueCmd(const char* line) {
  char b[CON_LINE_MAX + 1];
  strlcpy(b, line, sizeof(b));
  if (!netCmdQ || xQueueSend(netCmdQ, b, 0) != pdTRUE) LOG.println("[MQTT] cmd queue full - dropped");
}

// PubSubClient callback, runs in the net task. The payload buffer is reused by every publish, so copy the line
// into the queue and do nothing else: the loop runs it.
void mqttCallback(char* topic, uint8_t* payload, unsigned int len) {
  (void)topic;
  if (len == 0 || len > CON_LINE_MAX || !netCmdQ) return;
  char line[CON_LINE_MAX + 1];
  memcpy(line, payload, len);
  line[len] = '\0';
  xQueueSend(netCmdQ, line, 0);
}

// Commands that could cut the device off, hang it or dump the screen are refused when they arrive over MQTT.
bool conMqttAllowed(const char* raw) {
  char t0[12] = "", t1[12] = "";
  int k = 0;
  const char* p = raw;
  for (; k < 2; k++) {
    while (*p == ' ' || *p == '\t') p++;
    char* t = k == 0 ? t0 : t1;
    size_t i = 0;
    while (*p && *p != ' ' && *p != '\t') { if (i < 11) t[i++] = (char)tolower((unsigned char)*p); p++; }
    t[i] = '\0';
  }
  if (!strcmp(t0, "factory") || !strcmp(t0, "hang") || !strcmp(t0, "shot") || !strcmp(t0, "mqttcmd") || !strcmp(t0, "tune")) return false;
  if (!strcmp(t0, "wifi") || !strcmp(t0, "ota")) return t1[0] == '\0';          // status only
  if (!strcmp(t0, "mqtt")) return t1[0] == '\0' || !strcmp(t1, "dump");         // status and dump only
  if (!strcmp(t0, "sleep")) return t1[0] == '\0';                                // status only
  return true;
}

void conRunLine(const char* line, bool fromMqtt) {
  char tmp[CON_LINE_MAX + 1];
  strlcpy(tmp, line, sizeof(tmp));
  conFromMqtt = fromMqtt;
  conExec(tmp);
  conFromMqtt = false;
  memset(tmp, 0, sizeof(tmp));
}

// ---------------------------------------------------------------------------------------------------------
// OTA
// ---------------------------------------------------------------------------------------------------------
static void otaDraw(const char* l1, const char* l2) {
  if (!screenOn) return;
  spr.fillSprite(C_BG);
  spr.drawCircle(CX, CY, 119, C_GOLD);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_GOLD, C_BG);
  spr.setTextSize(2);
  spr.drawString(l1, CX, 78);
  int w = 120;
  spr.drawRect(CX - w / 2 - 1, 108, w + 2, 14, C_GREY);
  spr.fillRect(CX - w / 2, 109, otaPct * w / 100, 12, C_GOLD);
  char b[8];
  snprintf(b, sizeof(b), "%u%%", otaPct);
  spr.setTextSize(2);
  spr.setTextColor(C_WHITE, C_BG);
  spr.drawString(b, CX, 142);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString(l2, CX, 168);
  spr.pushSprite(0, 0);
}

static void otaOnStart() {
  otaActive = true;
  otaPct = 0;
  esp_task_wdt_reset();
  screenWake("ota");
  LOG.printf("[OTA] start (%s)\n", ArduinoOTA.getCommand() == U_FLASH ? "firmware" : "filesystem");
  otaDraw("UPDATING", "DO NOT POWER OFF");
}

static void otaOnProgress(unsigned int done, unsigned int total) {
  esp_task_wdt_reset();                               // ArduinoOTA.handle() blocks until the update ends
  uint8_t pct = total ? (uint8_t)((uint64_t)done * 100 / total) : 0;
  uint32_t now = millis();
  if (pct != otaPct && (now - otaDrawMs >= 100 || pct == 100)) {
    otaPct = pct;
    otaDrawMs = now;
    otaDraw("UPDATING", "DO NOT POWER OFF");
  }
  otaPct = pct;
}

static void otaOnEnd() {
  otaPct = 100;
  LOG.println("[OTA] end - rebooting");
  otaDraw("UPDATED", "REBOOTING");
  otaActive = false;
}

static void otaOnError(ota_error_t err) {
  const char* why = err == OTA_AUTH_ERROR ? "auth failed" : err == OTA_BEGIN_ERROR ? "begin failed"
                  : err == OTA_CONNECT_ERROR ? "connect failed" : err == OTA_RECEIVE_ERROR ? "receive failed"
                  : err == OTA_END_ERROR ? "end failed" : "unknown";
  LOG.printf("[OTA] error: %s\n", why);
  otaActive = false;
}

static void otaStart() {
  ArduinoOTA.setHostname(devHost);
  ArduinoOTA.setPassword(ncfg.otaPass);
  ArduinoOTA.setMdnsEnabled(false);                   // mDNS is ours (started when WiFi comes up)
  ArduinoOTA.setRebootOnSuccess(true);
  ArduinoOTA.onStart(otaOnStart).onProgress(otaOnProgress).onEnd(otaOnEnd).onError(otaOnError);
  ArduinoOTA.begin();
  MDNS.enableArduino(OTA_PORT, true);                 // _arduino._tcp, with auth
  otaRunning = true;
  LOG.printf("[OTA] ready at %s.local:%d (password required)\n", devHost, OTA_PORT);
}

static void otaStop() {
  ArduinoOTA.end();
  otaRunning = false;
  LOG.println("[OTA] stopped");
}

// ---------------------------------------------------------------------------------------------------------
// State, strike and discovery documents
// ---------------------------------------------------------------------------------------------------------
static void distFields(uint8_t d, bool any, char* km, size_t kn, const char** cls) {
  if (!any)                    { snprintf(km, kn, "null"); *cls = "none"; }
  else if (d == DIST_UNKNOWN)  { snprintf(km, kn, "null"); *cls = "unknown"; }
  else if (d == DIST_OOR)      { snprintf(km, kn, "null"); *cls = "out_of_range"; }
  else if (d <= 1)             { snprintf(km, kn, "%u", d); *cls = "overhead"; }
  else                         { snprintf(km, kn, "%u", d); *cls = "km"; }
}

static size_t buildState(char* out, size_t n) {
  uint32_t now = millis();
  const Stats &S = realStats;                         // real data only, never the demo
  bool any = S.strikeCount > 0;
  char dk[12], last[12], clr[12], batp[12], batm[12], rssi[12], en[12];
  const char* dist;
  distFields(S.lastDist, any, dk, sizeof(dk), &dist);
  if (any) { snprintf(last, sizeof(last), "%lu", (unsigned long)((now - S.lastStrikeMs) / 1000UL));
             snprintf(en, sizeof(en), "%lu", (unsigned long)S.lastEnergy); }
  else     { snprintf(last, sizeof(last), "null"); snprintf(en, sizeof(en), "null"); }
  if (S.clearRunning) snprintf(clr, sizeof(clr), "%lu", (unsigned long)((CLEAR_MS - (now - S.clearFromMs)) / 1000UL));
  else                snprintf(clr, sizeof(clr), "null");
  if (battPct >= 0) { snprintf(batp, sizeof(batp), "%d", battPct); snprintf(batm, sizeof(batm), "%lu", (unsigned long)(battMv * 2)); }
  else              { snprintf(batp, sizeof(batp), "null"); snprintf(batm, sizeof(batm), "null"); }
  if (wifiState == WIFI_ST_UP) snprintf(rssi, sizeof(rssi), "%d", WiFi.RSSI());
  else                         snprintf(rssi, sizeof(rssi), "null");
  char why[8];
  AlertStage al = alertCompute(S, now, why, sizeof(why));
  const char* alName = al == ALERT_SHELTER ? "shelter" : al == ALERT_ACTIVITY ? "activity" : "idle";
  const char* sensor = !sensorOk ? "not_found" : sensorLost ? "lost" : "ok";
  return snprintf(out, n,
    "{\"strikes\":%d,\"dist_km\":%s,\"dist\":\"%s\",\"energy\":%s,\"last_strike_s\":%s,\"alert\":\"%s\","
    "\"clear_left_s\":%s,\"noise_fault\":%s,\"sensor\":\"%s\",\"nf\":%u,\"wd\":%u,\"sr\":%u,\"mnl\":%u,"
    "\"profile\":\"%s\",\"profile_mode\":\"%s\",\"battery_pct\":%s,\"battery_mv\":%s,\"rssi\":%s,\"uptime_s\":%lu}",
    S.strikeCount, dk, dist, en, last, alName, clr, noiseFault ? "true" : "false", sensor, noiseFloor, watchdog,
    spikeRej, cfg.mnl, PROFILES[profEff].name, PMODE_NAME[profMode], batp, batm, rssi,
    (unsigned long)(uptimeMs() / 1000ULL));
}

static size_t buildStrike(const StrikeMsg &m, char* out, size_t n) {
  char dk[12];
  const char* dist;
  distFields(m.d, true, dk, sizeof(dk), &dist);
  return snprintf(out, n, "{\"dist_km\":%s,\"dist\":\"%s\",\"energy\":%lu,\"uptime_s\":%lu}", dk, dist,
                  (unsigned long)m.e, (unsigned long)m.upS);
}

static const HaEnt HA_ENTS[HA_DISC_COUNT] = {
  { "sensor", "strikes", "Strikes", "{{ value_json.strikes }}", "\"state_class\":\"total_increasing\",\"icon\":\"mdi:flash\"" },
  { "sensor", "distance", "Distance", "{{ value_json.dist_km }}", "\"device_class\":\"distance\",\"unit_of_measurement\":\"km\",\"state_class\":\"measurement\"" },
  { "sensor", "energy", "Last strike energy", "{{ value_json.energy }}", "\"state_class\":\"measurement\",\"icon\":\"mdi:lightning-bolt\"" },
  { "sensor", "last_strike", "Last strike", "{{ value_json.last_strike_s }}", "\"device_class\":\"duration\",\"unit_of_measurement\":\"s\",\"state_class\":\"measurement\"" },
  { "sensor", "alert", "Alert", "{{ value_json.alert }}", "\"device_class\":\"enum\",\"options\":[\"idle\",\"activity\",\"shelter\"],\"icon\":\"mdi:alert\"" },
  { "sensor", "clear_left", "30-30 countdown", "{{ value_json.clear_left_s }}", "\"device_class\":\"duration\",\"unit_of_measurement\":\"s\",\"state_class\":\"measurement\"" },
  { "sensor", "battery", "Battery", "{{ value_json.battery_pct }}", "\"device_class\":\"battery\",\"unit_of_measurement\":\"%\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"" },
  { "sensor", "rssi", "WiFi signal", "{{ value_json.rssi }}", "\"device_class\":\"signal_strength\",\"unit_of_measurement\":\"dBm\",\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\"" },
  { "sensor", "noise_floor", "Noise floor", "{{ value_json.nf }}", "\"state_class\":\"measurement\",\"entity_category\":\"diagnostic\",\"icon\":\"mdi:waveform\"" },
  { "binary_sensor", "noise_fault", "Noise fault", "{{ 'ON' if value_json.noise_fault else 'OFF' }}", "\"device_class\":\"problem\"" },
  { "binary_sensor", "sensor_ok", "Sensor", "{{ 'ON' if value_json.sensor != 'ok' else 'OFF' }}", "\"device_class\":\"problem\"" },
  { "select", "profile", "Profile", "{{ value_json.profile_mode }}", "\"options\":[\"auto\",\"station\",\"portable\",\"stormchase\"],\"command_topic\":\"~/cmd\",\"command_template\":\"profile {{ value }}\",\"icon\":\"mdi:tune\"" },
};

static size_t buildDisc(int i, char* topic, size_t tn, char* out, size_t on) {
  const HaEnt &e = HA_ENTS[i];
  snprintf(topic, tn, "homeassistant/%s/flashbee_%s/%s/config", e.comp, devId, e.key);
  return snprintf(out, on,
    "{\"name\":\"%s\",\"unique_id\":\"flashbee_%s_%s\",\"has_entity_name\":true,\"~\":\"%s\","
    "\"availability_topic\":\"~/availability\",\"state_topic\":\"~/state\",\"value_template\":\"%s\"%s%s,"
    "\"device\":{\"identifiers\":[\"flashbee_%s\"],\"name\":\"Flash Bee %s\",\"manufacturer\":\"DIY\",\"model\":\"Flash Bee\"}}",
    e.name, devId, e.key, ncfg.mPfx, e.tmpl, e.extra[0] ? "," : "", e.extra, devId, devId);
}

// ---------------------------------------------------------------------------------------------------------
// MQTT: the whole client (DNS, connect, mqtt.loop, every publish) lives in the net task, which is NOT subscribed
// to the task watchdog. A stalled link can block a socket write for 10 s or more; only that task waits for it.
// The loop builds payloads and hands them over by copy through netOutQ; it never touches `mqtt` or `netClient`
// (the loop only calls lwip shutdown() on the published socket fd to unblock the task). The task reads no loop globals: settings come through msnap
// (under netMtx), log lines go back through netLogQ.
// ---------------------------------------------------------------------------------------------------------
static void tlog(const char* fmt, ...) {              // task -> loop log line
  char b[LOG_LINE_MAX];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  if (netLogQ) xQueueSend(netLogQ, b, 0);
}

static void mqttSnapshot() {
  if (!netMtx) return;
  xSemaphoreTake(netMtx, portMAX_DELAY);
  strlcpy(msnap.host, ncfg.mHost, sizeof(msnap.host));
  msnap.port = ncfg.mPort;
  strlcpy(msnap.user, ncfg.mUser, sizeof(msnap.user));
  strlcpy(msnap.pass, ncfg.mPass, sizeof(msnap.pass));
  strlcpy(msnap.avail, ncfg.mPfx, sizeof(msnap.avail));
  strlcat(msnap.avail, "/availability", sizeof(msnap.avail));
  strlcpy(msnap.cmd, ncfg.mPfx, sizeof(msnap.cmd));
  strlcat(msnap.cmd, "/cmd", sizeof(msnap.cmd));
  strlcpy(msnap.clientId, devHost, sizeof(msnap.clientId));
  xSemaphoreGive(netMtx);
}

// MQTT settings changed (console): new snapshot, and the task reconnects.
void mqttCfgChanged() {
  mqttSnapshot();
  mqttResetReq = true;
}

static void netStop() {                               // net task only: the loop never calls a NetworkClient method
  netSockFd = -1;
  netClient.stop();
}

// A call that ran longer than MQTT_STALL_MS: the link is wedged. Close the socket and back off.
static bool netStalled(uint32_t t0, const char* what, uint32_t &nextMs, uint32_t &backoff) {
  uint32_t dt = millis() - t0;
  if (dt <= MQTT_STALL_MS) return false;
  netStop();
  tlog("[MQTT] %s took %lu s - link wedged, closing, retry in %lu s", what, (unsigned long)(dt / 1000UL),
       (unsigned long)(backoff / 1000UL));
  mqttState = MQTT_ST_RETRY;
  nextMs = millis() + backoff;
  backoff = min(backoff * 2, MQTT_BACKOFF_MAX);
  return true;
}

static void netTaskFn(void*) {
  uint32_t nextMs = 0, backoff = MQTT_BACKOFF_MIN;
  IPAddress ip;
  bool ipOk = false;
  uint32_t ipMs = 0;
  char ipHost[NET_HOST_MAX] = "";
  MqttSnap s;
  NetMsg m;
  mqtt.setCallback(mqttCallback);
  mqtt.setSocketTimeout(MQTT_SOCKET_TIMEOUT_S);
  mqtt.setKeepAlive(30);
  mqtt.setBufferSize(MQTT_BUF_SIZE);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(10));
    uint32_t now = millis();
    if (netStallS) {                                    // test hook: block inside the net task
      uint32_t sec = netStallS;
      netStallS = 0;
      netBusySince = now ? now : 1;
      tlog("[MQTT] test: net task blocked for %lu s", (unsigned long)sec);
      vTaskDelay(pdMS_TO_TICKS(sec * 1000UL));
      netBusySince = 0;
      continue;
    }
    if (!netTaskRun) {                                  // WiFi down, or no host: be offline
      if (mqttState == MQTT_ST_UP || netClient.connected()) {
        netBusySince = now ? now : 1;
        if (mqtt.connected()) { mqtt.publish(s.avail, "offline", true); mqtt.disconnect(); }
        netStop();
        netBusySince = 0;
      }
      mqttState = MQTT_ST_OFF;
      while (xQueueReceive(netOutQ, &m, 0) == pdTRUE) {}
      continue;
    }
    if (mqttResetReq) {
      mqttResetReq = false;
      netBusySince = now ? now : 1;
      if (mqtt.connected()) mqtt.disconnect();
      netStop();
      netBusySince = 0;
      ipOk = false;
      backoff = MQTT_BACKOFF_MIN;
      nextMs = now;
      mqttState = MQTT_ST_RETRY;
    }
    if (mqtt.connected()) {
      uint32_t t0 = millis();
      netBusySince = t0 ? t0 : 1;
      mqtt.loop();                                      // PINGREQ, incoming messages
      netBusySince = 0;
      if (netStalled(t0, "mqtt.loop", nextMs, backoff)) continue;
      while (mqtt.connected() && xQueueReceive(netOutQ, &m, 0) == pdTRUE) {
        t0 = millis();
        netBusySince = t0 ? t0 : 1;
        mqtt.publish(m.topic, m.payload, m.len, m.retain != 0);
        netBusySince = 0;
        if (netStalled(t0, "publish", nextMs, backoff)) break;
      }
      if (mqtt.connected()) continue;
    }
    if (mqttState == MQTT_ST_UP) {
      tlog("[MQTT] connection lost");
      netStop();
      mqttState = MQTT_ST_RETRY;
      nextMs = now + MQTT_BACKOFF_MIN;
      continue;
    }
    if ((int32_t)(now - nextMs) < 0) continue;
    xSemaphoreTake(netMtx, portMAX_DELAY);
    s = msnap;
    xSemaphoreGive(netMtx);
    if (!s.host[0]) continue;
    // -- one connection attempt: resolve (blocking is fine here), TCP connect <= 1.5 s, CONNACK <= 2 s
    mqttState = MQTT_ST_RESOLVING;
    uint32_t t0 = millis();
    netBusySince = t0 ? t0 : 1;
    IPAddress target;
    bool have = target.fromString(s.host);
    if (!have && ipOk && now - ipMs < MQTT_DNS_CACHE_MS && !strcmp(ipHost, s.host)) { target = ip; have = true; }
    if (!have) {
      have = WiFi.hostByName(s.host, target) == 1;
      if (have) { ip = target; ipOk = true; ipMs = millis(); strlcpy(ipHost, s.host, sizeof(ipHost)); }
    }
    const char* why = nullptr;
    char w[32];
    if (!have) {
      why = "cannot resolve host";
    } else {
      mqtt.setServer(target, s.port);
      netStop();
      if (!netClient.connect(target, s.port, MQTT_CONNECT_MS)) {
        why = "TCP connect failed";
      } else {
        netSockFd = netClient.fd();                     // published for the loop's stall guard (shutdown only)
        if (!mqtt.connect(s.clientId, s.user[0] ? s.user : nullptr, s.user[0] ? s.pass : nullptr, s.avail, 0, true, "offline")) {
          snprintf(w, sizeof(w), "broker refused (rc %d)", mqtt.state());
          why = w;
        }
      }
    }
    netBusySince = 0;
    if (why) {
      netStop();
      tlog("[MQTT] %s, retry in %lu s", why, (unsigned long)(backoff / 1000UL));
      mqttState = MQTT_ST_RETRY;
      nextMs = millis() + backoff;
      backoff = min(backoff * 2, MQTT_BACKOFF_MAX);
      continue;
    }
    backoff = MQTT_BACKOFF_MIN;
    while (xQueueReceive(netOutQ, &m, 0) == pdTRUE) {}  // anything queued for the old connection is stale
    netBusySince = millis() ? millis() : 1;
    mqtt.subscribe(s.cmd);
    mqtt.publish(s.avail, "online", true);
    netBusySince = 0;
    mqttState = MQTT_ST_UP;
    mqttConnSeq = mqttConnSeq + 1;                                      // the loop sends discovery and state
    tlog("[MQTT] connected to %u.%u.%u.%u:%u as %s", target[0], target[1], target[2], target[3], s.port, s.clientId);
  }
}

void netTaskEnsure() {
  if (netTaskH) return;
  netOutQ = xQueueCreate(NET_OUTQ_N, sizeof(NetMsg));
  netLogQ = xQueueCreate(4, LOG_LINE_MAX);
  if (!netOutQ || !netLogQ || xTaskCreate(netTaskFn, "net", NET_TASK_STACK, NULL, 1, &netTaskH) != pdPASS) {
    LOG.println("[MQTT] cannot start the net task (out of memory)");
    netTaskH = nullptr;
  }
}

static bool netSend(NetMsg &m) {
  return netOutQ && xQueueSend(netOutQ, &m, 0) == pdTRUE;
}

// A real strike (loop context): its own JSON message, then a fresh state.
void mqttQueueStrike(uint32_t e, uint8_t d) {
  if (!netWanted() || !ncfg.mHost[0]) return;
  mqttStateDirty = true;
  if (mqttState != MQTT_ST_UP || !netOutQ || uxQueueSpacesAvailable(netOutQ) == 0) return;   // not connected: dropped
  NetMsg m;
  StrikeMsg sm = { e, d, (uint32_t)(uptimeMs() / 1000ULL) };
  m.len = buildStrike(sm, (char*)m.payload, sizeof(m.payload));
  m.retain = 0;
  strlcpy(m.topic, tStrike, sizeof(m.topic));
  netSend(m);
}

// Loop side, while connected: discovery (one document per pass), state, log lines. Everything is built here and
// copied into netOutQ; a full queue just means "try again next pass".
static void mqttFeed(uint32_t now) {
  if (mqttConnSeq != mqttSeenSeq) {                    // a new connection: announce again
    mqttSeenSeq = mqttConnSeq;
    discIdx = ncfg.haDisc ? 0 : -1;
    discRemove = false;
    mqttStateDirty = true;
  }
  if (mqttState != MQTT_ST_UP || !netOutQ) return;
  NetMsg m;
  UBaseType_t room = uxQueueSpacesAvailable(netOutQ);
  if (discIdx >= 0 && discIdx < HA_DISC_COUNT && room >= 3) {
    size_t n = buildDisc(discIdx, m.topic, sizeof(m.topic), (char*)m.payload, sizeof(m.payload));
    if (discRemove) n = 0;
    if (n >= sizeof(m.payload)) {
      LOG.printf("[MQTT] discovery %s too big (%u bytes)\n", HA_ENTS[discIdx].key, (unsigned)n);
    } else {
      m.len = n; m.retain = 1;
      if (netSend(m)) room--; else return;
    }
    if (++discIdx >= HA_DISC_COUNT) { discIdx = -1; if (!discRemove) LOG.println("[MQTT] discovery queued"); discRemove = false; }
  }
  char why[8];
  AlertStage al = alertCompute(realStats, now, why, sizeof(why));
  uint8_t sensor = !sensorOk ? 2 : sensorLost ? 1 : 0;
  if (al != mqttLastAlert || profEff != mqttLastProf || profMode != mqttLastMode || sensor != mqttLastSensor
      || noiseFault != mqttLastFault) mqttStateDirty = true;
  if ((mqttStateDirty || now - mqttStateMs >= MQTT_STATE_MS) && room >= 1) {
    m.len = buildState((char*)m.payload, sizeof(m.payload));
    m.retain = 1;
    strlcpy(m.topic, tState, sizeof(m.topic));
    if (m.len < sizeof(m.payload) && netSend(m)) {
      room--;
      mqttStateMs = now;
      mqttStateDirty = false;
      mqttLastAlert = al; mqttLastProf = profEff; mqttLastMode = profMode; mqttLastSensor = sensor; mqttLastFault = noiseFault;
    }
  }
  for (int i = 0; i < LOG_PUB_PER_LOOP && room >= 3; i++) {
    char line[LOG_LINE_MAX];
    if (!LOG.takeLine(line, sizeof(line))) break;
    m.len = strlen(line); m.retain = 0;
    memcpy(m.payload, line, m.len);
    strlcpy(m.topic, tLog, sizeof(m.topic));
    if (netSend(m)) room--;
  }
}

// Loop side every pass: take the task's log lines, watch for a wedged task, feed it.
static void mqttTick(uint32_t now) {
  char line[LOG_LINE_MAX];
  while (netLogQ && xQueueReceive(netLogQ, line, 0) == pdTRUE) LOG.println(line);
  static uint32_t stoppedFor = 0;
  uint32_t busy = netBusySince;
  if (busy && now - busy > MQTT_STALL_MS && stoppedFor != busy) {   // the task is stuck in a socket call: close it under its feet
    stoppedFor = busy;
    int fd = netSockFd;
    if (fd >= 0) shutdown(fd, SHUT_RDWR);             // the blocked send/select in the task returns an error; the task closes and backs off
    LOG.println("[MQTT] net task blocked > 10 s: socket shut down by the stall guard");
  }
  mqttFeed(now);
}

// ---------------------------------------------------------------------------------------------------------
// WiFi
// ---------------------------------------------------------------------------------------------------------
static void wifiBegin() {
  if (!wifiRadioOn) {
    WiFi.persistent(false);                           // credentials live in our NVS keys, not the WiFi library's
    WiFi.setHostname(devHost);
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);                     // we drive the retries (backoff), the driver does not
    wifiRadioOn = true;
  }
  WiFi.begin(ncfg.ssid, ncfg.pass[0] ? ncfg.pass : nullptr);
  wifiState = WIFI_ST_CONNECTING;
  wifiAttemptMs = millis();
  LOG.printf("[WIFI] connecting to %s\n", ncfg.ssid);
}

static void netStopAll() {
  if (otaRunning) otaStop();
  netTaskRun = false;                                 // the task sends `offline` and drops the connection (the LWT covers a missed one)
  if (mdnsOk) { MDNS.end(); mdnsOk = false; }
  if (wifiRadioOn) { WiFi.disconnect(true, false); WiFi.mode(WIFI_OFF); wifiRadioOn = false; }
  if (wifiState != WIFI_ST_OFF) LOG.println("[WIFI] radio off");
  wifiState = WIFI_ST_OFF;
}

// Settings changed: reconnect now (next netTick starts a fresh attempt with the new values).
static void wifiRestart() {
  wifiBackoff = WIFI_BACKOFF_MIN;
  if (wifiState != WIFI_ST_OFF) {
    netTaskRun = false;
    if (wifiRadioOn) WiFi.disconnect(false, false);
    wifiState = WIFI_ST_RETRY;                        // begin again in 0.5 s: a begin() right after disconnect() is refused while still connecting
    wifiRetryAt = millis() + 500;
  }
}

void netTick(uint32_t now) {
  char line[CON_LINE_MAX + 1];
  if (netCmdQ && xQueueReceive(netCmdQ, line, 0) == pdTRUE) conRunLine(line, true);   // remote commands, one per pass
  bool want = netWanted();
  bool ring = want && ncfg.mHost[0];
  if (!ring && LOG.toRing) { LOG.count = 0; LOG.curLen = 0; }   // log publishing switched off: drop what was waiting
  LOG.toRing = ring;
  if (!want) { if (wifiState != WIFI_ST_OFF || wifiRadioOn) netStopAll(); mqttTick(now); return; }
  switch (wifiState) {
    case WIFI_ST_OFF:
      wifiBegin();
      break;
    case WIFI_ST_CONNECTING:
      if (WiFi.status() == WL_CONNECTED) {
        wifiState = WIFI_ST_UP;
        wifiBackoff = WIFI_BACKOFF_MIN;
        LOG.printf("[WIFI] connected to %s, IP %s, RSSI %d\n", ncfg.ssid, WiFi.localIP().toString().c_str(), WiFi.RSSI());
        if (!mdnsOk && MDNS.begin(devHost)) { mdnsOk = true; LOG.printf("[WIFI] mDNS %s.local\n", devHost); }
      } else if (now - wifiAttemptMs >= WIFI_ATTEMPT_MS) {
        WiFi.disconnect(false, false);
        wifiState = WIFI_ST_RETRY;
        wifiRetryAt = now + wifiBackoff;
        LOG.printf("[WIFI] not connected after %lu s (status %d), retry in %lu s\n", (unsigned long)(WIFI_ATTEMPT_MS / 1000UL),
                   (int)WiFi.status(), (unsigned long)(wifiBackoff / 1000UL));
        wifiBackoff = min(wifiBackoff * 2, WIFI_BACKOFF_MAX);
      }
      break;
    case WIFI_ST_UP:
      if (WiFi.status() != WL_CONNECTED) {
        LOG.println("[WIFI] connection lost");
        netTaskRun = false;
        wifiState = WIFI_ST_RETRY;
        wifiRetryAt = now + WIFI_BACKOFF_MIN;
        break;
      }
      if (otaRestart) { if (otaRunning) otaStop(); otaRestart = false; }
      if (ncfg.otaPass[0] && !otaRunning) otaStart();
      else if (!ncfg.otaPass[0] && otaRunning) otaStop();
      if (otaRunning) ArduinoOTA.handle();
      netTaskRun = ncfg.mHost[0] != 0;
      if (netTaskRun) netTaskEnsure();
      mqttTick(now);
      break;
    case WIFI_ST_RETRY:
      if ((int32_t)(now - wifiRetryAt) >= 0) wifiBegin();
      break;
  }
}

// ---------------------------------------------------------------------------------------------------------
// Nerd page rows
// ---------------------------------------------------------------------------------------------------------
void netNerdRows(int y, int dy) {
  char lb[16], b[28];
  uint32_t now = millis();
  uint16_t col = C_WHITE;
  snprintf(lb, sizeof(lb), "WIFI");
  if (!ncfg.net)               { snprintf(b, sizeof(b), "off"); col = C_DIM; }
  else if (!ncfg.ssid[0])      { snprintf(b, sizeof(b), "off (no SSID)"); col = C_DIM; }
  else if (wifiState == WIFI_ST_UP) {
    IPAddress ip = WiFi.localIP();
    snprintf(lb, sizeof(lb), "WIFI %ddBm", WiFi.RSSI());
    snprintf(b, sizeof(b), "%u.%u.%u.%u", ip[0], ip[1], ip[2], ip[3]);
  }
  else if (wifiState == WIFI_ST_RETRY) {
    snprintf(b, sizeof(b), "retry %lus", (unsigned long)(((int32_t)(wifiRetryAt - now) > 0 ? wifiRetryAt - now : 0) / 1000UL));
    col = C_ORANGE;
  }
  else                         { snprintf(b, sizeof(b), "connecting"); col = C_ORANGE; }
  drawNerdRow(y, lb, b, col);
  y += dy;
  col = C_WHITE;
  if (!netWanted())            { snprintf(b, sizeof(b), "off"); col = C_DIM; }
  else if (!ncfg.mHost[0])     { snprintf(b, sizeof(b), "off (no host)"); col = C_DIM; }
  else if (wifiState != WIFI_ST_UP) { snprintf(b, sizeof(b), "waiting for WiFi"); col = C_DIM; }
  else if (mqttState == MQTT_ST_UP) snprintf(b, sizeof(b), "connected");
  else                         { snprintf(b, sizeof(b), "connecting"); col = C_ORANGE; }
  drawNerdRow(y, "MQTT", b, col);
}

// ---------------------------------------------------------------------------------------------------------
// Console: wifi, ota, mqtt, ha (handlers print their own errors; true = print "[CON] ok")
// ---------------------------------------------------------------------------------------------------------
static void wifiStatusLine() {
  const char* st = !ncfg.net ? "off (switched off)" : !ncfg.ssid[0] ? "off (no SSID)" : WIFI_NAME[wifiState];
  char ip[16] = "-", rs[8] = "-";
  if (wifiState == WIFI_ST_UP) {
    IPAddress a = WiFi.localIP();
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", a[0], a[1], a[2], a[3]);
    snprintf(rs, sizeof(rs), "%d", WiFi.RSSI());
  }
  LOG.printf("[WIFI] net=%s state=%s ssid=%s pass=%s ip=%s rssi=%s host=%s heap=%lu\n", ncfg.net ? "on" : "off", st,
             ncfg.ssid[0] ? ncfg.ssid : "(none)", ncfg.pass[0] ? "set" : "not set", ip, rs, devHost,
             (unsigned long)ESP.getFreeHeap());
}

bool conWifi(int n, char** tok) {
  if (n == 1) { wifiStatusLine(); return true; }
  const char* sub = tok[1];
  const char* rest = conRest(conRaw, 2);
  if (!strcasecmp(sub, "ssid") && *rest) {
    if (!netSetStr("wSsid", ncfg.ssid, sizeof(ncfg.ssid), rest)) return false;
    wifiRestart();
    LOG.printf("[WIFI] SSID set (%s)\n", ncfg.net ? "reconnecting" : "WiFi is off");
    return true;
  }
  if (!strcasecmp(sub, "pass") && *rest) {
    if (!netSetStr("wPass", ncfg.pass, sizeof(ncfg.pass), rest)) return false;
    wifiRestart();
    LOG.println("[WIFI] password set");
    return true;
  }
  if (n == 2 && !strcasecmp(sub, "on")) {
    ncfg.net = 1;
    if (prefsOk) prefs.putUChar("net", 1);
    LOG.println(ncfg.ssid[0] ? "[WIFI] on" : "[WIFI] on, but no SSID set: radio stays off");
    return true;
  }
  if (n == 2 && !strcasecmp(sub, "off")) {
    ncfg.net = 0;
    if (prefsOk) prefs.putUChar("net", 0);
    LOG.println("[WIFI] off");
    return true;
  }
  if (n == 2 && !strcasecmp(sub, "forget")) {
    netSetStr("wSsid", ncfg.ssid, sizeof(ncfg.ssid), "");
    netSetStr("wPass", ncfg.pass, sizeof(ncfg.pass), "");
    wifiRestart();
    LOG.println("[WIFI] SSID and password cleared");
    return true;
  }
  LOG.println("[CON] err wifi | wifi ssid <name> | wifi pass <password> | wifi on|off | wifi forget");
  return false;
}

bool conOta(int n, char** tok) {
  const char* rest = conRest(conRaw, 2);
  if (n == 1) {
    LOG.printf("[OTA] pass=%s service=%s host=%s port=%d\n", ncfg.otaPass[0] ? "set" : "not set",
               otaRunning ? "running" : (ncfg.otaPass[0] ? "waiting for WiFi" : "off (no password)"), devHost, OTA_PORT);
    return true;
  }
  if (!strcasecmp(tok[1], "pass") && *rest) {
    if (!netSetStr("otaPass", ncfg.otaPass, sizeof(ncfg.otaPass), rest)) return false;
    otaRestart = true;
    LOG.println("[OTA] password set (service restarts)");
    return true;
  }
  if (n == 2 && !strcasecmp(tok[1], "forget")) {
    netSetStr("otaPass", ncfg.otaPass, sizeof(ncfg.otaPass), "");
    otaRestart = true;
    LOG.println("[OTA] password cleared: OTA is off");
    return true;
  }
  LOG.println("[CON] err ota | ota pass <password> | ota forget");
  return false;
}

static void mqttStatusLine() {
  const char* st = !netWanted() ? "off (WiFi off)" : !ncfg.mHost[0] ? "off (no host)"
                 : mqttState == MQTT_ST_UP ? "connected" : mqttState == MQTT_ST_RESOLVING ? "resolving"
                 : wifiState != WIFI_ST_UP ? "waiting for WiFi" : "connecting";
  LOG.printf("[MQTT] state=%s host=%s port=%u user=%s pass=%s prefix=%s ha=%s logq=%u dropped=%lu\n", st,
             ncfg.mHost[0] ? ncfg.mHost : "(none)", ncfg.mPort, ncfg.mUser[0] ? ncfg.mUser : "(none)",
             ncfg.mPass[0] ? "set" : "not set", ncfg.mPfx, ncfg.haDisc ? "on" : "off", LOG.count, (unsigned long)LOG.dropped);
}

// Print the state document and every discovery document with their sizes, so the JSON can be checked offline.
static void mqttDump() {
  char buf[MQTT_BUF_SIZE], topic[NET_TOPIC_MAX + 40];
  Serial.setTxTimeoutMs(300);                         // long lines: wait for USB instead of dropping them
  size_t n = buildState(buf, sizeof(buf));
  LOG.printf("[MQTT] state %s (%u bytes)\n%s\n", tState, (unsigned)n, buf);
  for (int i = 0; i < HA_DISC_COUNT; i++) {
    esp_task_wdt_reset();
    n = buildDisc(i, topic, sizeof(topic), buf, sizeof(buf));
    bool fits = 5 + 2 + strlen(topic) + n <= MQTT_BUF_SIZE;
    LOG.printf("[MQTT] disc %s (%u bytes, %s)\n%s\n", topic, (unsigned)n, fits ? "fits" : "TOO BIG", buf);
  }
  StrikeMsg m = { 123456, 14, 77 };
  n = buildStrike(m, buf, sizeof(buf));
  LOG.printf("[MQTT] strike %s (%u bytes)\n%s\n", tStrike, (unsigned)n, buf);
  Serial.setTxTimeoutMs(0);
}

bool conMqtt(int n, char** tok) {
  const char* sub = n > 1 ? tok[1] : "";
  const char* rest = conRest(conRaw, 2);
  if (n == 1) { mqttStatusLine(); return true; }
  if (!strcasecmp(sub, "dump")) { mqttDump(); return true; }
  if (!strcasecmp(sub, "forget")) {
    netSetStr("mHost", ncfg.mHost, sizeof(ncfg.mHost), "");
    netSetStr("mUser", ncfg.mUser, sizeof(ncfg.mUser), "");
    netSetStr("mPass", ncfg.mPass, sizeof(ncfg.mPass), "");
    mqttCfgChanged();
    LOG.println("[MQTT] host, user and password cleared");
    return true;
  }
  if (*rest) {
    if (!strcasecmp(sub, "host"))      { if (!netSetStr("mHost", ncfg.mHost, sizeof(ncfg.mHost), rest)) return false; }
    else if (!strcasecmp(sub, "user")) { if (!netSetStr("mUser", ncfg.mUser, sizeof(ncfg.mUser), rest)) return false; }
    else if (!strcasecmp(sub, "pass")) { if (!netSetStr("mPass", ncfg.mPass, sizeof(ncfg.mPass), rest)) return false; }
    else if (!strcasecmp(sub, "port")) {
      long v;
      if (!parseInt(rest, 1, 65535, v)) { LOG.println("[CON] err mqtt port 1..65535"); return false; }
      ncfg.mPort = (uint16_t)v;
      if (prefsOk) prefs.putUShort("mPort", ncfg.mPort);
    }
    else if (!strcasecmp(sub, "prefix")) {
      if (!strcasecmp(rest, "reset")) {
        snprintf(ncfg.mPfx, sizeof(ncfg.mPfx), "flashbee/%s", devId);
        if (prefsOk) prefs.remove("mPfx");
      } else if (!prefixOk(rest)) {
        LOG.println("[CON] err prefix: letters, digits and / _ . - only, no leading or trailing /");
        return false;
      } else if (!netSetStr("mPfx", ncfg.mPfx, sizeof(ncfg.mPfx), rest)) return false;
      netBuildTopics();
    }
    else { LOG.println("[CON] err mqtt host|port|user|pass|prefix <value> | mqtt forget | mqtt dump"); return false; }
    mqttCfgChanged();
    LOG.printf("[MQTT] %s set%s\n", sub, strcasecmp(sub, "pass") ? "" : " (not shown)");
    return true;
  }
  LOG.println("[CON] err mqtt host|port|user|pass|prefix <value> | mqtt forget | mqtt dump");
  return false;
}

bool conHa(int n, char** tok) {
  if (n == 1) { LOG.printf("[HA] discovery %s\n", ncfg.haDisc ? "on" : "off"); return true; }
  if (n == 2 && (!strcasecmp(tok[1], "on") || !strcasecmp(tok[1], "off"))) {
    bool on = !strcasecmp(tok[1], "on");
    ncfg.haDisc = on;
    if (prefsOk) prefs.putUChar("haDisc", on);
    if (mqttState == MQTT_ST_UP) { discIdx = 0; discRemove = !on; }   // on: publish now. off: remove the entities from HA
    LOG.printf("[HA] discovery %s\n", on ? "on" : "off");
    return true;
  }
  LOG.println("[CON] err ha on|off");
  return false;
}

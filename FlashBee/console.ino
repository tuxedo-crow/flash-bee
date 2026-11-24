// -- Serial test console and health line ---------------------------------------------------
// Line commands over USB serial for automated stress tests (see README "Test console").
// Every command prints "[CON] ok <cmd>" or "[CON] err <reason>". Nothing here blocks loop()
// except `hang` (on purpose) and the `shot` dump. Events go through the same handlers as real
// input: onRealStrike, onDisturber, onNoiseHigh, dispatchTouch, setGain/setUnits/setBright.

static void conOk(const char* cmd)  { LOG.printf("[CON] ok %s\n", cmd); }
static void conErr(const char* why) { LOG.printf("[CON] err %s\n", why); }
static void conErrMqtt() { LOG.println("[CON] not allowed over MQTT"); }

// Whole-string integer parse within [lo, hi].
static bool parseInt(const char* s, long lo, long hi, long &out) {
  if (!s || !*s) return false;
  char* end;
  long v = strtol(s, &end, 10);
  if (*end != '\0' || v < lo || v > hi) return false;
  out = v;
  return true;
}

static bool validDist(long km) {
  if (km == DIST_OOR || km == DIST_UNKNOWN) return true;
  for (size_t i = 0; i < sizeof(DIST_CODES); i++) if (DIST_CODES[i] == km) return true;
  return false;
}

// The text after the first `skip` words of the raw line (spaces kept): wifi / ota / mqtt values may contain spaces.
static const char* conRest(const char* raw, int skip) {
  const char* p = raw;
  for (int i = 0; i < skip; i++) {
    while (*p == ' ' || *p == '\t') p++;
    while (*p && *p != ' ' && *p != '\t') p++;
  }
  while (*p == ' ' || *p == '\t') p++;
  return p;
}

static void conHelp() {
  LOG.println("[CON] commands: help status strike <km> <energy> | burst <n> <ms>|stop | dist noise | mute on|off");
  LOG.println("[CON] page <0-5> next prev | tap <x> <y> | swipe l|r | hold <x> <y> | demo on|off");
  LOG.println("[CON] set gain in|out | set units mi|km | set bright 0|1|2 | set rot 0-3 | records resetrec");
  LOG.println("[CON] set mnl 0-3 | set scr 0-5 (30s 1m 2m 5m 10m never) | resetfilters  (set gain / set mnl: live override, not saved)");
  LOG.println("[CON] profile [auto|station|portable|stormchase]  (stormchase is never saved)");
  LOG.println("[CON] wifi [ssid <name>|pass <pw>|on|off|forget] | ota [pass <pw>|forget] | ha [on|off]");
  LOG.println("[CON] mqtt [host|port|user|pass|prefix <value>|forget|dump] | mqttcmd <line> (test: run a line as if from MQTT)");
  LOG.println("[CON] tune  (sweep the antenna cap on the IRQ wire, save it, ~4 s)");
  LOG.println("[CON] sleep | sleep on|off | sleep usb on|off  (light sleep while the screen is off; usb on = test: sleep with a USB host; not saved)");
  LOG.println("[CON] fault i2c on|off | fault nh | fault cfg | fault netstall <s>");
  LOG.println("[CON] diag reset | factory flush hang shot rst");
  LOG.println("[CON] km codes: 0 (unknown) 1 5 6 8 10 12 14 17 20 24 27 31 34 37 40 63 (out of range); energy 0..2097151");
}

static void conRecords() {
  LOG.printf("[REC] closest %u km, energy %lu, storm max %lu, total %lu, storms %lu\n", rec.closestKm,
                (unsigned long)rec.maxEnergy, (unsigned long)rec.stormMax, (unsigned long)rec.total,
                (unsigned long)rec.storms);
}

static void conStatus() {
  uint32_t now = millis();
  LOG.printf("[STATUS] profile=%s(%s) wifi=%s mqtt=%s\n", PMODE_NAME[profMode], PROFILES[profEff].name,
                !netWanted() ? "off" : WIFI_NAME[wifiState], mqttState == MQTT_ST_UP ? "up" : "down");
  LOG.printf("[STATUS] page=%u(%s) demo=%d sensorOk=%d bus=%s gain=%s units=%s bright=%u rot=%u NF=%u WD=%u SR=%u\n",
                (unsigned)curPage, pageName(curPage), demoActive, sensorOk,
                sensorBus == BUS_SD ? "SD" : sensorBus == BUS_WIRE ? "D4D5" : "none", cfg.indoor ? "in" : "out",
                cfg.miles ? "mi" : "km", cfg.bright, cfg.rot, noiseFloor, watchdog, spikeRej);
  LOG.printf("[STATUS] alert=%s mnl=%u scr=%u(%s) screen=%s sensorLost=%d i2cFails=%u noiseFault=%d faultI2c=%d\n",
                alertName(alertStage), cfg.mnl, cfg.scr, scrName(), screenOn ? "on" : "off", sensorLost, i2cFails,
                noiseFault, faultI2c);
  int n = 0;
  bool capped = false;
  Trend tr = stormStats(realStats, now, n, capped);
  LOG.printf("[STATUS] real strikes=%d lastDist=%u lastEnergy=%lu hist=%d in15m=%d%s trend=%s\n",
                realStats.strikeCount, realStats.lastDist, (unsigned long)realStats.lastEnergy,
                realStats.histCount, n, capped ? "+" : "", tr == TREND_NONE ? "none" : trendText(tr));
  long left = 0;
  if (realStats.clearRunning) { uint32_t el = now - realStats.clearFromMs; left = el >= CLEAR_MS ? 0L : (long)((CLEAR_MS - el) / 1000UL); }
  LOG.printf("[STATUS] allclear running=%d show=%d remaining=%lds muted=%d\n", realStats.clearRunning,
                realStats.clearDoneShow, left, sensorMuted);
  LOG.printf("[STATUS] antenna tunCap=%u lcoHz=%lu saved=%s\n", tunCap, (unsigned long)lcoHz,
                tunSaved == 0xFF ? "no" : "yes");
  LOG.printf("[STATUS] records closest=%u energy=%lu stormMax=%lu total=%lu storms=%lu\n", rec.closestKm,
                (unsigned long)rec.maxEnergy, (unsigned long)rec.stormMax, (unsigned long)rec.total,
                (unsigned long)rec.storms);
  sleepStatus("[STATUS]");
  LOG.printf("[STATUS] build %s %s\n", BUILD_GIT, BUILD_DATE);
  LOG.printf("[STATUS] diag boot=%lu crash=%lu lastCrash=%u(%s) heap=%lu burstLeft=%lu\n",
                (unsigned long)bootCount, (unsigned long)crashCount, lastCrash,
                lastCrash ? resetName(lastCrash) : "none", (unsigned long)ESP.getFreeHeap(),
                (unsigned long)burstLeft);
  if (crumbSaved.magic == CRUMB_MAGIC) { char cb[96]; crumbText(crumbSaved, cb, sizeof(cb)); LOG.printf("[STATUS] crumb %s\n", cb); }
  else LOG.println("[STATUS] crumb none");
}

// Screenshot: redraw the current page into the sprite like the next frame, dump it as RGB565 hex.
static void conShot() {
  static char row[240 * 4 + 1];
  CRUMB(CS_SHOT);
  drawUIFrame();                                   // same renderer as the loop, without the push
  Serial.setTxTimeoutMs(200);
  Serial.println("[SHOT] begin 240 240");
  for (int y = 0; y < 240; y++) {
    esp_task_wdt_reset();
    for (int x = 0; x < 240; x++) {
      snprintf(row + x * 4, 5, "%04X", y < (int)sprH ? (unsigned)spr.readPixel(x, y) : 0u);   // rows the sprite lacks are black on the panel
    }
    Serial.printf("[SHOT] %d ", y);
    Serial.write((const uint8_t*)row, 240 * 4);
    Serial.println();
  }
  Serial.println("[SHOT] end");
  Serial.setTxTimeoutMs(1);
}

static void injectStrike(uint32_t e, uint8_t d) {
  LOG.printf("[CON] strike dist=%u energy=%lu%s\n", d, (unsigned long)e, d == DIST_UNKNOWN ? " (unknown distance)" : "");
  onRealStrike(e, d);
}

static void conGesture(TouchEvt type, int x, int y) {
  screenWake("console");                           // a gesture draws: wake first
  TouchEvent ev = { type, (int16_t)x, (int16_t)y, millis() };
  lastTouchMs = ev.ms;                             // a real touch resets the 2-min idle timer too
  dispatchTouch(ev);
}

static void conExecInner(char* line) {
  char* save = nullptr;
  char* tok[5];
  int n = 0;
  for (char* t = strtok_r(line, " \t", &save); t && n < 5; t = strtok_r(nullptr, " \t", &save)) tok[n++] = t;
  if (n == 0) return;
  const char* c = tok[0];
  long a = 0, b = 0;
  #define IS(s) (strcasecmp(c, s) == 0)

  if (IS("help")) { conHelp(); conOk("help"); }
  else if (IS("status")) { conStatus(); conOk("status"); }
  else if (IS("strike")) {
    if (n != 3 || !parseInt(tok[1], 0, 63, a) || !validDist(a)) { conErr("strike <km code> <energy 0..2097151>"); return; }
    if (!parseInt(tok[2], 0, 2097151, b)) { conErr("energy out of range 0..2097151"); return; }
    injectStrike((uint32_t)b, (uint8_t)a);
    conOk("strike");
  }
  else if (IS("burst")) {
    if (n == 2 && strcasecmp(tok[1], "stop") == 0) {
      burstLeft = 0;
      conOk("burst stop");
    } else if (n == 3 && parseInt(tok[1], 1, BURST_MAX_N, a) && parseInt(tok[2], BURST_MIN_MS, 3600000, b)) {
      burstLeft = (uint32_t)a;
      burstIntervalMs = (uint32_t)b;
      burstNextMs = millis();
      conOk("burst");
    } else conErr("burst <n 1..100000> <interval_ms >=50> | burst stop");
  }
  else if (IS("dist")) {
    if (!sensorOk) { conErr("no sensor"); return; }
    onDisturber(); conOk("dist");
  }
  else if (IS("mute")) {
    if (n == 2 && !strcmp(tok[1], "on"))       sensorMuted = true;
    else if (n == 2 && !strcmp(tok[1], "off")) sensorMuted = false;
    else { conErr("mute on|off"); return; }
    conOk("mute");
  }
  else if (IS("tune")) {
#if USE_IRQ_TUNING
    if (!sensorOk || sensorLost) { conErr("no sensor"); return; }
    initSensor(true);                              // preset, gain, sweep + save, RCO calibration, thresholds
    if (!tuneOk) { conErr("no pulses on IRQ, kept cap"); return; }
    LOG.printf("[CON] tune cap %u, %lu Hz (%.1f%%), saved\n", tunCap, (unsigned long)lcoHz,
                  100.0 * ((double)lcoHz - 500000.0) / 500000.0);
    conOk("tune");
#else
    conErr("tuning off (USE_IRQ_TUNING 0)");
#endif
  }
  else if (IS("sleep")) {
    bool on2 = n == 2 && !strcasecmp(tok[1], "on"), off2 = n == 2 && !strcasecmp(tok[1], "off");
    bool usb = n == 3 && !strcasecmp(tok[1], "usb") && (!strcasecmp(tok[2], "on") || !strcasecmp(tok[2], "off"));
    if (n == 1) sleepStatus("[SLEEP]");
    else if (on2 || off2) { sleepEnabled = on2; LOG.printf("[SLEEP] %s (not saved)\n", on2 ? "on" : "off"); }
    else if (usb) {
      sleepUsb = !strcasecmp(tok[2], "on");
      LOG.println(sleepUsb ? "[SLEEP] usb override on: sleeps with a USB host too, serial is unreliable until `sleep usb off` (not saved)"
                           : "[SLEEP] usb override off");
    }
    else { conErr("sleep | sleep on|off | sleep usb on|off"); return; }
    conOk("sleep");
  }
  else if (IS("noise")) {
    if (!sensorOk) { conErr("no sensor"); return; }
    onNoiseHigh(); conOk("noise");
  }
  else if (IS("page")) {
    if (n != 2 || !parseInt(tok[1], 0, PAGE_COUNT - 1, a)) { conErr("page <0-5>"); return; }
    screenWake("console"); lastTouchMs = millis(); setPage((Page)a, "console"); conOk("page");
  }
  else if (IS("next")) { conGesture(EV_SWIPE_LEFT, 200, 120); conOk("next"); }
  else if (IS("prev")) { conGesture(EV_SWIPE_RIGHT, 40, 120); conOk("prev"); }
  else if (IS("tap")) {
    if (n != 3 || !parseInt(tok[1], 0, 239, a) || !parseInt(tok[2], 0, 239, b)) { conErr("tap <x 0-239> <y 0-239>"); return; }
    conGesture(EV_TAP, a, b); conOk("tap");
  }
  else if (IS("swipe")) {
    if (n == 2 && strcasecmp(tok[1], "l") == 0) conGesture(EV_SWIPE_LEFT, 200, 120);
    else if (n == 2 && strcasecmp(tok[1], "r") == 0) conGesture(EV_SWIPE_RIGHT, 40, 120);
    else { conErr("swipe l|r"); return; }
    conOk("swipe");
  }
  else if (IS("hold")) {
    if (n != 3 || !parseInt(tok[1], 0, 239, a) || !parseInt(tok[2], 0, 239, b)) { conErr("hold <x 0-239> <y 0-239>"); return; }
    conGesture(holdKindAt(a, b) == HOLD_RESET ? EV_RESET_HOLD : EV_LONG_PRESS, a, b);   // same rule as touchPoll
    conOk("hold");
  }
  else if (IS("demo")) {
    bool want;
    if (n == 2 && strcasecmp(tok[1], "on") == 0) want = true;
    else if (n == 2 && strcasecmp(tok[1], "off") == 0) want = false;
    else { conErr("demo on|off"); return; }
    if (want != demoActive) conGesture(EV_LONG_PRESS, 120, 120);   // the long-press handler toggles demo
    conOk("demo");
  }
  else if (IS("set")) {
    if (n == 3 && strcasecmp(tok[1], "gain") == 0 && (!strcasecmp(tok[2], "in") || !strcasecmp(tok[2], "out"))) setGain(!strcasecmp(tok[2], "in"));
    else if (n == 3 && strcasecmp(tok[1], "units") == 0 && (!strcasecmp(tok[2], "mi") || !strcasecmp(tok[2], "km"))) setUnits(!strcasecmp(tok[2], "mi"));
    else if (n == 3 && strcasecmp(tok[1], "bright") == 0 && parseInt(tok[2], 0, 2, a)) setBright((uint8_t)a);
    else if (n == 3 && strcasecmp(tok[1], "rot") == 0 && parseInt(tok[2], 0, 3, a)) setRot((uint8_t)a);
    else if (n == 3 && strcasecmp(tok[1], "mnl") == 0 && parseInt(tok[2], 0, 3, a)) setMnl((uint8_t)a);
    else if (n == 3 && strcasecmp(tok[1], "scr") == 0 && parseInt(tok[2], 0, SCR_COUNT - 1, a)) setScr((uint8_t)a);
    else { conErr("set gain in|out | set units mi|km | set bright 0|1|2 | set rot 0-3 | set mnl 0-3 | set scr 0-5"); return; }
    conOk("set");
  }
  else if (IS("profile")) { if (conProfile(n, tok)) conOk("profile"); }
  else if (IS("wifi")) { if (conWifi(n, tok)) conOk("wifi"); }
  else if (IS("ota")) { if (conOta(n, tok)) conOk("ota"); }
  else if (IS("mqtt")) { if (conMqtt(n, tok)) conOk("mqtt"); }
  else if (IS("ha")) { if (conHa(n, tok)) conOk("ha"); }
  else if (IS("mqttcmd")) {                        // test: queue the rest of the line exactly as an MQTT message
    const char* rest = conRest(conRaw, 1);
    if (!*rest) { conErr("mqttcmd <console line>"); return; }
    mqttEnqueueCmd(rest);
    conOk("mqttcmd");
  }
  else if (IS("resetfilters")) { resetFilters(); conOk("resetfilters"); }
  else if (IS("fault")) {
    if (n == 3 && strcasecmp(tok[1], "i2c") == 0 && (!strcasecmp(tok[2], "on") || !strcasecmp(tok[2], "off"))) {
      faultI2c = !strcasecmp(tok[2], "on");
      LOG.printf("[CON] fault i2c %s%s\n", faultI2c ? "on" : "off", sensorOk ? "" : " (no sensor: nothing to fail)");
    }
    else if (n == 3 && strcasecmp(tok[1], "netstall") == 0 && parseInt(tok[2], 1, 120, a)) {
      netTaskEnsure();                             // test: the net task blocks this long, as a wedged socket write would
      netStallS = (uint32_t)a;
      LOG.printf("[CON] fault netstall %ld s: the loop must keep running\n", a);
    }
    else if (n == 2 && strcasecmp(tok[1], "nh") == 0) {
      onNoiseHigh();                               // same handler as a real INT_NH, rate limit included
      LOG.printf("[CON] fault nh: NF=%u noiseFault=%d\n", noiseFloor, noiseFault);
    }
    else if (n == 2 && strcasecmp(tok[1], "cfg") == 0) {
      if (!sensorOk || sensorLost) { conErr("no working sensor"); return; }
      writeRegOk(0x01, 0x77);                      // NF 7 / WD 7 behind the firmware's back, as after a chip reset
      cfgCheckMs = millis() - CONFIG_CHECK_MS;     // check on the next pass
      LOG.println("[CON] fault cfg: REG0x01 corrupted, config check due now");
    }
    else { conErr("fault i2c on|off | fault nh | fault cfg | fault netstall <s>"); return; }
    conOk("fault");
  }
  else if (IS("records")) { conRecords(); conOk("records"); }
  else if (IS("resetrec")) { resetRecords(); conOk("resetrec"); }
  else if (IS("diag")) {
    if (n == 2 && strcasecmp(tok[1], "reset") == 0) { diagReset(); conOk("diag reset"); }
    else conErr("diag reset");
  }
  else if (IS("factory")) { factoryReset(); LOG.println("[CON] factory done"); conOk("factory"); }
  else if (IS("flush")) { persistFlush("console"); conOk("flush"); }
  else if (IS("hang")) {
    conOk("hang");
    Serial.flush();
    while (true) {}                                // no esp_task_wdt_reset(): the task watchdog must reboot us
  }
  else if (IS("shot")) { screenWake("console"); conShot(); conOk("shot"); }
  else if (IS("rst")) { conOk("rst"); Serial.flush(); delay(100); ESP.restart(); }
  else conErr("unknown command (try help)");
  #undef IS
}

// Every line goes through here, from the serial port or from <pfx>/cmd (conFromMqtt).
// The raw copy keeps the spaces of wifi / ota / mqtt values and is wiped afterwards (it may hold a password).
static void conExec(char* line) {
  CRUMB(CS_CONSOLE);
  CRUMB_EVT(CE_CMD);
  strlcpy(conRaw, line, sizeof(conRaw));
  if (conFromMqtt && !conMqttAllowed(conRaw)) conErrMqtt();
  else conExecInner(line);
  memset(conRaw, 0, sizeof(conRaw));
}

// Called once per loop(): accumulate input, run complete lines, step a running burst.
void consolePoll() {
  int budget = 256;                                // bytes per loop, so a flood cannot starve the UI
  while (budget-- > 0 && Serial.available() > 0) {
    int ch = Serial.read();
    if (ch < 0) break;
    if (ch == '\r') continue;
    if (ch == '\n') {
      if (conOverflow) conErr("line too long");
      else if (conLen > 0) { conBuf[conLen] = '\0'; conExec(conBuf); memset(conBuf, 0, sizeof(conBuf)); }
      conLen = 0;
      conOverflow = false;
      continue;
    }
    if (conOverflow) continue;
    if (conLen >= CON_LINE_MAX) { conOverflow = true; continue; }
    conBuf[conLen++] = (char)ch;
  }

  if (burstLeft > 0 && (int32_t)(millis() - burstNextMs) >= 0) {
    uint8_t d = (random(10) == 0) ? DIST_OOR : DIST_CODES[random(sizeof(DIST_CODES))];
    uint32_t e = (uint32_t)random(2097152);
    burstLeft--;
    burstNextMs = millis() + burstIntervalMs;
    injectStrike(e, d);
    if (burstLeft == 0) LOG.println("[CON] burst done");
  }
}

// Called at the end of every loop() pass with that pass's duration (microseconds).
void healthTick(uint32_t nowMs, uint32_t loopUs) {
  healthLoops++;
  healthSumUs += loopUs;
  if (loopUs > healthMaxUs) healthMaxUs = loopUs;
  if (nowMs - healthLastMs < HEALTH_MS) return;
  healthLastMs = nowMs;
  LOG.printf("[HEALTH] up=%lu heap=%lu minheap=%lu maxblk=%lu loops=%lu maxloop=%lu avgloop=%.1f wifi=%s mqtt=%s stack=%u nstack=%u\n",
                (unsigned long)(uptimeMs() / 1000ULL), (unsigned long)ESP.getFreeHeap(),
                (unsigned long)ESP.getMinFreeHeap(), (unsigned long)ESP.getMaxAllocHeap(),
                (unsigned long)healthLoops, (unsigned long)(healthMaxUs / 1000UL),
                healthLoops ? (double)healthSumUs / healthLoops / 1000.0 : 0.0,
                !netWanted() ? "off" : WIFI_NAME[wifiState], mqttState == MQTT_ST_UP ? "up" : "down", (unsigned)uxTaskGetStackHighWaterMark(NULL),
                netTaskH ? (unsigned)uxTaskGetStackHighWaterMark(netTaskH) : 0u);
  healthLoops = 0; healthMaxUs = 0; healthSumUs = 0;
}

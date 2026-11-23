// -- Settings and lifetime records in NVS (Preferences namespace "flashbee") ----------
// Keys: miles (u8), bright (u8 0-2), rot (u8 0-3, screen rotation), scr (u8 0-5, screen-off timeout index),
//       prof (u8, ProfileMode 0-2: AUTO/STATION/PORTABLE; STORMCHASE is never saved),
//       network (net.ino): wSsid wPass otaPass mHost mPort mUser mPass mPfx haDisc net.
//       gain and mnl are old keys: the profile decides both now, they are ignored (and left in place).
//       bootCount, crashCount (u32), lastCrash (u8): restart diagnostics, written once per boot.
//       tunCap (u8 0-15, antenna TUN_CAP; absent = never tuned), lcoHz (u32, LCO measured at that cap).
//       rClose (u8 km code, 0 = none), rEnergy (u32), rStormMax (u32), rTotal (u32), rStorms (u32).
// Writes are deferred: a change is written at most once per NVS_MIN_INTERVAL_MS (10 s), only the
// keys that changed. persistFlush() writes at once (leaving Settings, resetting records).

static const uint8_t BRIGHT_DUTY[3] = { 40, 120, 255 };
static const char* const BRIGHT_NAME[3] = { "LOW", "MED", "HIGH" };
static const uint32_t SCR_MS[SCR_COUNT] = { 30000UL, 60000UL, 120000UL, 300000UL, 600000UL, 0 };   // 0 = never
static const char* const SCR_NAME[SCR_COUNT] = { "30 SEC", "1 MIN", "2 MIN", "5 MIN", "10 MIN", "NEVER" };

void loadPrefs() {
  prefsOk = prefs.begin("flashbee", false);
  if (!prefsOk) {
    LOG.println("[SET] NVS open failed - defaults, nothing saved");
    return;
  }
  uint8_t pm = prefs.getUChar("prof", PM_AUTO);
  profSaved = pm <= PM_PORTABLE ? (ProfileMode)pm : PM_AUTO;
  cfg.miles  = prefs.getUChar("miles", USE_MILES) ? 1 : 0;
  cfg.bright = prefs.getUChar("bright", 2);
  if (cfg.bright > 2) cfg.bright = 2;
  cfg.rot = prefs.getUChar("rot", 0);
  if (cfg.rot > 3) cfg.rot = 0;
  cfg.scr = prefs.getUChar("scr", SCR_DEFAULT);
  if (cfg.scr >= SCR_COUNT) cfg.scr = SCR_DEFAULT;
  rec.closestKm = prefs.getUChar("rClose", 0);
  rec.maxEnergy = prefs.getULong("rEnergy", 0);
  rec.stormMax  = prefs.getULong("rStormMax", 0);
  rec.total     = prefs.getULong("rTotal", 0);
  rec.storms    = prefs.getULong("rStorms", 0);
  uint8_t tc = prefs.getUChar("tunCap", 0xFF);
  if (tc <= 15) { tunSaved = tc; tunCap = tc; lcoHz = prefs.getULong("lcoHz", 0); }
  savedCfg = cfg;
  savedRec = rec;
  LOG.printf("[SET] profile %s, units %s, brightness %s, rot %u, screen off %s\n", PMODE_NAME[profSaved],
                cfg.miles ? "mi" : "km", BRIGHT_NAME[cfg.bright], cfg.rot, SCR_NAME[cfg.scr]);
  LOG.printf("[REC] loaded: closest %u km, energy %lu, storm max %lu, total %lu, storms %lu\n",
                rec.closestKm, (unsigned long)rec.maxEnergy, (unsigned long)rec.stormMax,
                (unsigned long)rec.total, (unsigned long)rec.storms);
}

// Antenna tuning result: written at once (rare, and a reboot must find it).
void saveTune() {
  tunSaved = tunCap;
  if (!prefsOk) { LOG.println("[ANT] NVS not open - tuning not saved"); return; }
  prefs.putUChar("tunCap", tunCap);
  prefs.putULong("lcoHz", lcoHz);
  LOG.printf("[ANT] saved cap %u (%lu Hz) to NVS\n", tunCap, (unsigned long)lcoHz);
}

bool cfgDirty() {
  return cfg.miles != savedCfg.miles || cfg.bright != savedCfg.bright    // indoor and mnl are live profile values: never saved
      || cfg.rot != savedCfg.rot || cfg.scr != savedCfg.scr;
}

bool recDirty() {
  return rec.closestKm != savedRec.closestKm || rec.maxEnergy != savedRec.maxEnergy
      || rec.stormMax != savedRec.stormMax || rec.total != savedRec.total || rec.storms != savedRec.storms;
}

void persistWrite(const char* why) {
  bool c = cfgDirty(), r = recDirty();
  if (!c && !r) return;
  nvsLastWrite = millis();
  if (!prefsOk) { savedCfg = cfg; savedRec = rec; return; }
  if (cfg.miles != savedCfg.miles)   prefs.putUChar("miles", cfg.miles);
  if (cfg.bright != savedCfg.bright) prefs.putUChar("bright", cfg.bright);
  if (cfg.rot != savedCfg.rot)       prefs.putUChar("rot", cfg.rot);
  if (cfg.scr != savedCfg.scr)       prefs.putUChar("scr", cfg.scr);
  if (rec.closestKm != savedRec.closestKm) prefs.putUChar("rClose", rec.closestKm);
  if (rec.maxEnergy != savedRec.maxEnergy) prefs.putULong("rEnergy", rec.maxEnergy);
  if (rec.stormMax != savedRec.stormMax)   prefs.putULong("rStormMax", rec.stormMax);
  if (rec.total != savedRec.total)         prefs.putULong("rTotal", rec.total);
  if (rec.storms != savedRec.storms)       prefs.putULong("rStorms", rec.storms);
  savedCfg = cfg;
  savedRec = rec;
  if (c) LOG.printf("[SET] saved (%s)\n", why);
  if (r) LOG.printf("[REC] saved (%s)\n", why);
}

void persistTick(uint32_t now) {
  if (now - nvsLastWrite < NVS_MIN_INTERVAL_MS) return;
  persistWrite("deferred");
}

void persistFlush(const char* why) { persistWrite(why); }

// Real strikes only (called from pollSensor, never from the demo).
void recordsOnStrike(uint32_t e, uint8_t d, uint32_t now) {
  rec.total++;
  if (!stormSeen || now - stormLastMs >= STORM_GAP_MS) {
    rec.storms++;
    stormCount = 0;
    LOG.printf("[REC] new storm (#%lu lifetime)\n", (unsigned long)rec.storms);
  }
  stormSeen = true;
  stormLastMs = now;
  stormCount++;
  if (stormCount > rec.stormMax) {
    rec.stormMax = stormCount;
    if (stormCount > 1) LOG.printf("[REC] biggest storm: %lu strikes\n", (unsigned long)stormCount);
  }
  if (d != DIST_OOR && d != DIST_UNKNOWN && (rec.closestKm == 0 || d < rec.closestKm)) {
    rec.closestKm = d;
    LOG.printf("[REC] closest strike: %u km\n", d);
  }
  if (e > rec.maxEnergy) {
    rec.maxEnergy = e;
    LOG.printf("[REC] strongest strike: %lu\n", (unsigned long)e);
  }
}

void resetRecords() {
  rec = Records{};
  stormSeen = false;
  stormCount = 0;
  LOG.println("[REC] reset by 3 s hold on Records");
  persistFlush("reset");
  resetMsgUntil = millis() + 2500;
}

void applyBrightness() {
  ledcWrite(BL_PIN, screenOn ? BRIGHT_DUTY[cfg.bright] : 0);   // never light a sleeping panel
}

const char* brightName() { return BRIGHT_NAME[cfg.bright]; }

// -- Screen power ----------------------------------------------------------------
// Off = backlight duty 0, then DISPOFF and SLPIN to the panel. Wake = SLPOUT, 120 ms, DISPON, one frame
// pushed, then the backlight. drawUI() does nothing while the screen is off.
const char* scrName() { return SCR_NAME[cfg.scr]; }

void screenOff() {
  if (!screenOn) return;
  screenOn = false;
  ledcWrite(BL_PIN, 0);
  tft.writecommand(TFT_CMD_DISPOFF);
  tft.writecommand(TFT_CMD_SLPIN);
  LOG.printf("[SCREEN] off (no touch or strike for %s)\n", scrName());
}

// Any activity (strike, touch, console draw command, SHELTER) lands here. Cheap when already on.
void screenWake(const char* why) {
  lastActMs = millis();
  if (screenOn) return;
  tft.writecommand(TFT_CMD_SLPOUT);
  delay(120);                                      // the panel needs 120 ms after SLPOUT
  tft.writecommand(TFT_CMD_DISPON);
  screenOn = true;
  drawUI();                                        // a fresh frame before the light comes on
  applyBrightness();
  LOG.printf("[SCREEN] on (%s)\n", why);
}

// Not while SHELTER, a finger is down, or flash-to-bang is timing (the next tap would only wake the screen).
void screenTick(uint32_t now) {
  if (!screenOn || SCR_MS[cfg.scr] == 0) return;
  if (alertStage == ALERT_SHELTER || ts.down || f2b.state == F2B_TIMING) return;
  uint32_t idle = min(now - lastTouchMs, now - lastActMs);
  if (idle >= SCR_MS[cfg.scr]) screenOff();
}

// -- Restart diagnostics and watchdog ---------------------------------------------------
const char* resetName(uint8_t r) {
  switch (r) {
    case ESP_RST_POWERON:   return "POWERON";
    case ESP_RST_SW:        return "SW";
    case ESP_RST_PANIC:     return "PANIC";
    case ESP_RST_INT_WDT:   return "INT_WDT";
    case ESP_RST_TASK_WDT:  return "TASK_WDT";
    case ESP_RST_WDT:       return "WDT";
    case ESP_RST_BROWNOUT:  return "BROWNOUT";
    case ESP_RST_USB:       return "USB";
    case ESP_RST_EXT:       return "EXT";
    case ESP_RST_DEEPSLEEP: return "DEEPSLEEP";
    default:                return "UNKNOWN";
  }
}

// Once per boot: log the reset reason and keep the counters in NVS.
void bootDiag() {
  esp_reset_reason_t r = esp_reset_reason();
  LOG.printf("[BOOT] reset reason: %s\n", resetName((uint8_t)r));
  bool crash = (r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT || r == ESP_RST_WDT);
  if (!prefsOk) return;
  bootCount = prefs.getULong("bootCount", 0) + 1;
  crashCount = prefs.getULong("crashCount", 0);
  lastCrash = prefs.getUChar("lastCrash", 0);
  if (crash) { crashCount++; lastCrash = (uint8_t)r; }
  prefs.putULong("bootCount", bootCount);
  if (crash) { prefs.putULong("crashCount", crashCount); prefs.putUChar("lastCrash", lastCrash); }
}

// Backstop: any hang longer than WDT_TIMEOUT_MS reboots the device (the task watchdog panics).
void startWatchdog() {
  esp_task_wdt_config_t wcfg = { .timeout_ms = WDT_TIMEOUT_MS, .idle_core_mask = 0, .trigger_panic = true };
  if (esp_task_wdt_reconfigure(&wcfg) == ESP_ERR_INVALID_STATE) esp_task_wdt_init(&wcfg);
  esp_task_wdt_add(NULL);   // the loop task
}

// Test console: zero the restart diagnostics (RAM and NVS).
void diagReset() {
  bootCount = 0; crashCount = 0; lastCrash = 0;
  if (prefsOk) {
    prefs.putULong("bootCount", 0);
    prefs.putULong("crashCount", 0);
    prefs.putUChar("lastCrash", 0);
  }
  LOG.println("[DIAG] reset");
}

// Test console: first-boot state (records, settings, profile AUTO, diagnostics), written to NVS at once. Network settings stay (wifi forget).
void factoryReset() {
  resetRecords();
  cfg.miles = USE_MILES;
  cfg.bright = 2;
  cfg.rot = 0;
  cfg.scr = SCR_DEFAULT;
  tft.setRotation(0);
  tft.fillScreen(C_BG);
  profileSetMode(PM_AUTO, "factory");               // gain, MNL and the chip knobs follow the profile
  applyBrightness();
  diagReset();
  persistFlush("factory");
}

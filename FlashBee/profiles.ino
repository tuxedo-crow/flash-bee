// -- Operating profiles ------------------------------------------------------------------------
// The profile decides the sensor knobs (see PROFILES in FlashBee.ino): gain is always OUTDOOR, NF starts at 2.
//   STATION    WD 2 SR 2 MNL 1 (5 strikes / 15 min) disturbers masked   adaptation on   (mains, WiFi, MQTT)
//   PORTABLE   WD 2 SR 2 MNL 0 (every strike)       disturbers masked   adaptation on   (battery, no WiFi)
//   STORMCHASE WD 1 SR 0 MNL 0                      disturbers reported adaptation off  (attended only, never saved)
// AUTO resolves to STATION while WiFi is associated and PORTABLE otherwise, never STORMCHASE.

const char* profileName(Profile p) { return p < PROF_COUNT ? PROFILES[p].name : "?"; }

static bool wifiAssociated() { return wifiState == WIFI_ST_UP; }

// RAM side of a profile: the live knobs the chip writes and the read-back check use.
static void profileLoadKnobs() {
  const ProfileDef &d = PROFILES[profEff];
  cfg.indoor = 0;                                   // OUTDOOR in every profile
  cfg.mnl = d.mnl;
  noiseFloor = NF_DEFAULT;
  watchdog = d.wd;
  spikeRej = d.sr;
  profOverride = false;
}

static void profileLogApplied(const char* why) {
  const ProfileDef &d = PROFILES[profEff];
  LOG.printf("[PROFILE] applied %s (mode %s, %s): NF %u WD %u SR %u MNL %u (%u strikes/15 min) MASK_DIST %u freeze %u gain %s%s\n",
             d.name, PMODE_NAME[profMode], why, noiseFloor, watchdog, spikeRej, cfg.mnl, MNL_STRIKES[cfg.mnl],
             d.maskDist, d.freeze, cfg.indoor ? "IN" : "OUT", sensorOk ? "" : " (no sensor: not written)");
}

// Make `p` the effective profile: knobs into RAM, chip writes, log.
void profileApply(Profile p, const char* why) {
  profEff = p;
  profileLoadKnobs();
  if (sensorOk && !sensorLost) {                    // a lost sensor gets them from initSensor() when it returns
    applyGain();
    applyWatchdogSpike();
    maskWrite(0x03, 0x20, PROFILES[profEff].maskDist << 5);
    applyMnl();
    senseLastAdj = nfLastAdj = millis();
    nfLastRaise = millis() - NF_RAISE_INTERVAL;
    cfgCheckMs = millis();
  }
  mqttStateDirty = true;
  profileLogApplied(why);
}

// Called once from setup(), before the first chip write.
void profileBoot() {
  profMode = profSaved;                             // STORMCHASE was never saved: a reboot always lands here
  Profile p;
  if (profMode == PM_AUTO) {
    bool wantNet = ncfg.net && ncfg.ssid[0];
    profPending = wantNet;                          // optimistic STATION while the first connect runs
    profBootMs = millis();
    p = wantNet ? PROF_STATION : PROF_PORTABLE;
  } else {
    p = (Profile)(profMode - 1);
  }
  profEff = p;
  profileLoadKnobs();                               // initSensor() writes the chip
  LOG.printf("[PROFILE] boot: mode %s -> %s%s\n", PMODE_NAME[profMode], PROFILES[profEff].name,
             profPending ? " (pending: PORTABLE if no WiFi within 60 s)" : "");
}

// User choice (console, Settings button, MQTT select). Always re-applies, which clears any live override.
void profileSetMode(ProfileMode m, const char* why) {
  profMode = m;
  profPending = false;
  if (m == PM_STORMCHASE) {
    LOG.println("[PROFILE] STORMCHASE is for this run only (not saved): attended use, it floods with false strikes");
  } else {
    if (profSaved != m && prefsOk) prefs.putUChar("prof", (uint8_t)m);
    profSaved = m;
  }
  Profile p;
  if (m == PM_AUTO) {
    profAssoc = wifiAssociated();
    profAssocSince = millis();
    p = profAssoc ? PROF_STATION : PROF_PORTABLE;
  } else {
    p = (Profile)(m - 1);
  }
  profileApply(p, why);
}

void profileCycle() {                                // Settings button: auto -> station -> portable -> stormchase -> auto
  profileSetMode((ProfileMode)((profMode + 1) % PM_COUNT), "touch");
}

// AUTO only: follow the WiFi association, with the boot grace and the 30 s debounce.
void profileTick(uint32_t now) {
  if (profMode != PM_AUTO) return;
  bool assoc = wifiAssociated();
  if (profPending) {
    if (assoc) { profPending = false; profAssoc = true; profAssocSince = now; LOG.println("[PROFILE] WiFi up: STATION confirmed"); }
    else if (now - profBootMs >= PROF_BOOT_PENDING_MS) {
      profPending = false;
      profAssoc = false;
      profAssocSince = now;
      profileApply(PROF_PORTABLE, "no WiFi 60 s after boot");
    }
    return;
  }
  if (assoc != profAssoc) { profAssoc = assoc; profAssocSince = now; }
  Profile target = profAssoc ? PROF_STATION : PROF_PORTABLE;
  if (target != profEff && now - profAssocSince >= PROF_DEBOUNCE_MS)
    profileApply(target, profAssoc ? "WiFi associated 30 s" : "WiFi gone 30 s");
}

// Console: `profile` (show) and `profile auto|station|portable|stormchase`.
bool conProfile(int n, char** tok) {
  if (n == 1) {
    const ProfileDef &d = PROFILES[profEff];
    LOG.printf("[PROFILE] mode=%s saved=%s effective=%s%s%s wd=%u sr=%u mnl=%u mask=%u freeze=%u gain=%s nf=%u\n",
               PMODE_NAME[profMode], PMODE_NAME[profSaved], d.name, profPending ? " (pending)" : "",
               profOverride ? " (live override)" : "", d.wd, d.sr, cfg.mnl, d.maskDist, d.freeze,
               cfg.indoor ? "in" : "out", noiseFloor);
    uint8_t r0 = readReg(0x00), r1 = readReg(0x01), r2 = readReg(0x02);
    char r3[4] = "--";
    if (reg3Ok) snprintf(r3, sizeof(r3), "%02X", reg3Last);
    LOG.printf("[PROFILE] chip R0=%02X R1=%02X R2=%02X R3=%s (R3 from the last poll; MASK_DIST=bit5) sensor=%s\n",
               r0, r1, r2, r3, sensorOk ? (sensorLost ? "lost" : "ok") : "none");
    return true;
  }
  if (n == 2) {
    for (int i = 0; i < PM_COUNT; i++) {
      if (!strcasecmp(tok[1], PMODE_NAME[i])) { profileSetMode((ProfileMode)i, "console"); return true; }
    }
  }
  LOG.println("[CON] err profile [auto|station|portable|stormchase]");
  return false;
}

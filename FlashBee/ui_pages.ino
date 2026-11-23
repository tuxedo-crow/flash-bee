// -- UI: Storm chart, Flash-to-bang, Records, Settings and Nerd stats pages -------------

// ============================ Storm chart ============================
// Plot: x 50..206 (156 px = 60 min, now at the right), y 50..140 (distance, near at the bottom).
// Bars: strikes per 5 min (all strikes, also out of range) in y 162..190, drawn at their real time.
#define CH_X0 50
#define CH_X1 206
#define CH_YT 50
#define CH_YB 140
#define CH_BT 162
#define CH_BB 190

int chartX(uint32_t age) {                      // age in ms -> x (age 0 = right edge)
  if (age >= CHART_MS) return CH_X0;
  return CH_X1 - (int)((uint64_t)age * (CH_X1 - CH_X0) / CHART_MS);
}

int chartY(float units, float maxUnits) {
  if (units > maxUnits) units = maxUnits;
  return CH_YB - (int)(units / maxUnits * (CH_YB - CH_YT) + 0.5f);
}

void drawChart() {
  drawBorder();
  const Stats &S = *st;
  uint32_t now = millis();
  bool miles = cfg.miles;
  float maxU = miles ? 25.0f : 40.0f;
  int step = miles ? 5 : 10;

  drawPageTitle(demoActive ? "DEMO - NOT REAL" : "LAST 60 MINUTES", demoActive ? C_DEMO : C_GOLD, 1);

  // Grid + distance labels (right-aligned at x=46)
  spr.setTextSize(1);
  for (int u = 0; u <= (int)maxU; u += step) {
    int y = chartY(u, maxU);
    if (u > 0) for (int x = CH_X0; x <= CH_X1; x += 4) spr.drawPixel(x, y, C_DIM);
    spr.setTextDatum(MR_DATUM);
    spr.setTextColor(C_GREY);
    char b[12];
    snprintf(b, sizeof(b), "%d", u);
    spr.drawString(b, CH_X0 - 4, y);
  }
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C_GREY);
  spr.drawString(miles ? "DISTANCE (mi)" : "DISTANCE (km)", CX, 33);
  // 30-30 line: 10 mi / 16 km, dashed red-orange
  int yd = chartY(miles ? 10.0f : (float)CLEAR_KM, maxU);
  for (int x = CH_X0; x <= CH_X1; x += 6) spr.drawFastHLine(x, yd, 3, C_REDORG);
  // Time grid every 15 min
  for (int m = 15; m < 60; m += 15) {
    int x = chartX((uint32_t)m * 60000UL);
    for (int y = CH_YT; y <= CH_YB; y += 4) spr.drawPixel(x, y, C_DIM);
  }
  spr.drawFastHLine(CH_X0, CH_YB, CH_X1 - CH_X0 + 1, C_GREY);
  spr.drawFastVLine(CH_X0, CH_YT, CH_YB - CH_YT + 1, C_GREY);
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C_GREY);
  spr.drawString("-60m", CH_X0, CH_YB + 4);
  spr.drawString("-30m", (CH_X0 + CH_X1) / 2, CH_YB + 4);
  spr.drawString("now", CH_X1, CH_YB + 4);

  // Dots: in-range strikes of the last 60 min, oldest first so new ones are on top
  int shown = 0, newestIdx = -1;
  for (int i = 0; i < S.histCount; i++) {
    int idx = (S.histHead - S.histCount + i + HIST_MAX) % HIST_MAX;
    const StrikeRec &r = S.hist[idx];
    uint32_t age = now - r.t;
    if (age > CHART_MS || r.d == DIST_OOR || r.d == DIST_UNKNOWN) continue;   // no distance: bars only
    float u = miles ? r.d * 0.621371f : (float)r.d;
    if (r.d <= 1) u = 0;                         // overhead sits on the axis
    uint16_t c = energyColor(r.e);
    if (c == C_DIM) c = C_GREY;                  // weakest class would vanish on black
    spr.fillCircle(chartX(age), chartY(u, maxU) - 2, 2, c);
    newestIdx = idx;
    shown++;
  }
  if (newestIdx >= 0) {                          // ring around the newest dot
    const StrikeRec &r = S.hist[newestIdx];
    float u = (r.d <= 1) ? 0 : (miles ? r.d * 0.621371f : (float)r.d);
    spr.drawCircle(chartX(now - r.t), chartY(u, maxU) - 2, 4, C_WHITE);
  }
  if (shown == 0) {
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(C_GREY);
    spr.drawString("no strikes in range", CX + 14, 80);
    spr.drawString("in the last hour", CX + 14, 92);
  }

  // Bars: bin k back from the current one starts at binStartMs - k*5min
  uint16_t mx = 0;
  for (int k = 0; k < BIN_COUNT; k++) {
    uint16_t c = S.binCnt[(S.binHead - k + BIN_COUNT) % BIN_COUNT];
    if (c > mx) mx = c;
  }
  uint16_t scale = mx < 5 ? 5 : mx;
  uint32_t curAge = now - S.binStartMs;          // age of the current bin's start
  for (int k = 0; k < BIN_COUNT; k++) {
    uint16_t c = S.binInit ? S.binCnt[(S.binHead - k + BIN_COUNT) % BIN_COUNT] : 0;
    uint32_t ageStart = curAge + (uint32_t)k * BIN_MS;          // older edge
    uint32_t ageEnd = (k == 0) ? 0 : curAge + (uint32_t)(k - 1) * BIN_MS;
    if (ageEnd >= CHART_MS) continue;
    int xl = chartX(ageStart) + 1, xr = chartX(ageEnd) - 1;
    if (xr <= xl) continue;
    spr.drawFastHLine(xl, CH_BB, xr - xl + 1, C_DIM);
    if (c == 0) continue;
    int h = max(2, (int)c * (CH_BB - CH_BT) / scale);
    spr.fillRect(xl, CH_BB - h, xr - xl + 1, h, demoActive ? C_DEMO : C_GOLD);
    if (c < 100 && xr - xl + 1 >= 11) {
      char b[4];
      snprintf(b, sizeof(b), "%u", c);
      spr.setTextDatum(BC_DATUM);
      spr.setTextColor(C_WHITE);
      spr.drawString(b, (xl + xr) / 2 + 1, CH_BB - h - 1);
    }
  }
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(C_GREY);
  spr.drawString("STRIKES PER 5 MIN", CX, 196);
}

// ============================ Flash-to-bang ============================
// Big round target r=66 at (120,112). Any tap on this page counts, so a missed edge still works.
#define F2B_CY 112
#define F2B_R  66

void f2bTap(uint32_t tapMs) {
  switch (f2b.state) {
    case F2B_IDLE:
      f2b.state = F2B_TIMING;
      f2b.startMs = tapMs;
      LOG.println("[F2B] flash - timing");
      break;
    case F2B_TIMING: {
      float sec = (tapMs - f2b.startMs) / 1000.0f;
      f2b.sec = sec;
      f2b.state = F2B_RESULT;
      for (int i = F2B_HIST - 1; i > 0; i--) f2b.hist[i] = f2b.hist[i - 1];
      f2b.hist[0] = sec;
      if (f2b.histN < F2B_HIST) f2b.histN++;
      const Stats &S = *st;
      f2b.sensorHad = S.strikeCount > 0 && tapMs - S.lastStrikeMs <= F2B_SENSOR_MS;
      f2b.sensorDemo = demoActive;
      f2b.sensorD = S.lastDist;
      LOG.printf("[F2B] thunder after %.1f s = %.1f mi / %.1f km", sec, sec / 5.0f, sec / 3.0f);
      if (f2b.sensorHad && f2b.sensorD == DIST_UNKNOWN) LOG.printf(" (%s: distance unknown)", demoActive ? "demo" : "sensor");
      else if (f2b.sensorHad) LOG.printf(" (%s said %u km)", demoActive ? "demo" : "sensor", f2b.sensorD);
      LOG.println();
      break;
    }
    default:
      f2b.state = F2B_IDLE;
      LOG.println("[F2B] reset");
      break;
  }
}

void f2bTick(uint32_t now) {
  if (f2b.state == F2B_TIMING && now - f2b.startMs >= F2B_TIMEOUT_MS) {
    f2b.state = F2B_TOOFAR;
    LOG.println("[F2B] no thunder within 60 s - too far to hear");
  }
}

void fmtF2BDist(char *buf, size_t n, float sec) {
  snprintf(buf, n, "%.1f %s", cfg.miles ? sec / 5.0f : sec / 3.0f, distUnit());
}

void drawF2B() {
  drawBorder();
  uint32_t now = millis();
  drawPageTitle("FLASH TO BANG", C_GOLD, 1);
  spr.setTextDatum(MC_DATUM);

  if (f2b.state == F2B_IDLE) {
    spr.drawCircle(CX, F2B_CY, F2B_R, C_GOLD);
    spr.drawCircle(CX, F2B_CY, F2B_R - 1, C_GOLD);
    spr.setTextColor(C_WHITE, C_BG);
    spr.setTextSize(2);
    spr.drawString("TAP AT", CX, F2B_CY - 12);
    spr.drawString("THE FLASH", CX, F2B_CY + 12);
    spr.setTextSize(1);
    spr.setTextColor(C_GREY, C_BG);
    spr.drawString("see lightning? tap!", CX, F2B_CY + 38);
  } else if (f2b.state == F2B_TIMING) {
    spr.fillCircle(CX, F2B_CY, F2B_R, C_GOLD);
    float s = (now - f2b.startMs) / 1000.0f;
    char b[12];
    snprintf(b, sizeof(b), "%.1f", s);
    spr.setTextColor(C_BG);
    spr.setTextSize(4);
    spr.drawString(b, CX, F2B_CY - 20);
    spr.setTextSize(1);
    spr.drawString("seconds", CX, F2B_CY + 4);
    spr.drawString("TAP AT THE", CX, F2B_CY + 22);
    spr.setTextSize(2);
    spr.drawString("THUNDER", CX, F2B_CY + 38);
  } else if (f2b.state == F2B_RESULT) {
    char b[24];
    snprintf(b, sizeof(b), "%.1f SECONDS", f2b.sec);
    spr.setTextColor(C_GREY, C_BG);
    spr.setTextSize(2);
    spr.drawString(b, CX, 56);
    snprintf(b, sizeof(b), "%.1f", cfg.miles ? f2b.sec / 5.0f : f2b.sec / 3.0f);
    spr.setTextColor(C_GOLD, C_BG);
    spr.setTextSize(5);
    spr.drawString(b, CX, 96);
    spr.setTextSize(2);
    spr.drawString(cfg.miles ? "MILES AWAY" : "KM AWAY", CX, 132);
    if (f2b.sensorHad) {
      char d[16], s2[32];
      fmtDist(d, sizeof(d), f2b.sensorD);
      snprintf(s2, sizeof(s2), "%s said: %s", f2b.sensorDemo ? "Demo" : "Sensor", d);
      spr.setTextSize(1);
      spr.setTextColor(f2b.sensorDemo ? C_DEMO : C_WHITE, C_BG);
      spr.drawString(s2, CX, 154);
    }
    spr.setTextSize(1);
    spr.setTextColor(C_GREY, C_BG);
    spr.drawString("tap to start again", CX, 168);
  } else {                                       // F2B_TOOFAR
    spr.drawCircle(CX, F2B_CY, F2B_R, C_GREY);
    spr.setTextColor(C_ORANGE, C_BG);
    spr.setTextSize(2);
    spr.drawString("TOO FAR", CX, F2B_CY - 22);
    spr.drawString("TO HEAR", CX, F2B_CY - 2);
    spr.setTextSize(1);
    spr.setTextColor(C_GREY, C_BG);
    spr.drawString("no thunder in 60 s", CX, F2B_CY + 20);
    spr.drawString("tap to start again", CX, F2B_CY + 34);
  }

  // Last 3 results, newest first: y 184, 194, 204 (visible x 43..197 at y=212)
  if (f2b.histN > 0) {
    spr.setTextSize(1);
    for (int i = 0; i < f2b.histN; i++) {
      char d[16], line[32];
      fmtF2BDist(d, sizeof(d), f2b.hist[i]);
      snprintf(line, sizeof(line), "%.1f s = %s", f2b.hist[i], d);
      spr.setTextColor(i == 0 ? C_GREY : C_DIM, C_BG);
      spr.drawString(line, CX, 186 + i * 10);
    }
  }
}

// ============================ Records ============================
// Rows: label right-aligned at x=114 (size 1), value left-aligned at x=122 (size 2), y 54..166.
void drawRecordRow(int y, const char *label, const char *value) {
  spr.setTextSize(1);
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString(label, 114, y);
  spr.setTextSize(2);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(C_GOLD, C_BG);
  spr.drawString(value, 122, y);
}

// Rows y 48..136 (step 22), then the RESET button (3 s hold, not a tap) and a note under it.
static const Btn BTN_RECRESET = { 50, 154, 140, 44 };

void drawRecords() {
  drawBorder();
  drawPageTitle("RECORDS", C_GOLD, 2);
  char b[20];
  if (rec.closestKm == 0) snprintf(b, sizeof(b), "--");
  else                    fmtDist(b, sizeof(b), rec.closestKm);
  drawRecordRow(48, "CLOSEST", b);
  if (rec.maxEnergy == 0) snprintf(b, sizeof(b), "--");
  else                    fmtEnergy(b, sizeof(b), rec.maxEnergy);
  drawRecordRow(70, "STRONGEST", b);
  snprintf(b, sizeof(b), "%lu", (unsigned long)rec.stormMax);
  drawRecordRow(92, "BIGGEST STORM", b);
  snprintf(b, sizeof(b), "%lu", (unsigned long)rec.total);
  drawRecordRow(114, "ALL STRIKES", b);
  snprintf(b, sizeof(b), "%lu", (unsigned long)rec.storms);
  drawRecordRow(136, "STORMS", b);

  drawBtnFrame(BTN_RECRESET, C_REDORG);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(2);
  spr.setTextColor(C_REDORG, C_BG);
  spr.drawString("RESET", CX, BTN_RECRESET.y + 15);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("RECORDS (HOLD 3 S)", CX, BTN_RECRESET.y + 34);
  spr.setTextColor(demoActive ? C_DEMO : C_DIM, C_BG);
  spr.drawString(demoActive ? "demo strikes don't count" : "real strikes, ever", CX, 207);

  if ((int32_t)(resetMsgUntil - millis()) > 0) {  // confirmation after the hold
    spr.fillRoundRect(46, 160, 148, 32, 5, C_BG);
    spr.drawRoundRect(46, 160, 148, 32, 5, C_GREEN);
    spr.setTextSize(1);
    spr.setTextColor(C_GREEN, C_BG);
    spr.drawString("RECORDS RESET", CX, 176);
  }
}

void recordsTap(int x, int y) {
  if (inBtn(BTN_RECRESET, x, y)) LOG.println("[REC] reset needs a 3 s hold, tap ignored");
}

// ============================ Settings ============================
// Four buttons, all >= 44 px tall, corners inside r=110:
//   PROFILE x44..196 y42..88      UNITS x28..116 y94..140    BRIGHT x124..212 y94..140
//   SCREEN x44..196  y146..192 (tap cycles the screen-off timeout)
static const Btn BTN_PROFILE = { 44, 42, 152, 46 };
static const Btn BTN_UNITS  = { 28, 94, 88, 46 };
static const Btn BTN_BRIGHT = { 124, 94, 88, 46 };
static const Btn BTN_SCREEN = { 44, 146, 152, 46 };
#define BTN_MARGIN 4                              // fingers are wider than outlines

bool inBtn(const Btn &b, int x, int y) {
  return x >= b.x - BTN_MARGIN && x < b.x + b.w + BTN_MARGIN
      && y >= b.y - BTN_MARGIN && y < b.y + b.h + BTN_MARGIN;
}

// Where a press started decides what a 3 s hold does.
HoldKind holdKindAt(int x, int y) {
  if (curPage == PAGE_RECORDS && inBtn(BTN_RECRESET, x, y)) return HOLD_RESET;
  return HOLD_DEMO;
}

// Live overrides for this run (console only, never saved): the next profile change puts the profile's values back.
void setGain(bool indoor) {
  cfg.indoor = indoor;
  profOverride = true;
  applyGain();
  LOG.printf("[SET] gain %s (AFE 0x%02X) - override until the next profile change%s\n", cfg.indoor ? "INDOOR" : "OUTDOOR", afeValue(),
                sensorOk ? "" : " (no sensor: not written)");
}

void setUnits(bool miles) {
  cfg.miles = miles;
  LOG.printf("[SET] units %s\n", cfg.miles ? "mi" : "km");
}

void setBright(uint8_t b) {
  cfg.bright = b;
  applyBrightness();
  LOG.printf("[SET] brightness %s\n", brightName());
}

// MIN_NUM_LIGH 0..3: the chip raises a lightning interrupt only after 1, 5, 9 or 16 strikes in 15 min.
void setMnl(uint8_t m) {
  cfg.mnl = m & 3;
  profOverride = true;
  applyMnl();
  LOG.printf("[SET] min lightning %u (chip reports after %u strikes in 15 min) - override until the next profile change%s\n", cfg.mnl,
                MNL_STRIKES[cfg.mnl], sensorOk ? "" : " (no sensor: not written)");
}

// Screen-off timeout index 0..5 = 30 s, 1, 2, 5, 10 min, never.
void setScr(uint8_t i) {
  cfg.scr = i < SCR_COUNT ? i : SCR_DEFAULT;
  lastActMs = millis();                           // count from now
  LOG.printf("[SET] screen off after %s\n", scrName());
}

// Screen rotation 0..3 (tft.setRotation). Clears the panel; the loop redraws the current page next frame.
// Only the test console (set rot) calls this: the Settings page has no room for another button.
void setRot(uint8_t r) {
  cfg.rot = r & 3;
  tft.setRotation(cfg.rot);
  tft.fillScreen(C_BG);
  LOG.printf("[SET] rotation %u\n", cfg.rot);
}

void settingsTap(int x, int y) {
  if (inBtn(BTN_PROFILE, x, y)) {
    profileCycle();
  } else if (inBtn(BTN_UNITS, x, y)) {
    setUnits(!cfg.miles);
  } else if (inBtn(BTN_BRIGHT, x, y)) {
    setBright((cfg.bright + 1) % 3);
  } else if (inBtn(BTN_SCREEN, x, y)) {
    setScr((cfg.scr + 1) % SCR_COUNT);
  }
}

void drawBtnFrame(const Btn &b, uint16_t col) {
  spr.drawRoundRect(b.x, b.y, b.w, b.h, 8, col);
  spr.drawRoundRect(b.x + 1, b.y + 1, b.w - 2, b.h - 2, 7, col);
}

void drawSettings() {
  drawBorder();
  drawPageTitle("SETTINGS", C_GOLD, 2);
  int cxg = BTN_PROFILE.x + BTN_PROFILE.w / 2;
  int cxu = BTN_UNITS.x + BTN_UNITS.w / 2;
  int cxb = BTN_BRIGHT.x + BTN_BRIGHT.w / 2;

  // PROFILE: the mode (size 2); AUTO shows the resolved profile under it, STORMCHASE a warning colour.
  bool chase = profMode == PM_STORMCHASE;
  uint16_t pc = chase ? C_REDORG : C_WHITE;
  drawBtnFrame(BTN_PROFILE, chase ? C_REDORG : C_GREY);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("PROFILE", cxg, BTN_PROFILE.y + 9);
  char pn[16];
  strlcpy(pn, PMODE_NAME[profMode], sizeof(pn));
  for (char* c = pn; *c; c++) *c = toupper(*c);
  spr.setTextSize(2);
  spr.setTextColor(pc, C_BG);
  bool sub = profMode == PM_AUTO || chase;
  spr.drawString(pn, cxg, BTN_PROFILE.y + (sub ? 22 : 28));
  if (sub) {
    char sb[24];
    if (chase) {
      snprintf(sb, sizeof(sb), "THIS RUN ONLY");
    } else {
      snprintf(sb, sizeof(sb), "%s%s", PROFILES[profEff].name, profPending ? " ..." : "");
      for (char* c = sb; *c; c++) *c = toupper(*c);
    }
    spr.setTextSize(1);
    spr.setTextColor(chase ? C_REDORG : C_ORANGE, C_BG);
    spr.drawString(sb, cxg, BTN_PROFILE.y + 38);
  }

  drawBtnFrame(BTN_UNITS, C_GREY);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("UNITS", cxu, BTN_UNITS.y + 11);
  spr.setTextSize(2);
  spr.setTextColor(C_WHITE, C_BG);
  spr.drawString(cfg.miles ? "MILES" : "KM", cxu, BTN_UNITS.y + 30);

  drawBtnFrame(BTN_BRIGHT, C_GREY);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("BRIGHTNESS", cxb, BTN_BRIGHT.y + 11);
  for (int i = 0; i < 3; i++) {                  // three rising bars, filled up to the level
    int bh = 6 + i * 5;
    int bx = cxb - 17 + i * 12;
    int by = BTN_BRIGHT.y + 38 - bh;
    if (i <= cfg.bright) spr.fillRect(bx, by, 10, bh, C_GOLD);
    else                 spr.drawRect(bx, by, 10, bh, C_GREY);
  }

  drawBtnFrame(BTN_SCREEN, C_GREY);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("SCREEN OFF AFTER", CX, BTN_SCREEN.y + 11);
  spr.setTextSize(2);
  spr.setTextColor(cfg.scr == SCR_COUNT - 1 ? C_ORANGE : C_WHITE, C_BG);
  spr.drawString(scrName(), CX, BTN_SCREEN.y + 30);

  spr.setTextSize(1);
  spr.setTextColor(C_DIM, C_BG);
  spr.drawString("tap a button to change", CX, 203);
}

// ============================ Nerd stats ============================
// Two columns: label right-aligned at x=114, value left-aligned at x=122, rows y 44..154 step 11,
// then the RESET FILTERS button (44 px tall, y 166..210; its corners are 105-115 px from the centre).
static const Btn BTN_FILTERS = { 50, 168, 140, 44 };

void drawNerdRow(int y, const char *label, const char *value, uint16_t col) {
  spr.setTextSize(1);
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString(label, 114, y);
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(col, C_BG);
  spr.drawString(value, 122, y);
}

void nerdTap(int x, int y) {
  if (inBtn(BTN_FILTERS, x, y)) resetFilters();   // harmless: no strike data and no 30-30 state changes
}

void drawNerd() {
  drawBorder();
  drawPageTitle("NERD STATS", C_GOLD, 2);
  char b[32];
  int y = 41;
  const int dy = 10;
  if (sensorOk) snprintf(b, sizeof(b), "%u / %u / %u", noiseFloor, watchdog, spikeRej);
  else          snprintf(b, sizeof(b), "--");
  drawNerdRow(y, "NF/WD/SR", b, noiseFault ? 0xF800 : C_WHITE); y += dy;
  if (!sensorOk)        drawNerdRow(y, "SENSOR", "NOT FOUND", 0xF800);
  else if (sensorLost)  drawNerdRow(y, "SENSOR", "LOST", 0xF800);
  else if (noiseFault)  drawNerdRow(y, "SENSOR", "ENV TOO NOISY", 0xF800);
  else                  drawNerdRow(y, "SENSOR", "OK", C_WHITE);
  y += dy;
  if (profMode == PM_AUTO) snprintf(b, sizeof(b), "AUTO>%s%s", PROFILES[profEff].name, profPending ? "?" : "");
  else                     snprintf(b, sizeof(b), "%s", PROFILES[profEff].name);
  for (char* c = b; *c; c++) *c = toupper(*c);
  drawNerdRow(y, "PROFILE", b, profMode == PM_STORMCHASE ? C_REDORG : C_WHITE); y += dy;
  snprintf(b, sizeof(b), "%s 0x%02X / MNL %u%s", cfg.indoor ? "IN" : "OUT", afeValue(), cfg.mnl, profOverride ? " *" : "");
  drawNerdRow(y, "GAIN/MNL", b, (cfg.indoor || profOverride) ? C_ORANGE : C_WHITE); y += dy;
  snprintf(b, sizeof(b), "%s%s", alertName(alertStage), demoActive ? " (demo)" : "");
  drawNerdRow(y, "ALERT", b, demoActive ? C_DEMO : alertStage == ALERT_SHELTER ? 0xF800
                                           : alertStage == ALERT_ACTIVITY ? C_ORANGE : C_WHITE); y += dy;
  snprintf(b, sizeof(b), "%lu / %lu", (unsigned long)disturbers, (unsigned long)noiseEvents);
  drawNerdRow(y, "DIST/NOISE", b, C_WHITE); y += dy;
  char ant[12];
  if (lcoHz) snprintf(ant, sizeof(ant), "%.1fk", lcoHz / 1000.0);
  else       snprintf(ant, sizeof(ant), "cap %u", tunCap);
  if (sensorOk) snprintf(b, sizeof(b), "%02X/%02X %s %s", calTrco, calSrco, rcoCalOk ? "OK" : "FAIL", ant);
  else          snprintf(b, sizeof(b), "--");
  drawNerdRow(y, "CAL / ANT", b, (sensorOk && !rcoCalOk) ? C_REDORG : C_WHITE); y += dy;
  uint64_t up = uptimeMs() / 1000ULL;
  unsigned long days = (unsigned long)(up / 86400ULL);
  unsigned long rest = (unsigned long)(up % 86400ULL);
  if (days) snprintf(b, sizeof(b), "%lud %02lu:%02lu", days, rest / 3600, (rest % 3600) / 60);
  else      snprintf(b, sizeof(b), "%02lu:%02lu:%02lu", rest / 3600, (rest % 3600) / 60, rest % 60);
  drawNerdRow(y, "UPTIME", b, C_WHITE); y += dy;
  if (battPct >= 0) snprintf(b, sizeof(b), "%lu mV %d%%", (unsigned long)(battMv * 2), battPct);   // cell = 2x pin
  else              snprintf(b, sizeof(b), "-- (KE off?)");
  drawNerdRow(y, "BATTERY", b, (battPct >= 0 && battPct < BATT_LOW_PCT) ? C_REDORG : C_WHITE); y += dy;
  snprintf(b, sizeof(b), "%d boot / %lu storm", realStats.strikeCount, (unsigned long)stormCount);
  drawNerdRow(y, "STRIKES", b, C_WHITE); y += dy;
  if (crashCount > 0) snprintf(b, sizeof(b), "%lu / %lu %s", (unsigned long)bootCount, (unsigned long)crashCount, resetName(lastCrash));
  else                snprintf(b, sizeof(b), "%lu / %lu", (unsigned long)bootCount, (unsigned long)crashCount);
  drawNerdRow(y, "BOOTS/CRASH", b, crashCount > 0 ? C_REDORG : C_WHITE); y += dy;
  netNerdRows(y, dy);

  bool done = (int32_t)(filtersMsgUntil - millis()) > 0;
  drawBtnFrame(BTN_FILTERS, done ? C_GREEN : C_ORANGE);
  spr.setTextDatum(MC_DATUM);
  spr.setTextSize(2);
  spr.setTextColor(done ? C_GREEN : C_ORANGE, C_BG);
  spr.drawString(done ? "DONE" : "RESET", CX, BTN_FILTERS.y + 15);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  char fb[16];
  snprintf(fb, sizeof(fb), "NF2 WD%u SR%u", PROFILES[profEff].wd, PROFILES[profEff].sr);
  spr.drawString(done ? fb : "FILTERS", CX, BTN_FILTERS.y + 34);
}

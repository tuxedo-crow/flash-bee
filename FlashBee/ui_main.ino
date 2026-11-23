// -- UI: shared drawing helpers, the page dispatcher and the Main page -------------------
// Everything is drawn into the one 240x240 sprite and pushed once per loop.
// Round screen: a row at height y is visible for |x - 120| < sqrt(120^2 - (y - 120)^2).

// -- Intensity color -----------------------------------------------------
uint16_t energyColor(uint32_t e) {
  if (e > 500000) return C_WHITE;
  else if (e > 300000) return C_GOLD;
  else if (e > 150000) return C_GOLD;
  else if (e > 75000) return C_ORANGE;
  else if (e > 20000) return C_REDORG;
  else return C_DIM;
}

void fmtEnergy(char *buf, size_t n, uint32_t e) {
  if (e > 999) snprintf(buf, n, "%luK", (unsigned long)(e / 1000));
  else         snprintf(buf, n, "%lu", (unsigned long)e);
}

// Distance code -> short text in the active unit: "6 mi", "< 1 mi", ">25 mi", "unknown"
void fmtDist(char *buf, size_t n, uint8_t d) {
  if (d == DIST_UNKNOWN) snprintf(buf, n, "unknown");
  else if (d == DIST_OOR) snprintf(buf, n, "%s %s", distMaxStr(), distUnit());
  else if (d <= 1)     snprintf(buf, n, "%s", distOverheadStr());
  else                 snprintf(buf, n, "%d %s", distForDisplay(d), distUnit());
}

// Outer border: magenta 2 px in demo (on every page), dim otherwise.
void drawBorder() {
  if (demoActive) {
    spr.drawCircle(CX, CY, 119, C_DEMO);
    spr.drawCircle(CX, CY, 118, C_DEMO);
  } else {
    spr.drawCircle(CX, CY, 119, C_DIMRING);
  }
}

// Page title, top centre. Size 2 titles stay <= 10 chars (120 px): at y=20 the corners of a
// 120 px line sit at r=117, just inside the glass.
void drawPageTitle(const char *title, uint16_t col, uint8_t size) {
  spr.setTextDatum(TC_DATUM);
  spr.setTextSize(size);
  spr.setTextColor(col, C_BG);
  spr.drawString(title, CX, size >= 2 ? 20 : 22);
}

// Page indicator: 6 dots at y=226, 10 px apart (x 95..145), current one bigger and gold.
void drawPageDots() {
  int x = CX - (PAGE_COUNT - 1) * 5;
  for (int i = 0; i < PAGE_COUNT; i++, x += 10) {
    if (i == curPage) spr.fillCircle(x, 226, 3, C_GOLD);
    else              spr.fillCircle(x, 226, 2, C_GREY);
  }
}

// "HOLD: DEMO ON/OFF" or "HOLD: RESET RECORDS" box with a filling bar
void drawHoldOverlay(TFT_eSPI &g) {
  uint32_t held = millis() - ts.downMs;
  int fill = (int)(min(held, (uint32_t)LONG_PRESS_MS) * 124UL / LONG_PRESS_MS);
  bool reset = ts.kind == HOLD_RESET;
  uint16_t col = reset ? C_REDORG : (demoActive ? C_GOLD : C_DEMO);
  g.fillRoundRect(46, 164, 148, 38, 5, C_BG);
  g.drawRoundRect(46, 164, 148, 38, 5, col);
  g.setTextDatum(MC_DATUM);
  g.setTextColor(C_WHITE, C_BG);
  g.setTextSize(1);
  g.drawString(reset ? "HOLD: RESET RECORDS" : (demoActive ? "HOLD: DEMO OFF" : "HOLD: DEMO ON"), CX, 176);
  g.drawRect(58, 188, 124, 8, C_GREY);
  g.fillRect(58, 188, fill, 8, col);
}

// Small battery icon + "87%" centred at (cx, y top). Grey; red-orange under 15 %; "--" = no reading.
void drawBattery(int cx, int y) {
  char b[12];
  if (battPct < 0) snprintf(b, sizeof(b), "--");
  else             snprintf(b, sizeof(b), "%d%%", battPct);
  uint16_t col = (battPct >= 0 && battPct < BATT_LOW_PCT) ? C_REDORG : C_GREY;
  int tw = strlen(b) * 6;
  int w = 12 + 2 + 3 + tw;                 // body + nub + gap + text
  int x = cx - w / 2;
  spr.drawRect(x, y, 12, 7, col);
  spr.fillRect(x + 12, y + 2, 2, 3, col);
  if (battPct > 0) spr.fillRect(x + 2, y + 2, max(1, battPct * 8 / 100), 3, col);
  spr.setTextDatum(TL_DATUM);
  spr.setTextSize(1);
  spr.setTextColor(col);
  spr.drawString(b, x + 17, y);
}

// Small WiFi bars (11 x 7 px) at (x, y), only when WiFi is wanted: grey bars = associated, dim outline = not yet.
// Band y=165..172 right of the battery: x 163..173 clears the r=58 ring (x<=157 there), the energy value and the ticker box.
void drawWifiIcon(int x, int y) {
  if (!ncfg.net || !ncfg.ssid[0]) return;
  bool up = wifiState == WIFI_ST_UP;
  for (int i = 0; i < 3; i++) {
    int bh = 3 + i * 2;
    int bx = x + i * 4;
    if (up) spr.fillRect(bx, y + 7 - bh, 3, bh, C_GREY);
    else    spr.drawRect(bx, y + 7 - bh, 3, bh, C_DIM);
  }
}

// -- Arc angle mapping ---------------------------------------------------
// TFT_eSPI drawArc: 0 deg = 6 o'clock, clockwise (90 = 9, 180 = 12, 270 = 3).
// Speedometer gauge: empty at bottom-left (7:30 = 45 deg), sweeps clockwise over the top to
// bottom-right (4:30 = 315 deg), gap at the bottom. pct 0.0-1.0 maps to 45 -> 45 + pct*270.
void drawGaugeArc(TFT_eSprite &s, uint16_t fillColor, float pct) {
  uint8_t r_outer = 116;
  uint8_t r_inner = 106;  // thickness = 10 px

  // Background: full 45 -> 315 sweep
  s.drawArc(CX, CY, r_outer, r_inner, 45, 315, 0x2945, C_BG, false);

  // Filled portion (never past 315, no wrap)
  if (pct > 0.01f) {
    uint16_t endAngle = 45 + (uint16_t)(pct * 270);
    if (endAngle > 315) endAngle = 315;
    s.drawArc(CX, CY, r_outer, r_inner, 45, endAngle, fillColor, C_BG, true);
  }
}

// -- Tick marks on gauge -------------------------------------------------
// Ticks use screen math: cos/sin, 0 deg = 3 o'clock, clockwise (90 = 6, 180 = 9, 270 = 12).
// Same sweep as the arc: 135 (7:30) -> 405 (= 45, 4:30), over the top.
void drawGaugeTicks(TFT_eSprite &s, int count) {
  for (int i = 0; i <= count; i++) {
    float deg = 135 + (270.0f / count) * i;
    float rad = deg * DEG_TO_RAD;
    int x1 = CX + (int)(104 * cos(rad));
    int y1 = CY + (int)(104 * sin(rad));
    int x2 = CX + (int)(118 * cos(rad));
    int y2 = CY + (int)(118 * sin(rad));
    s.drawLine(x1, y1, x2, y2, 0x528A);
  }
}

// -- NOT FOUND (Main page without a sensor and without the demo) --------------
void drawNotFound() {
  spr.drawCircle(CX, CY, 118, C_DIMRING);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(C_GOLD, C_BG);
  spr.setTextSize(2);
  spr.drawString("AS3935", CX, CY - 14);
  spr.setTextSize(1);
  spr.setTextColor(0xF800, C_BG);
  spr.drawString("NOT FOUND", CX, CY + 12);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("hold screen for demo", CX, CY + 28);
  spr.drawString("swipe for other pages", CX, CY + 42);
}

// -- SENSOR LOST (Main page while the AS3935 has stopped answering) ---------------
// Like NOT FOUND, but the alert stays visible: strikes seen before the loss still count.
void drawAlertFooter(const Stats &S, uint32_t now);
void drawAlertBorder(uint32_t now);

void drawSensorLost() {
  uint32_t now = millis();
  spr.drawCircle(CX, CY, 118, C_DIMRING);
  if (alertStage == ALERT_SHELTER) drawAlertBorder(now);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(0xF800, C_BG);
  spr.setTextSize(2);
  spr.drawString("SENSOR LOST", CX, CY - 30);
  spr.setTextSize(1);
  spr.setTextColor(C_GREY, C_BG);
  spr.drawString("AS3935 not answering", CX, CY - 8);
  spr.drawString("retrying every 3 s", CX, CY + 6);
  spr.drawString("swipe for other pages", CX, CY + 40);
  if (alertStage != ALERT_IDLE) drawAlertFooter(*st, now);
}

// -- Alert drawing -----------------------------------------------------------------
// SHELTER: the border and a banner flip red/amber every ALERT_BLINK_MS (1 Hz); in demo everything is
// magenta (banner alternates filled/outline, the border is already magenta).
void drawAlertBorder(uint32_t now) {
  if (demoActive) return;
  uint16_t c = ((now / ALERT_BLINK_MS) & 1) ? C_ORANGE : 0xF800;
  spr.drawCircle(CX, CY, 119, c);
  spr.drawCircle(CX, CY, 118, c);
  spr.drawCircle(CX, CY, 117, c);
}

// Banner x42..198, y174..206 (where the ticker box sits): at y=206 the glass is x36..204 wide.
void drawShelterBanner(uint32_t now) {
  bool ph = (now / ALERT_BLINK_MS) & 1;
  uint16_t fill, txt;
  if (demoActive) { fill = ph ? C_BG : C_DEMO; txt = ph ? C_DEMO : C_BG; }
  else            { fill = ph ? C_ORANGE : 0xF800; txt = ph ? C_BG : C_WHITE; }
  spr.fillRoundRect(42, 174, 156, 32, 6, fill);
  if (demoActive) spr.drawRoundRect(42, 174, 156, 32, 6, C_DEMO);
  spr.setTextDatum(MC_DATUM);
  spr.setTextColor(txt, fill);
  spr.setTextSize(3);
  spr.drawString("SHELTER", CX, 191);
}

// Label under the ticker / banner (y=212..219). One slot, by priority: the 30-30 countdown, the reason
// for SHELTER, ACTIVITY, ALL CLEAR (60 s), then the plain caption. "ALL CLEAR IN 23:41" = 108 px (x 66..174);
// at y=220 the screen is visible for x 54..186.
void drawAlertLabel(const Stats &S, uint32_t now) {
  char lb[28];
  uint16_t lc = C_GREY;
  snprintf(lb, sizeof(lb), "ENERGY HISTORY");
  int anyN, nearN;
  alertCounts(S, now, anyN, nearN);
  if (S.clearRunning) {
    uint32_t left = (CLEAR_MS - (now - S.clearFromMs) + 999) / 1000;   // round up: starts at 30:00
    snprintf(lb, sizeof(lb), "ALL CLEAR IN %02lu:%02lu", (unsigned long)(left / 60), (unsigned long)(left % 60));
    lc = demoActive ? C_DEMO : C_REDORG;
  } else if (alertStage == ALERT_SHELTER) {
    snprintf(lb, sizeof(lb), "%d STRIKES IN 15 MIN", nearN);
    lc = demoActive ? C_DEMO : C_REDORG;
  } else if (alertStage == ALERT_ACTIVITY) {
    snprintf(lb, sizeof(lb), "ACTIVITY");
    lc = demoActive ? C_DEMO : C_ORANGE;
  } else if (S.clearDoneShow) {
    snprintf(lb, sizeof(lb), demoActive ? "ALL CLEAR (DEMO)" : "ALL CLEAR");
    lc = demoActive ? C_DEMO : C_GREEN;
  }
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(lc, C_BG);
  spr.setTextSize(1);
  spr.drawString(lb, CX, 212);
}

void drawAlertFooter(const Stats &S, uint32_t now) {
  if (alertStage == ALERT_SHELTER) drawShelterBanner(now);
  drawAlertLabel(S, now);
}

// -- Main page -----------------------------------------------------------
void drawMain() {
  if (!sensorOk && !demoActive) { drawNotFound(); return; }
  if (sensorLost && !demoActive) { drawSensorLost(); return; }

  const Stats &S = *st;
  float pct = (S.lastEnergy > 0)
                ? constrain(S.lastEnergy / 2097151.0f, 0, 1)
                : 0;

  // Energy gauge arc
  drawGaugeArc(spr, energyColor(S.lastEnergy), pct);
  drawGaugeTicks(spr, 10);

  drawBorder();
  if (alertStage == ALERT_SHELTER) drawAlertBorder(millis());

  // Inner guide rings
  spr.drawCircle(CX, CY, 90, C_DIM);
  spr.drawCircle(CX, CY, 58, C_DIM);

  // Listening rings: two rings contract from r=90 toward the centre, half a period apart, so one
  // is always mid-travel. Purely radial on purpose: the AS3935 has one loop antenna and cannot
  // tell direction, so a rotating sweep would suggest a bearing it does not have. Brightness
  // follows sin(t), brightest at mid-radius. Green-teal normally, red-amber during SHELTER.
  {
    uint32_t now = millis();
    bool shelter = (alertStage == ALERT_SHELTER);
    for (int i = 0; i < 2; i++) {
      uint32_t p = (now + (uint32_t)i * (RING_PERIOD_MS / 2)) % RING_PERIOD_MS;
      float t = p / (float)RING_PERIOD_MS;
      int r = 90 - (int)(t * 82);                     // 90 -> 8
      uint8_t v = (uint8_t)(sinf(t * (float)PI) * 120);
      uint16_t rc = shelter ? spr.color565(v, v / 3, 0) : spr.color565(0, v, v / 2);
      spr.drawCircle(CX, CY, r, rc);                  // two radii = a 2 px ring
      spr.drawCircle(CX, CY, r - 1, rc);
    }
  }

  // Top label
  spr.setTextDatum(TC_DATUM);
  spr.setTextColor(demoActive ? C_DEMO : C_GREY, C_BG);
  spr.setTextSize(1);
  spr.drawString(demoActive ? "DEMO - NOT REAL" : "LIGHTNING DETECTOR", CX, 16);

  // Tune line + profile: "NF2 WD2 SR2 STAT" = 16 chars, 96 px (x 72..168). At y=27 the arc's inner
  // edge leaves about 102 px. Profile tag: STAT / PORT / CHAS (STORMCHASE, red-orange so it is hard to forget);
  // the tag turns orange when set gain / set mnl overrides the profile.
  // While the noise fault holds, the line is a red ENV TOO NOISY (13 chars = 78 px) instead.
  char tb[24];
  snprintf(tb, sizeof(tb), "NF%u WD%u SR%u ", noiseFloor, watchdog, spikeRej);
  static const char* const PTAG[PROF_COUNT] = { "STAT", "PORT", "CHAS" };
  const char *gainStr = PTAG[profEff];
  int tw = (strlen(tb) + strlen(gainStr)) * 6;
  int tx = CX - tw / 2;
  spr.setTextDatum(TL_DATUM);
  spr.setTextSize(1);
  if (noiseFault) {
    spr.setTextColor(0xF800, C_BG);
    spr.setTextDatum(TC_DATUM);
    spr.drawString("ENV TOO NOISY", CX, 27);
    spr.setTextDatum(TL_DATUM);
  } else {
    spr.setTextColor(C_DIM, C_BG);
    spr.drawString(tb, tx, 27);
    spr.setTextColor(profEff == PROF_STORMCHASE ? C_REDORG : (profOverride ? C_ORANGE : C_GREY), C_BG);
    spr.drawString(gainStr, tx + strlen(tb) * 6, 27);
  }

  // Strike age + strikes in the last 15 min (y=40..48; chord at r=104 is +-66 px there).
  // Single-argument setTextColor = transparent background, so the r=90 guide ring is not cut.
  uint32_t now = millis();
  int winN = 0;
  bool winCapped = false;
  Trend trend = stormStats(S, now, winN, winCapped);
  bool stale = (S.strikeCount > 0) && (now - S.lastStrikeMs > STALE_MS);
  uint16_t distCol = stale ? C_DIM : C_GOLD;     // an old reading must not look current
  if (S.strikeCount > 0) {
    unsigned long age = (now - S.lastStrikeMs) / 1000UL;
    char ab[20], lb[48];
    if (age < 60)        snprintf(ab, sizeof(ab), "%lus ago", age);
    else if (age < 3600) snprintf(ab, sizeof(ab), "%lum ago", age / 60);
    else                 snprintf(ab, sizeof(ab), "%luh %02lum ago", age / 3600, (age % 3600) / 60);
    snprintf(lb, sizeof(lb), "%s  %d%s in 15m", ab, winN, winCapped ? "+" : "");
    spr.setTextDatum(TC_DATUM);
    spr.setTextColor(C_GREY);
    spr.setTextSize(1);
    spr.drawString(lb, CX, 40);
  }

  // Distance: hero value (yellow, big; dim once stale)
  spr.setTextDatum(MC_DATUM);
  if (S.strikeCount == 0) {
    spr.setTextColor(C_DIM, C_BG);
    spr.setTextSize(3);
    spr.drawString("--", CX, 84);
    spr.setTextSize(1);
    spr.setTextColor(C_GREY, C_BG);
    spr.drawString("waiting...", CX, 108);
  } else if (S.lastDist == DIST_UNKNOWN) {
    // strike seen, distance unknown
    spr.setTextColor(distCol, C_BG);
    spr.setTextSize(3);
    spr.drawString("--", CX, 84);
    spr.setTextSize(1);
    spr.setTextColor(C_GREY, C_BG);
    spr.drawString("distance unknown", CX, 108);
  } else if (S.lastDist == DIST_OOR) {
    // out of range
    spr.setTextColor(distCol, C_BG);
    spr.setTextSize(3);
    spr.drawString(distMaxStr(), CX, 80);
    spr.setTextSize(2);
    spr.setTextColor(distCol, C_BG);
    spr.drawString(distUnit(), CX, 105);
  } else if (S.lastDist == 0x01) {
    // overhead
    spr.setTextColor(stale ? C_DIM : C_REDORG, C_BG);
    spr.setTextSize(2);
    spr.drawString("OVERHEAD", CX, 78);
    spr.setTextSize(2);
    spr.setTextColor(distCol, C_BG);
    spr.drawString(distOverheadStr(), CX, 100);
  } else {
    // normal distance: size 6 number + size 3 unit
    spr.setTextColor(distCol, C_BG);
    spr.setTextSize(6);
    spr.drawString(String(distForDisplay(S.lastDist)), CX, 80);
    spr.setTextColor(distCol, C_BG);
    spr.setTextSize(3);
    spr.drawString(distUnit(), CX, 112);
  }

  // Divider
  spr.drawFastHLine(44, 128, 152, C_DIM);

  // Strikes (left)
  spr.setTextDatum(ML_DATUM);
  spr.setTextColor(C_GREY, C_BG);
  spr.setTextSize(1);
  spr.drawString("STRIKES", 40, 140);

  // Storm trend: centered in the gap between the STRIKES (ends x=82) and ENERGY (starts x=164)
  // labels, y=136..144. 11 chars = 66 px -> x=87..153, inside the r=58 ring (x=66..174 at y=140).
  if (trend != TREND_NONE) {
    uint16_t tc = (trend == TREND_APPROACH) ? C_REDORG : (trend == TREND_AWAY) ? C_GREEN : C_GREY;
    spr.setTextDatum(MC_DATUM);
    spr.setTextColor(tc, C_BG);
    spr.setTextSize(1);
    spr.drawString(trendText(trend), CX, 140);
    spr.setTextDatum(ML_DATUM);
  }
  spr.setTextColor(C_GOLD, C_BG);
  spr.setTextSize(2);
  spr.drawString(String(S.strikeCount), 40, 156);

  // Energy (right)
  spr.setTextDatum(MR_DATUM);
  spr.setTextColor(C_GREY, C_BG);
  spr.setTextSize(1);
  spr.drawString("ENERGY", 200, 140);
  spr.setTextColor(energyColor(S.lastEnergy), C_BG);
  spr.setTextSize(2);
  char eb[12];
  fmtEnergy(eb, sizeof(eb), S.lastEnergy);
  spr.drawString(eb, 200, 156);

  // Battery: the free band y=165..172 between the values row (148..164) and the ticker box (174).
  // Centred: at most ~41 px (x 100..140); strikes value ends by x=88, energy value starts at x>=140.
  drawBattery(CX, 165);
  drawWifiIcon(163, 165);

  // Ticker bar. In SHELTER the banner takes its place (same box).
  if (alertStage == ALERT_SHELTER) {
    drawShelterBanner(now);
  } else {
    int bax = 32, bay = 176, baw = 176, bah = 30;
    int bw = baw / MAX_TICKS - 1;
    spr.drawRoundRect(bax - 2, bay - 2, baw + 4, bah + 6, 4, C_DIM);
    for (int i = 0; i < S.tickCount; i++) {
      int idx = (S.tickHead - S.tickCount + i + MAX_TICKS) % MAX_TICKS;
      float bpct = constrain(S.tickEnergy[idx] / 2097151.0f, 0, 1);
      int bh = max(2, (int)(bpct * bah));
      int bx = bax + i * (bw + 1);
      int by = bay + bah - bh;
      uint16_t bc = (i == S.tickCount - 1)
                      ? C_GOLD
                      : energyColor(S.tickEnergy[idx]);
      spr.fillRect(bx, by, bw, bh, bc);
    }
  }

  drawAlertLabel(S, now);
}

// SHELTER on every non-Main page: border blink plus a small label at y=9..16 (above the titles, which start at y=20;
// its corners are 113 px from the centre). Demo: magenta label.
void drawShelterTag(uint32_t now) {
  bool ph = (now / ALERT_BLINK_MS) & 1;
  drawAlertBorder(now);
  uint16_t c = demoActive ? C_DEMO : (ph ? C_ORANGE : 0xF800);
  spr.setTextDatum(TC_DATUM);
  spr.setTextSize(1);
  spr.setTextColor(c, C_BG);
  spr.drawString("SHELTER", CX, 9);
}

// -- Dispatcher ------------------------------------------------------------
void drawUIFrame() {                 // render the current page into the sprite (no push)
  spr.fillSprite(C_BG);
  switch (curPage) {
    case PAGE_MAIN:     drawMain(); break;
    case PAGE_CHART:    drawChart(); break;
    case PAGE_F2B:      drawF2B(); break;
    case PAGE_RECORDS:  drawRecords(); break;
    case PAGE_SETTINGS: drawSettings(); break;
    case PAGE_NERD:     drawNerd(); break;
    default:            break;
  }
  if (curPage != PAGE_MAIN && alertStage == ALERT_SHELTER) drawShelterTag(millis());
  drawPageDots();
  if (holdFeedbackWanted()) drawHoldOverlay(spr);
}

void drawUI() {
  if (!screenOn) return;                // nothing to draw: the panel is asleep
  drawUIFrame();
  spr.pushSprite(0, 0);
}

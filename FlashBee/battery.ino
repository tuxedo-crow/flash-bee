// -- Battery: Round Display A0 divider on D0 (cell voltage / 2) ----------------------
// Percent comes from a piecewise-linear Li-ion discharge table: 100 % at 2100 mV (4.20 V), 0 % at 1650 mV
// (3.30 V). The table values are a generic single-cell curve, not measured on this pack. Only valid with the KE switch ON.
// Outside 1500-2300 mV, or a noisy (floating) pin, means "no reading" and the screen shows "--".
// The reading is scaled by BATT_CAL_PERMILLE (calibrated so a full cell reads 100 %).

// Calibrated pin mV -> percent, linear between table points, clamped 0-100.
int battPctFromMv(uint32_t mv) {
  static const uint16_t MV[]  = {2100, 2050, 2000, 1950, 1900, 1850, 1800, 1750, 1650};  // pin mV (battery V / 2)
  static const uint8_t  PCT[] = { 100,   90,   78,   62,   40,   15,    5,    2,    0};
  const int N = sizeof(MV) / sizeof(MV[0]);
  if (mv >= MV[0]) return PCT[0];
  if (mv <= MV[N - 1]) return PCT[N - 1];
  for (int i = 1; i < N; i++) {
    if (mv >= MV[i]) {
      int span = MV[i - 1] - MV[i];
      return PCT[i] + (int)(mv - MV[i]) * (PCT[i - 1] - PCT[i]) / span;
    }
  }
  return 0;
}

void battTick(uint32_t now) {
  if (battEverRead && now - battLastRead < BATT_READ_MS) return;
  if (now - lastFlashMs < FLASH_QUIET_MS) return;        // not right after a screen flash
  battLastRead = now;
  battEverRead = true;

  uint32_t sum = 0, lo = UINT32_MAX, hi = 0;
  for (int i = 0; i < BATT_SAMPLES; i++) {
    uint32_t mv = analogReadMilliVolts(BATT_PIN);
    sum += mv;
    if (mv < lo) lo = mv;
    if (mv > hi) hi = mv;
  }
  uint32_t avg = sum / BATT_SAMPLES;
  battRawMv = avg;
  avg = avg * BATT_CAL_PERMILLE / 1000;                  // calibrated pin mV (battRawMv stays raw)
  bool valid = avg >= BATT_MV_MIN && avg <= BATT_MV_MAX && hi - lo <= BATT_SPREAD_MV;
  if (valid) {
    battMv = battMv ? (battMv * 3 + avg) / 4 : avg;      // light smoothing
    battPct = battPctFromMv(battMv);
  } else {
    battMv = 0;
    battPct = -1;
  }
  if (battLastLog == 0 || now - battLastLog >= BATT_LOG_MS) {
    battLastLog = now ? now : 1;
    if (valid) LOG.printf("[BATT] %lu mV (avg %lu, spread %lu) -> %d%%\n", (unsigned long)battMv,
                             (unsigned long)avg, (unsigned long)(hi - lo), battPct);
    else       LOG.printf("[BATT] no reading (avg %lu mV, spread %lu) - KE switch or battery OFF?\n",
                             (unsigned long)avg, (unsigned long)(hi - lo));
  }
}

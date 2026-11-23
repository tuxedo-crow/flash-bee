// -- Input: one touch state machine for tap, swipe, long press (demo) and hold-to-reset ----
//
// INT (D7) is LOW while touched (Seeed chsc6x_is_pressed() = digitalRead(TOUCH_INT) == LOW).
// Some controllers pulse INT while held, so "down" = LOW seen within TOUCH_GAP_MS.
// Coordinates come from the CHSC6X over I2C, read only while INT is LOW, at most every TOUCH_READ_MS.
//
// One press = finger down .. finger up. Classified once:
//   - while held: 3 s (LONG_PRESS_MS) within HOLD_SLOP_PX of the start -> long press. A press that
//     started on the Records RESET button is a records-reset hold, anywhere else a demo toggle.
//     It fires once and marks the press consumed: the release is then neither tap nor swipe.
//   - on release: < TAP_MAX_MS and < TAP_SLOP_PX travel -> tap (timestamp = finger down);
//     horizontal travel >= SWIPE_MIN_PX, |dx| >= 2|dy|, < SWIPE_MAX_MS -> swipe; else ignored.
//   - a press that finds the screen off only wakes it: it is consumed at once (no tap, swipe or hold).
// Events go into a small queue; handleTouch() runs them from loop().

const char* pageName(Page p) {
  switch (p) {
    case PAGE_MAIN:     return "MAIN";
    case PAGE_CHART:    return "CHART";
    case PAGE_F2B:      return "FLASH-TO-BANG";
    case PAGE_RECORDS:  return "RECORDS";
    case PAGE_SETTINGS: return "SETTINGS";
    case PAGE_NERD:     return "NERD";
    default:            return "?";
  }
}

void pushEvent(TouchEvt type, int16_t x, int16_t y, uint32_t ms) {
  if (evCount >= EVQ_LEN) return;                 // full: drop (a human can't outrun 4 slots)
  TouchEvent &e = evQ[(evHead + evCount) % EVQ_LEN];
  e.type = type; e.x = x; e.y = y; e.ms = ms;
  evCount++;
}

bool popEvent(TouchEvent &out) {
  if (evCount == 0) return false;
  out = evQ[evHead];
  evHead = (evHead + 1) % EVQ_LEN;
  evCount--;
  return true;
}

bool readTouchXY(int16_t &x, int16_t &y) {
  uint8_t n = Wire.requestFrom(TOUCH_ADDR, 5);
  if (n != 5) {
    while (Wire.available()) Wire.read();
    return false;
  }
  uint8_t b[5];
  for (int i = 0; i < 5; i++) b[i] = Wire.read();
  if (b[0] != 0x01) return false;                 // not a single valid touch point
  // Raw CHSC6X point (rx, ry) is in the panel's rotation-0 frame. Convert it to the frame of
  // tft.setRotation(cfg.rot). Source: derived from Seeed_GFX TFT_Drivers/GC9A01_Rotation.h (MADCTL per
  // rotation) and identical to Seeed_Arduino_Round_display chsc6x_convert_xy() (repeat (x,y) -> (y,N-x)
  // once per rotation step), except N-1 instead of N so the result stays 0..239.
  //   rot 0 (none)          : x = rx,      y = ry
  //   rot 1 (MX|MV)         : x = ry,      y = 239 - rx
  //   rot 2 (MX|MY)         : x = 239 - rx, y = 239 - ry
  //   rot 3 (MV|MY)         : x = 239 - ry, y = rx
  int16_t rx = b[2], ry = b[4];
  if (rx > 239) rx = 239;
  if (ry > 239) ry = 239;
  switch (cfg.rot & 3) {
    case 1:  x = ry;       y = 239 - rx;  break;
    case 2:  x = 239 - rx; y = 239 - ry;  break;
    case 3:  x = 239 - ry; y = rx;        break;
    default: x = rx;       y = ry;        break;
  }
  return true;
}

void touchPoll() {
  uint32_t now = millis();
  bool low = digitalRead(TOUCH_PIN) == LOW;
  if (low) { ts.lastLowMs = now; ts.seen = true; }
  bool down = ts.seen && (now - ts.lastLowMs <= TOUCH_GAP_MS);

  if (down && !ts.down) {                          // finger down: start a new press
    ts.down = true;
    ts.downMs = now;
    ts.lastReadMs = now - TOUCH_READ_MS;
    ts.havePos = false;
    ts.maxTravel2 = 0;
    ts.kind = HOLD_DEMO;
    ts.consumed = false;
    ts.wake = false;
    lastTouchMs = now;
    LOG.println("[TOUCH] down");
    if (!screenOn) {                               // the touch that wakes the screen does nothing else
      ts.consumed = true;
      ts.wake = true;
      screenWake("touch");
    }
  }

  if (ts.down && low && now - ts.lastReadMs >= TOUCH_READ_MS) {
    ts.lastReadMs = now;
    int16_t x, y;
    if (readTouchXY(x, y)) {
      if (!ts.havePos) {
        ts.havePos = true;
        ts.x0 = x; ts.y0 = y;
        ts.kind = holdKindAt(x, y);
      }
      ts.x1 = x; ts.y1 = y;
      int32_t dx = x - ts.x0, dy = y - ts.y0;
      int32_t d2 = dx * dx + dy * dy;
      if (d2 > ts.maxTravel2) ts.maxTravel2 = d2;
    }
  }

  if (ts.down && down) {
    lastTouchMs = now;
    if (!ts.consumed && now - ts.downMs >= LONG_PRESS_MS
        && ts.maxTravel2 <= (int32_t)HOLD_SLOP_PX * HOLD_SLOP_PX) {
      ts.consumed = true;
      pushEvent(ts.kind == HOLD_RESET ? EV_RESET_HOLD : EV_LONG_PRESS, ts.x0, ts.y0, ts.downMs);
    }
  }

  if (!down && ts.down) {                          // finger up: classify the press
    ts.down = false;
    lastTouchMs = now;
    uint32_t dur = ts.lastLowMs - ts.downMs;
    if (ts.consumed) {
      if (ts.wake) LOG.printf("[TOUCH] up (wake only, %lu ms)\n", (unsigned long)dur);
      else         LOG.printf("[TOUCH] up (long press, %lu ms)\n", (unsigned long)dur);
      return;
    }
    if (!ts.havePos) {                             // no coordinates read: only a tap is possible
      if (dur < TAP_MAX_MS) pushEvent(EV_TAP, -1, -1, ts.downMs);
      else LOG.printf("[TOUCH] up (held %lu ms, no position) ignored\n", (unsigned long)dur);
      return;
    }
    int32_t dx = ts.x1 - ts.x0, dy = ts.y1 - ts.y0;
    if (dur < TAP_MAX_MS && ts.maxTravel2 < (int32_t)TAP_SLOP_PX * TAP_SLOP_PX) {
      pushEvent(EV_TAP, ts.x0, ts.y0, ts.downMs);
    } else if (abs(dx) >= SWIPE_MIN_PX && abs(dx) >= 2 * abs(dy) && dur < SWIPE_MAX_MS) {
      pushEvent(dx < 0 ? EV_SWIPE_LEFT : EV_SWIPE_RIGHT, ts.x0, ts.y0, ts.downMs);
    } else {
      LOG.printf("[TOUCH] up (%lu ms, dx %ld dy %ld) ignored\n", (unsigned long)dur, (long)dx, (long)dy);
    }
  }
}

// delay() that keeps sampling the touch pin and feeding the task watchdog (boot waits run 1.5 s)
void waitSampling(uint32_t ms) {
  uint32_t t0 = millis();
  do { esp_task_wdt_reset(); touchPoll(); delay(5); } while (millis() - t0 < ms);
}

// Hold feedback only for a still finger, and a reset hold only while Settings is still showing.
bool holdFeedbackWanted() {
  if (!ts.down || ts.consumed) return false;
  if (millis() - ts.downMs <= HOLD_FEEDBACK_MS) return false;
  if (ts.maxTravel2 > (int32_t)HOLD_SLOP_PX * HOLD_SLOP_PX) return false;
  if (ts.kind == HOLD_RESET && curPage != PAGE_RECORDS) return false;
  return true;
}

void setPage(Page p, const char* reason) {
  if (p == curPage) return;
  if (curPage == PAGE_SETTINGS) persistFlush("left settings");
  LOG.printf("[PAGE] %s -> %s (%s)\n", pageName(curPage), pageName(p), reason);
  curPage = p;
}

// One gesture, from the touch state machine or the test console.
void dispatchTouch(const TouchEvent &ev) {
    switch (ev.type) {
      case EV_LONG_PRESS:
        if (demoActive) exitDemo("touch");
        else enterDemo();
        break;
      case EV_RESET_HOLD:
        if (curPage == PAGE_RECORDS) resetRecords();
        else LOG.println("[REC] reset hold cancelled (page changed)");
        break;
      case EV_SWIPE_LEFT:
        LOG.println("[SWIPE] left (next page)");
        setPage((Page)((curPage + 1) % PAGE_COUNT), "swipe");
        break;
      case EV_SWIPE_RIGHT:
        LOG.println("[SWIPE] right (previous page)");
        setPage((Page)((curPage + PAGE_COUNT - 1) % PAGE_COUNT), "swipe");
        break;
      case EV_TAP:
        LOG.printf("[TAP] %d,%d on %s\n", ev.x, ev.y, pageName(curPage));
        if (curPage == PAGE_F2B) f2bTap(ev.ms);
        else if (curPage == PAGE_SETTINGS) settingsTap(ev.x, ev.y);
        else if (curPage == PAGE_RECORDS) recordsTap(ev.x, ev.y);
        else if (curPage == PAGE_NERD) nerdTap(ev.x, ev.y);
        break;
      default:
        break;
    }
}

void handleTouch() {
  TouchEvent ev;
  while (popEvent(ev)) dispatchTouch(ev);
  // Demo also ends by itself after DEMO_TIMEOUT_MS.
  if (demoActive && millis() - demoStartMs >= DEMO_TIMEOUT_MS) exitDemo("timeout");
}

// Any page other than Main goes back to Main after 2 min without touch,
// except flash-to-bang while it is timing.
void navTick(uint32_t now) {
  if (curPage == PAGE_MAIN || ts.down) return;
  if (f2b.state == F2B_TIMING) return;
  if (now - lastTouchMs >= PAGE_IDLE_MS) setPage(PAGE_MAIN, "2 min idle");
}

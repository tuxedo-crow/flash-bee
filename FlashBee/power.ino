// -- Light sleep while the screen is off ------------------------------------------------------
//
// Each loop pass ends in restOrSleep(): the normal 50 ms of touch sampling, or one light sleep of at most
// SLEEP_MAX_MS when nothing needs the CPU awake (sleepBlocker() names the first reason it does).
// Wakes: AS3935 IRQ high (D2), touch INT low (D7), or the timer, so the periodic work (sensor poll, noise
// decay, config check, battery, deferred NVS writes, countdowns) still runs about once a second.
// esp_timer (millis) is corrected for the time asleep by the IDF; the task watchdog timer is paused in sleep.
//
// IDF facts this relies on (core 3.3.10, IDF 5.5, checked in the shipped libraries):
//   - CONFIG_ESP_SLEEP_GPIO_RESET_WORKAROUND isolates every pin in sleep (no output, no input, no pull).
//     gpio_wakeup_enable() exempts its pin (gpio_sleep_sel_dis), so the wake pins keep their input and the
//     touch pull-up. The display, backlight and I2C pins are exempted here too, so they keep their levels:
//     CS stays high (the panel ignores the bus) and the backlight stays driven low.
//   - Light sleep switches the USB-Serial/JTAG pad off (sleep_console_usj_pad_backup_and_disable) and back on
//     at wake: a computer sees the device unplug on every sleep. So no sleep while a host sends SOF
//     (usb_serial_jtag_is_connected(); a charger sends none), and a short awake window every 30 s lets a
//     computer plugged in meanwhile enumerate, after which the "usb host" rule keeps the device awake.

#if USE_IRQ_TUNING
static const uint8_t SLEEP_KEEP_PINS[] = { TFT_CS, TFT_DC, TFT_SCLK, TFT_MOSI, BL_PIN, D4, D5 };
#endif

// nullptr = light sleep allowed now; otherwise the first reason it is not.
const char* sleepBlocker(uint32_t now) {
#if !USE_IRQ_TUNING
  (void)now;
  return "no IRQ wire";
#else
  if (!sleepEnabled) return "off";
  if (!sensorOk || sensorBus != BUS_WIRE) return "no sensor";
  if (sensorLost) return "sensor lost";
  if (screenOn) return "screen on";
  if (otaActive || netWanted() || wifiRadioOn) return "wifi";
  if (!sleepUsb && now - usbSeenMs < SLEEP_USB_IDLE_MS) return "usb host";
  if (now - usbListenMs < SLEEP_LISTEN_MS) return "usb listen";
  if (demoActive) return "demo";
  if (burstLeft) return "burst";
  if (ts.down || evCount || digitalRead(TOUCH_PIN) == LOW) return "touch";
  if (f2b.state == F2B_TIMING) return "flash-to-bang";
  if (alertStage == ALERT_SHELTER) return "shelter";
  if (digitalRead(IRQ_PIN) == HIGH) return "irq pending";   // the next pass reads REG0x03 first
  return nullptr;
#endif
}

static double slpPct(uint64_t asleepUs, uint64_t spanMs) {
  return spanMs ? 100.0 * (double)asleepUs / ((double)spanMs * 1000.0) : 0.0;
}

// `status` and `sleep`: switch, override, the rule that blocks sleep now, counters since boot.
void sleepStatus(const char* tag) {
  const char* why = sleepBlocker(millis());
  LOG.printf("%s sleep enabled=%d usbOverride=%d now=%s%s%s sleeps=%lu asleep=%lus (%.1f%% of uptime) wakes timer=%lu irq=%lu touch=%lu other=%lu\n",
             tag, sleepEnabled, sleepUsb, why ? "blocked(" : "allowed", why ? why : "", why ? ")" : "",
             (unsigned long)slpCount, (unsigned long)(slpUs / 1000000ULL), slpPct(slpUs, uptimeMs()),
             (unsigned long)slpWTimer, (unsigned long)slpWIrq, (unsigned long)slpWTouch, (unsigned long)slpWOther);
}

#if USE_IRQ_TUNING
static void lightSleepOnce() {
  const gpio_num_t irq = (gpio_num_t)IRQ_PIN, tp = (gpio_num_t)TOUCH_PIN;
  if (slpWinN == 0) slpWinMs = millis();             // a new [SLEEP] log window starts with its first sleep
  esp_task_wdt_reset();
  for (uint8_t p : SLEEP_KEEP_PINS) gpio_sleep_sel_dis((gpio_num_t)p);
  gpio_wakeup_enable(irq, GPIO_INTR_HIGH_LEVEL);     // also keeps the pin's own config in sleep
  gpio_wakeup_enable(tp, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();
  esp_sleep_enable_timer_wakeup(SLEEP_MAX_MS * 1000ULL);
  int64_t t0 = esp_timer_get_time();
  esp_err_t err = esp_light_sleep_start();           // ESP_ERR_SLEEP_REJECT: a wake source was already active
  uint64_t us = (uint64_t)(esp_timer_get_time() - t0);
  esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
  gpio_wakeup_disable(irq);                          // back to plain inputs: no level interrupt type left behind
  gpio_wakeup_disable(tp);
  gpio_set_intr_type(irq, GPIO_INTR_DISABLE);
  gpio_set_intr_type(tp, GPIO_INTR_DISABLE);
  bool irqHigh = gpio_get_level(irq) == 1;
  bool touchLow = gpio_get_level(tp) == 0;

  slpCount++; slpUs += us;
  slpWinN++; slpWinUs += us;
  if (err != ESP_OK)                         slpWOther++;
  else if (cause == ESP_SLEEP_WAKEUP_TIMER)  slpWTimer++;
  else if (cause == ESP_SLEEP_WAKEUP_GPIO) {
    if (irqHigh) { slpWIrq++; slpWinIrq++; }
    else         { slpWTouch++; slpWinTouch++; }
  }
  else                                       slpWOther++;

  // A GPIO wake that is not the IRQ is a touch, even if the finger is already up: mark INT as seen low now,
  // so the next touchPoll() starts a press and wakes the screen (that press does nothing else).
  bool gpioWake = err == ESP_OK && cause == ESP_SLEEP_WAKEUP_GPIO;
  if (gpioWake && (touchLow || !irqHigh)) { ts.lastLowMs = millis(); ts.seen = true; }
  // IRQ: the next pass polls REG0x03 at once (sensorTick has no throttle); give the chip its 2 ms first.
  if (irqHigh) delayMicroseconds(AS3935_IRQ_WAIT_US);

  uint32_t now = millis();
  if (now - slpWinMs >= SLEEP_LOG_MS) {
    LOG.printf("[SLEEP] last %lu s: %lu sleeps, %.1f%% asleep, wakes irq %lu touch %lu | since boot: %lu sleeps, %.1f%% asleep\n",
               (unsigned long)((now - slpWinMs) / 1000UL), (unsigned long)slpWinN, slpPct(slpWinUs, now - slpWinMs),
               (unsigned long)slpWinIrq, (unsigned long)slpWinTouch, (unsigned long)slpCount, slpPct(slpUs, uptimeMs()));
    slpWinN = 0; slpWinUs = 0; slpWinIrq = 0; slpWinTouch = 0;
  }
}
#endif

// End of every loop pass: the normal rest, or one light sleep.
void restOrSleep() {
  uint32_t now = millis();
  if (usb_serial_jtag_is_connected()) usbSeenMs = now;
  if (now - usbListenMs >= SLEEP_LISTEN_EVERY_MS) usbListenMs = now;
  if (sleepBlocker(now)) { waitSampling(50); return; }
#if USE_IRQ_TUNING
  lightSleepOnce();
#endif
}

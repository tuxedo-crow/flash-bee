// Flash Bee: handheld lightning sensing and ranging device
// Source: "Flash Bee : Handheld Lightning Sensing and Ranging Device" by gokux (Instructables, CC BY-NC-SA)
// Transcribed from the instructable, then fixed:
//   - Wire.begin(D4, D5) instead of Wire.begin(6, 7): runs on XIAO ESP32-C3, -C6 and -S3
//   - AFE gain from datasheet values (original 0x12 was not a valid setting); Indoor/Outdoor
//     switchable on the Settings page, first boot = Indoor
//   - Real PRESET_DEFAULT + CALIB_RCO with status check (original never sent the 0x96 commands)
//   - Antenna (LCO) tuning on the IRQ wire (D2, USE_IRQ_TUNING): swept once, saved in NVS, `tune` console command re-runs it
//   - REG0x02 written with a mask (original clobbered reserved/CL_STAT/MIN_NUM_LIGH bits)
//   - Noise floor now decays after quiet periods (original only ratcheted up)
//   - Sensitivity function names corrected; NF shown on screen
//   - Demo mode is a runtime state: hold a finger still on the screen for 3 s to start/stop fake strikes
//     (magenta "DEMO - NOT REAL" label and border; ends after 10 min, on a real strike, or on power-off).
//     Every boot starts in real mode. Works without an AS3935 too (Main page shows NOT FOUND).
//   - Noise-floor raises rate-limited (NF_RAISE_INTERVAL): INT_NH stays asserted while noisy,
//     so polling used to ramp NF to 7 in half a second
//   - Sensitivity recovery stops at the datasheet defaults (WD_MIN / SR_MIN = 2)
//   - Watchdog/spike write keeps reserved bit 7 of REG0x01 (masked write)
//   - Screen shows time since the last strike; distance dims after 15 min (AS3935 purges its stats)
//   - Storm trend from the last 15 min of in-range strikes: count, APPROACHING / STEADY / MOVING AWAY
//   - Demo distance is a random walk in and out so the trend text shows
//   - Real and demo strike state live in separate structs (realStats / demoStats): fake strikes
//     never touch the real counts, ticker, trend, chart, all-clear or records
//   - Gauge arc and ticks fixed to the same 270 degree sweep: empty at 7:30, full at 4:30, gap at the bottom
//   - Units mi/km chosen on the Settings page (USE_MILES = first-boot default); internals and serial in km
//   - Touch: CHSC6X INT on D7 (LOW while touched) plus I2C coordinates (0x2E) read only while INT is LOW.
//     One input state machine: tap, swipe left/right, 3 s long press (demo), 3 s hold on the Records RESET button
//   - Six pages, swipe to move, dots at the bottom: Main, Storm chart (60 min), Flash-to-bang,
//     Records (NVS, reset by 3 s hold), Settings (gain, units, brightness, screen-off), Nerd stats.
//     A real strike jumps to Main; any other page returns to Main after 2 min without touch
//     (neither while flash-to-bang is timing).
//   - 30-minute all-clear (NWS 30-30 rule) after strikes at <= 10 mi / 16 km, shown on Main
//   - Battery % on Main (Round Display A0 divider on D0), brightness by backlight PWM on D6
//   - Network (net.ino): WiFi, OTA, MQTT, Home Assistant discovery, remote log and commands. Without an SSID
//     there is no radio at all. Operating profiles (profiles.ino) replace the indoor/outdoor gain choice.
//   - Sensor safety: I2C failures counted (lost after 8, retry + full initSensor every 3 s), 60 s config read-back,
//     ENV TOO NOISY fault, distance code 0 = unknown, MIN_NUM_LIGHTNING, two-stage alert (ACTIVITY / SHELTER),
//     reset filters, screen-off timeout. See README "Sensor safety and alerts".
//   - Light sleep (power.ino) while the screen is off: wakes on the AS3935 IRQ (D2), a touch (D7) or after 1 s.
//     See README "Light sleep".

#include <Wire.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <lwip/sockets.h>
#include "esp_mac.h"
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <esp_sleep.h>
#include <esp_timer.h>
#include <esp_heap_caps.h>
#include "driver/gpio.h"
#include "driver/usb_serial_jtag.h"
#include "esp_rom_sys.h"
#include "driver.h"
#include "FlashBee.h"
#include <math.h>

// -- Display -------------------------------------------------------------
TFT_eSPI tft = TFT_eSPI();
TFT_eSprite spr = TFT_eSprite(&tft);
uint16_t sprH = 240;                   // sprite rows actually allocated (see setup)

// -- Strike state ----------------------------------------------------------
Stats realStats;
Stats demoStats;
Stats *st = &realStats;                // the one the UI and recordStrike use
bool demoActive = false;
uint32_t demoStartMs = 0;
bool sensorOk = false;                 // sensor found at boot: gates every AS3935 read/write
SensorBus sensorBus = BUS_NONE;        // which bus found the sensor
bool sensorLost = false;               // found at boot, then stopped answering (SENSOR_LOST_FAILS failures in a row)
uint8_t i2cFails = 0;                  // consecutive failed transfers, reset by any success
uint32_t i2cErrTotal = 0;              // every failed transfer since boot (recovery checks that it did not move)
uint32_t sensorRetryMs = 0;            // last recovery attempt
uint32_t cfgCheckMs = 0;               // last config read-back
bool faultI2c = false;                 // test console: every sensor read and write fails (RAM only)
bool noiseFault = false;               // INT_NH while the noise floor is already at NF_MAX
AlertStage alertStage = ALERT_IDLE;
bool screenOn = true;                  // false = backlight off and panel in sleep
uint32_t lastActMs = 0;                // last strike or wake (touch is lastTouchMs): the screen-off timer
uint32_t filtersMsgUntil = 0;          // "FILTERS RESET" confirmation on the Nerd page

uint8_t noiseFloor = 2;
uint8_t watchdog = 2;
uint8_t spikeRej = 2;
uint32_t senseLastAdj = 0;
uint32_t nfLastAdj = 0;
uint32_t nfLastRaise = 0;
uint8_t tunCap = TUN_CAP_DEFAULT;
uint32_t lcoHz = 0;             // measured antenna frequency (0 = not measured)
uint8_t tunSaved = 0xFF;        // TUN_CAP loaded from NVS (0xFF = never tuned)
bool tuneOk = false;            // the last sweep measured pulses
bool irqWired = false;          // pulses were measured on IRQ (this boot's sweep, or a saved sweep): D2 is a real IRQ wire
bool irqHighSeen = false;       // IRQ read HIGH on the last poll pass ...
uint32_t irqHighUs = 0;         // ... and since when (micros): REG0x03 is read only 2 ms after the edge
uint32_t sensorReadMs = 0;      // last REG0x03 read (IRQ mode: the slow fallback poll runs from here)
bool uiDirty = true;            // something changed that a static page must redraw now (see drawUI)
bool rcoCalOk = false;
uint8_t calTrco = 0, calSrco = 0;
uint32_t disturbers = 0;        // since boot
uint32_t noiseEvents = 0;       // rate-limited INT_NH reactions since boot
uint32_t lastFlashMs = 0;

// -- UI / input state ------------------------------------------------------
Page curPage = PAGE_MAIN;
uint32_t lastTouchMs = 0;        // any touch activity (down, held, up)
char splashText[32] = "";        // owner's dedication line (NVS key "splash", max 31 chars); empty = title only
uint32_t splashUntil = 0;        // `splash show`: drawUIFrame() draws the splash instead of the page until this millis() (0 = off)
TouchState ts;
TouchEvent evQ[EVQ_LEN];
uint8_t evHead = 0, evCount = 0;
FlashToBang f2b;
uint32_t resetMsgUntil = 0;      // "RECORDS RESET" confirmation on Records

// -- Settings / records (NVS) -----------------------------------------------
Preferences prefs;
bool prefsOk = false;
uint32_t bootCount = 0, crashCount = 0;   // restart diagnostics (NVS), never cleared by a records reset
uint8_t lastCrash = 0;                    // esp_reset_reason_t of the last crash, 0 = none
Settings cfg = { 0, USE_MILES, 2, 0, 0, SCR_DEFAULT };   // indoor / mnl: live values, set by the profile
Settings savedCfg = cfg;
Records rec = {};
Records savedRec = {};
uint32_t nvsLastWrite = 0;
bool stormSeen = false;          // current storm (RAM only; a reboot starts a new one)
uint32_t stormLastMs = 0;
uint32_t stormCount = 0;

// -- Battery ---------------------------------------------------------------
uint32_t battMv = 0;             // smoothed pin mV, 0 = no reading
int battPct = -1;                // -1 = no reading
uint32_t battLastRead = 0;
uint32_t battLastLog = 0;
bool battEverRead = false;
uint32_t battRawMv = 0;

// -- Serial test console ------------------------------------------------------
char conBuf[CON_LINE_MAX + 1];
uint8_t conLen = 0;
bool conOverflow = false;               // current line too long: drop it up to the next newline
uint32_t burstLeft = 0;                 // strikes still to inject
bool sensorMuted = false;               // test console: ignore sensor strikes (RAM only, a reboot clears it)
uint32_t burstIntervalMs = 0;
uint32_t burstNextMs = 0;
const uint8_t DIST_CODES[15] = {1, 5, 6, 8, 10, 12, 14, 17, 20, 24, 27, 31, 34, 37, 40};   // valid AS3935 km codes (plus DIST_OOR)

// -- Health line (per-minute loop statistics) ---------------------------------
uint32_t loopStartUs = 0;
uint32_t healthLastMs = 0;
uint32_t healthLoops = 0;
uint32_t healthMaxUs = 0;
uint64_t healthSumUs = 0;

// -- Light sleep (power.ino; counters are RAM only, since boot) ---------------------
bool sleepEnabled = true;              // console `sleep on|off` (not saved)
bool sleepUsb = false;                 // console `sleep usb on`: sleep with a USB host attached (test; the host loses the port)
uint32_t usbSeenMs = 0;                // last loop pass that saw SOF from a USB host
uint32_t usbListenMs = 0;              // start of the last USB listen window
uint32_t slpCount = 0, slpWTimer = 0, slpWIrq = 0, slpWTouch = 0, slpWOther = 0;   // sleeps = sum of the wakes
uint64_t slpUs = 0;                    // time asleep
uint32_t slpWinMs = 0, slpWinN = 0, slpWinIrq = 0, slpWinTouch = 0;   // the current [SLEEP] log window
uint64_t slpWinUs = 0;

// -- Uptime (64-bit, survives the 49.7-day millis wrap) ----------------------
uint32_t upLast = 0, upHi = 0;

// -- Remote log (every LOG.print goes to Serial; lines also queue for MQTT) -------
LogPrint LOG;

// -- Operating profiles ----------------------------------------------------------
const ProfileDef PROFILES[PROF_COUNT] = {
  { "station",    2, 2, 1, 1, 0 },     // WD SR MNL MASK_DIST freeze
  { "portable",   2, 2, 0, 1, 0 },
  { "stormchase", 1, 0, 0, 0, 1 },
};
const uint8_t MNL_STRIKES[4] = { 1, 5, 9, 16 };      // strikes in 15 min before the chip raises INT_L
const char* const WIFI_NAME[4] = { "off", "connecting", "up", "retry" };
const char* const PMODE_NAME[PM_COUNT] = { "auto", "station", "portable", "stormchase" };
ProfileMode profMode = PM_AUTO;        // mode in use this run (STORMCHASE is never saved)
ProfileMode profSaved = PM_AUTO;       // what NVS holds
Profile profEff = PROF_PORTABLE;       // what is applied to the chip
bool profPending = false;              // AUTO at boot with an SSID: STATION until PROF_BOOT_PENDING_MS without WiFi
uint32_t profBootMs = 0;
bool profOverride = false;             // set gain / set mnl changed a knob since the last profile apply
bool profAssoc = false;                // association state AUTO is tracking, and since when
uint32_t profAssocSince = 0;
uint8_t reg3Last = 0;                  // REG0x03 as the last poll read it (reading it clears the interrupt: never read it twice)
bool reg3Ok = false;

// -- Network ---------------------------------------------------------------------
NetCfg ncfg;
char devId[7] = "000000";              // last 3 bytes of the STA MAC, lowercase hex
char devHost[20] = "flashbee-000000";
WifiState wifiState = WIFI_ST_OFF;
bool wifiRadioOn = false;
uint32_t wifiAttemptMs = 0, wifiRetryAt = 0, wifiBackoff = WIFI_BACKOFF_MIN;
bool mdnsOk = false;
bool otaRunning = false;               // ArduinoOTA.begin() done
bool otaActive = false;                // an update is in progress (UPDATING screen)
bool otaRestart = false;               // OTA settings changed: restart the service
uint8_t otaPct = 0;
uint32_t otaDrawMs = 0;
WiFiClient netClient;
PubSubClient mqtt(netClient);
volatile MqttState mqttState = MQTT_ST_OFF;   // written by the net task, read by the loop (UI only)
volatile bool mqttResetReq = false;    // loop -> task: settings changed, reconnect now
volatile bool netTaskRun = false;      // loop -> task: WiFi is up and an MQTT host is set
volatile uint32_t mqttConnSeq = 0;     // task: +1 on every new connection (the loop then republishes discovery and state)
volatile uint32_t netBusySince = 0;    // task: millis() when the current network call began, 0 = idle (stall guard)
// Crash breadcrumb: plain stores only. Lives in RTC memory, so a watchdog/panic reset keeps it; power loss does not.
RTC_NOINIT_ATTR Crumb crumb;
Crumb crumbSaved = {};                 // the crumb of the last crash (NVS), magic 0 = none
RTC_NOINIT_ATTR HeapRec heapRec;       // heap low-water ring (see heapWatch in console.ino)
RTC_NOINIT_ATTR HeapFail heapFail;
uint32_t heapLastMin = 0xFFFFFFFFUL;   // lowest minimum already recorded this boot
uint32_t heapFailSeen = 0;             // heapFail.count already printed
uint8_t heapPrevStage = 0;
volatile uint8_t netOp = 0;            // task: what the last network call was (NetOp); valid while netBusySince != 0
#define CRUMB(s)     do { crumb.stage = (s); crumb.page = (uint8_t)curPage; crumb.net = netBusySince != 0; } while (0)
#define CRUMB_EVT(e) (crumb.evt = (e))
volatile int netSockFd = -1;          // task: socket fd while connected, else -1 (the loop may only shutdown() it)
volatile uint32_t netStallS = 0;       // test hook `fault netstall <s>`: the task blocks this long
uint32_t mqttSeenSeq = 0;
uint32_t mqttStateMs = 0;              // last state publish
bool mqttStateDirty = false;
int8_t discIdx = -1;                   // next discovery document to publish, -1 = none pending
char tState[NET_TOPIC_MAX], tStrike[NET_TOPIC_MAX], tLog[NET_TOPIC_MAX];   // loop side; the task gets avail/cmd through msnap
MqttSnap msnap;
SemaphoreHandle_t netMtx = nullptr;    // guards msnap
QueueHandle_t netOutQ = nullptr;       // loop -> task: NetMsg
QueueHandle_t netCmdQ = nullptr;       // task (and the mqttcmd test hook) -> loop: console lines
QueueHandle_t netLogQ = nullptr;       // task -> loop: log lines (LOG is not thread-safe)
TaskHandle_t netTaskH = nullptr;
AlertStage mqttLastAlert = ALERT_IDLE;   // values behind the last state publish (change = publish at once)
uint8_t mqttLastProf = 255, mqttLastMode = 255, mqttLastSensor = 255;
bool mqttLastFault = false;
bool discRemove = false;               // `ha off` while connected: publish empty retained documents
char conRaw[CON_LINE_MAX + 1];         // the console line before tokenizing: wifi / mqtt values take the rest of it
bool conFromMqtt = false;              // the line being executed came from <pfx>/cmd

// -- Units -----------------------------------------------------------------
const char* distUnit()      { return cfg.miles ? "mi" : "km"; }
const char* distMaxStr()    { return cfg.miles ? ">25" : ">40"; }
const char* distOverheadStr() { return cfg.miles ? "< 1 mi" : "< 1 km"; }
// AS3935 km code -> number shown on screen
int distForDisplay(uint8_t km) { return cfg.miles ? (int)(km * 0.621371f + 0.5f) : (int)km; }

// -- AS3935 helpers ------------------------------------------------------
// Bit-banged open-drain I2C master for the microSD-slot wiring. The ESP32-C3 has one I2C
// controller and Wire uses it. D9 belongs to the SPI bus (TFT_MISO), so use gpio_* directly:
// pinMode on arduino-esp32 3.x would detach the pin from SPI. Lines are released (high) when idle.
static inline void sdLine(gpio_num_t p, int level) { gpio_set_level(p, level); esp_rom_delay_us(SOFTI2C_HALF_US); }
#define SDA_G ((gpio_num_t)SD_SDA_PIN)
#define SCL_G ((gpio_num_t)SD_SCL_PIN)

void softI2cInit() {
  gpio_set_direction(SDA_G, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_direction(SCL_G, GPIO_MODE_INPUT_OUTPUT_OD);
  gpio_set_level(SDA_G, 1);
  gpio_set_level(SCL_G, 1);
}

static void softStart() {   // also a repeated start: SDA falls while SCL is high
  sdLine(SDA_G, 1); sdLine(SCL_G, 1);
  sdLine(SDA_G, 0); sdLine(SCL_G, 0);
}

static void softStop() {
  sdLine(SDA_G, 0); sdLine(SCL_G, 1); sdLine(SDA_G, 1);
}

static bool softWrite(uint8_t b) {   // true = ACK
  for (int i = 7; i >= 0; i--) {
    sdLine(SDA_G, (b >> i) & 1);
    sdLine(SCL_G, 1); sdLine(SCL_G, 0);
  }
  sdLine(SDA_G, 1);                  // release for the ACK bit
  sdLine(SCL_G, 1);
  bool ack = gpio_get_level(SDA_G) == 0;
  sdLine(SCL_G, 0);
  return ack;
}

static uint8_t softRead() {          // reads one byte, answers NACK (last byte)
  uint8_t v = 0;
  sdLine(SDA_G, 1);
  for (int i = 0; i < 8; i++) {
    sdLine(SCL_G, 1);
    v = (v << 1) | gpio_get_level(SDA_G);
    sdLine(SCL_G, 0);
  }
  sdLine(SDA_G, 1); sdLine(SCL_G, 1); sdLine(SCL_G, 0);   // NACK
  return v;
}

// Every transfer reports success. A run of SENSOR_LOST_FAILS failures marks the sensor lost.
// While lost the counter keeps running but changes no state: the recovery path decides.
static void i2cNote(bool ok) {
  if (ok) { i2cFails = 0; return; }
  i2cErrTotal++;
  if (i2cFails < 255) i2cFails++;
  if (i2cFails >= SENSOR_LOST_FAILS && sensorOk && !sensorLost) {
    sensorLost = true;
    sensorRetryMs = millis();
    LOG.println("[I2C] sensor lost");
  }
}

static bool rawRead(uint8_t reg, uint8_t &v) {
  if (sensorBus == BUS_SD) {
    bool ok = false;
    softStart();
    if (softWrite(AS3935_ADDR << 1) && softWrite(reg)) {
      softStart();
      if (softWrite((AS3935_ADDR << 1) | 1)) { v = softRead(); ok = true; }
    }
    softStop();
    return ok;
  }
  Wire.beginTransmission(AS3935_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(AS3935_ADDR, 1) != 1 || !Wire.available()) return false;
  v = Wire.read();
  return true;
}

static bool rawWrite(uint8_t reg, uint8_t val) {
  if (sensorBus == BUS_SD) {
    bool ok = false;
    softStart();
    if (softWrite(AS3935_ADDR << 1) && softWrite(reg)) ok = softWrite(val);
    softStop();
    return ok;
  }
  Wire.beginTransmission(AS3935_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission(true) == 0;
}

// true = value read. No traffic at all when no sensor was found at boot.
bool readRegOk(uint8_t reg, uint8_t &v) {
  if (!sensorOk) return false;
  bool ok = !faultI2c && rawRead(reg, v);
  i2cNote(ok);
  return ok;
}

bool writeRegOk(uint8_t reg, uint8_t val) {
  if (!sensorOk) return false;
  bool ok = !faultI2c && rawWrite(reg, val);
  i2cNote(ok);
  return ok;
}

uint8_t readReg(uint8_t reg) {          // 0xFF on failure: only for callers that do not care
  uint8_t v = 0xFF;
  readRegOk(reg, v);
  return v;
}

void writeReg(uint8_t reg, uint8_t val) { writeRegOk(reg, val); }

// Read-modify-write. Writes nothing when the read failed (it would clobber the other bits with 0xFF).
bool maskWrite(uint8_t reg, uint8_t mask, uint8_t val) {
  uint8_t cur;
  if (!readRegOk(reg, cur)) return false;
  return writeRegOk(reg, (cur & ~mask) | (val & mask));
}

// REG0x03 write without a read (a read clears a pending interrupt). Bits 7:6 LCO_FDIV, 5 MASK_DIST, 4 reserved (0).
bool writeReg3(uint8_t lcoFdiv, uint8_t maskDist) {
  uint8_t v = (uint8_t)((lcoFdiv << 6) | ((maskDist & 1) << 5));
  if (!writeRegOk(0x03, v)) return false;
  reg3Last = (reg3Last & 0x1F) | v;                  // `profile` prints the new MASK_DIST; the INT bits stay as polled
  return true;
}

bool readEnergyOk(uint32_t &e) {
  uint8_t lo, mid, hi;
  if (!readRegOk(0x06, hi) || !readRegOk(0x05, mid) || !readRegOk(0x04, lo)) return false;
  e = ((uint32_t)(hi & 0x1F) << 16) | ((uint32_t)mid << 8) | lo;
  return true;
}

uint8_t afeValue() { return cfg.indoor ? AFE_INDOOR : AFE_OUTDOOR; }

// AFE_GB is REG0x00 bits 5:1; bit 0 (PWD) must stay 0 or the sensor powers down.
void applyGain() {
  if (!sensorOk) return;
  maskWrite(0x00, 0x3F, afeValue() & 0x3E);
}

void pushTick(uint32_t e) {
  st->tickEnergy[st->tickHead] = e;
  st->tickHead = (st->tickHead + 1) % MAX_TICKS;
  if (st->tickCount < MAX_TICKS) st->tickCount++;
}

void applyMnl() {                       // MIN_NUM_LIGH = REG0x02 bits 5:4; bits 7:6 and 3:0 stay
  if (!sensorOk) return;
  maskWrite(0x02, 0x30, (cfg.mnl & 3) << 4);
}

void applyWatchdogSpike() {
  maskWrite(0x01, 0x7F, (noiseFloor << 4) | (watchdog & 0x0F));   // keep reserved bit 7
  maskWrite(0x02, 0x0F, spikeRej & 0x0F);
}

// Higher WDTH/SREJ = more disturber rejection = LESS sensitive.
// (Original names were inverted: "increaseSensitivity" actually reduced it.)
void desensitize() {
  if (PROFILES[profEff].freeze) return;              // STORMCHASE: sensitivity is frozen
  if (spikeRej < watchdog) {
    if (spikeRej < 11) { spikeRej++; LOG.print("[TUNE] Spike up: "); LOG.println(spikeRej); }
  } else {
    if (watchdog < 10) { watchdog++; LOG.print("[TUNE] WD up: "); LOG.println(watchdog); }
  }
  applyWatchdogSpike();
}

// Lowers the higher of SREJ/WDTH; if that one is at its minimum, lowers the other.
// Never goes below the datasheet defaults. Does nothing (no write, no log) when both are at min.
void resensitize() {
  if (PROFILES[profEff].freeze) return;              // STORMCHASE: sensitivity is frozen
  uint8_t wdMin = PROFILES[profEff].wd, srMin = PROFILES[profEff].sr;   // the profile's values are the floor
  bool srHigher = spikeRej > watchdog;
  bool lowerSr;
  if (srHigher && spikeRej > srMin)        lowerSr = true;
  else if (!srHigher && watchdog > wdMin)  lowerSr = false;
  else if (spikeRej > srMin)               lowerSr = true;
  else if (watchdog > wdMin)               lowerSr = false;
  else return;

  LOG.println("[TUNE] Quiet - restoring sensitivity");
  if (lowerSr) { spikeRej--; LOG.print("[TUNE] Spike dn: "); LOG.println(spikeRej); }
  else         { watchdog--; LOG.print("[TUNE] WD dn: "); LOG.println(watchdog); }
  applyWatchdogSpike();
}

void raiseNoiseFloor() {
  if (noiseFloor < 7) {
    noiseFloor++;
    maskWrite(0x01, 0x70, noiseFloor << 4);
    LOG.print("[NF] Raised to ");
    LOG.println(noiseFloor);
  }
}

// Original only ever raised the noise floor, so sensitivity ratcheted down until reboot.
void lowerNoiseFloor() {
  if (noiseFloor > NF_MIN) {
    noiseFloor--;
    maskWrite(0x01, 0x70, noiseFloor << 4);
    LOG.print("[NF] Lowered to ");
    LOG.println(noiseFloor);
    if (noiseFault && noiseFloor < NF_MAX) {        // the floor fell below its maximum: readings count again
      noiseFault = false;
      LOG.println("[NH] noise fault cleared");
    }
  }
}

// -- AS3935 init / calibration -------------------------------------------
bool sensorPresent() {
  if (faultI2c) return false;
  if (sensorBus == BUS_SD) {
    softStart();
    bool ack = softWrite(AS3935_ADDR << 1);
    softStop();
    return ack;
  }
  Wire.beginTransmission(AS3935_ADDR);
  return Wire.endTransmission() == 0;   // ACK
}

// Direct commands: write 0x96 to the command register.
void presetDefault() { writeReg(0x3C, 0x96); delay(2); }

// CALIB_RCO, then pulse DISP_SRCO for 2 ms (datasheet procedure).
// Must run after antenna tuning: the RCOs are calibrated against the LCO.
bool calibrateRCO() {
  writeReg(0x3D, 0x96);
  delay(2);
  maskWrite(0x08, 0x40, 0x40);
  delay(2);
  maskWrite(0x08, 0x40, 0x00);
  delay(2);
  calTrco = readReg(0x3A);
  calSrco = readReg(0x3B);
  bool ok = (calTrco & 0x80) && !(calTrco & 0x40) && (calSrco & 0x80) && !(calSrco & 0x40);
  LOG.printf("[CAL] TRCO 0x%02X SRCO 0x%02X -> %s\n", calTrco, calSrco, ok ? "OK" : "FAIL");
  return ok;
}

#if USE_IRQ_TUNING
volatile uint32_t lcoPulses = 0;
void IRAM_ATTR onLcoPulse() { lcoPulses = lcoPulses + 1; }

// Output LCO/128 on IRQ, count pulses for each TUN_CAP, keep the one closest to 500 kHz.
// Returns false (cap and hz untouched) when no cap gave a single pulse: IRQ is not connected.
bool tuneAntenna(uint8_t &bestCap, uint32_t &bestHz) {
  pinMode(IRQ_PIN, INPUT);
  writeReg3(3, PROFILES[profEff].maskDist);   // LCO_FDIV = 128
  uint8_t best = 0;
  uint32_t bestErr = UINT32_MAX, hzBest = 0;
  bool any = false;
  for (uint8_t cap = 0; cap < 16; cap++) {
    // 16 x 220 ms: keep the 8 s task watchdog fed. At boot the loop task is not subscribed yet, and an
    // unguarded reset logs "task_wdt: task not found" on every step.
    if (esp_task_wdt_status(NULL) == ESP_OK) esp_task_wdt_reset();
    maskWrite(0x08, 0x0F, cap);
    maskWrite(0x08, 0x80, 0x80);            // DISP_LCO on
    delay(20);
    lcoPulses = 0;
    uint32_t t0 = micros();
    attachInterrupt(digitalPinToInterrupt(IRQ_PIN), onLcoPulse, RISING);
    delay(200);
    detachInterrupt(digitalPinToInterrupt(IRQ_PIN));
    uint32_t dt = micros() - t0;
    maskWrite(0x08, 0x80, 0x00);            // DISP_LCO off
    uint32_t hz = (uint32_t)((uint64_t)lcoPulses * 128ULL * 1000000ULL / dt);
    uint32_t err = hz > 500000 ? hz - 500000 : 500000 - hz;
    LOG.printf("[ANT] cap %2u: %lu Hz\n", cap, (unsigned long)hz);
    if (hz) any = true;
    if (err < bestErr) { bestErr = err; best = cap; hzBest = hz; }
  }
  writeReg3(0, PROFILES[profEff].maskDist);   // LCO_FDIV back to 16
  if (!any) {
    LOG.println("[ANT] no pulses on IRQ - wire not connected? keeping the old TUN_CAP");
    return false;
  }
  bestCap = best;
  bestHz = hzBest;
  LOG.printf("[ANT] Best cap %u: %lu Hz (%.1f%%)\n", best, (unsigned long)hzBest,
                100.0 * ((double)hzBest - 500000.0) / 500000.0);
  return true;
}
#endif

// -- Storm statistics ------------------------------------------------------
// In-range strikes (known distance, not out of range) in the last STALE_MS (n) and the distance trend.
// capped: the history is full and its oldest entry is still inside the window, so n may be short.
// Trend: least-squares line through the in-range distances of the last 10 min (TREND_WINDOW_MS);
// needs >= 3 strikes spanning >= 2 min. The fitted change across that span decides the result.
Trend stormStats(const Stats &S, uint32_t now, int &n, bool &capped) {
  n = 0;
  bool oldestInWindow = false;
  for (int i = 0; i < S.histCount; i++) {
    int idx = (S.histHead - S.histCount + i + HIST_MAX) % HIST_MAX;
    if (now - S.hist[idx].t > STALE_MS) continue;
    if (i == 0) oldestInWindow = true;
    if (S.hist[idx].d == DIST_OOR || S.hist[idx].d == DIST_UNKNOWN) continue;
    n++;
  }
  capped = (S.histCount == HIST_MAX) && oldestInWindow;
  // Pass 1: count, mean x (s before now) and mean distance of in-range strikes in the trend window.
  int m = 0;
  uint32_t tOld = 0, tNew = 0;
  float sx = 0, sy = 0;
  for (int i = 0; i < S.histCount; i++) {
    int idx = (S.histHead - S.histCount + i + HIST_MAX) % HIST_MAX;
    uint32_t age = now - S.hist[idx].t;
    if (age > TREND_WINDOW_MS || S.hist[idx].d == DIST_OOR || S.hist[idx].d == DIST_UNKNOWN) continue;
    if (m == 0) tOld = S.hist[idx].t;
    tNew = S.hist[idx].t;
    sx += -(float)age / 1000.0f;
    sy += (float)S.hist[idx].d;
    m++;
  }
  if (m < TREND_MIN_STRIKES || tNew - tOld < TREND_MIN_SPAN_MS) return TREND_NONE;
  float mx = sx / m, my = sy / m;
  // Pass 2: slope = Sxy / Sxx.
  float sxx = 0, sxy = 0;
  for (int i = 0; i < S.histCount; i++) {
    int idx = (S.histHead - S.histCount + i + HIST_MAX) % HIST_MAX;
    uint32_t age = now - S.hist[idx].t;
    if (age > TREND_WINDOW_MS || S.hist[idx].d == DIST_OOR || S.hist[idx].d == DIST_UNKNOWN) continue;
    float dx = -(float)age / 1000.0f - mx;
    sxx += dx * dx;
    sxy += dx * ((float)S.hist[idx].d - my);
  }
  if (sxx <= 0) return TREND_NONE;
  float change = (sxy / sxx) * ((float)(tNew - tOld) / 1000.0f);  // km across the span
  if (change <= -TREND_DELTA_KM) return TREND_APPROACH;
  if (change >= TREND_DELTA_KM) return TREND_AWAY;
  return TREND_STEADY;
}

const char* trendText(Trend t) {
  switch (t) {
    case TREND_APPROACH: return "APPROACHING";
    case TREND_AWAY:     return "MOVING AWAY";
    case TREND_STEADY:   return "STEADY";
    default:             return "";
  }
}

// Advance the 5-min bin ring to the bin that contains `now` (wrap-safe: differences only).
void binsAdvance(Stats &S, uint32_t now) {
  if (!S.binInit) {
    memset(S.binCnt, 0, sizeof(S.binCnt));
    S.binHead = 0;
    S.binStartMs = now;
    S.binInit = true;
    return;
  }
  if (now - S.binStartMs >= BIN_MS * BIN_COUNT) {     // long gap: everything is old
    memset(S.binCnt, 0, sizeof(S.binCnt));
    S.binStartMs = now;
    return;
  }
  while (now - S.binStartMs >= BIN_MS) {
    S.binHead = (S.binHead + 1) % BIN_COUNT;
    S.binCnt[S.binHead] = 0;
    S.binStartMs += BIN_MS;
  }
}

// millis() wraps after 49.7 days, which would make old strikes look recent. Every pass: drop history older
// than the longest window (CHART_MS) and cap the age of the last strike at 2^30 ms (12 days), far over STALE_MS.
void ageCap(Stats &S, uint32_t now) {
  while (S.histCount > 0) {
    int idx = (S.histHead - S.histCount + HIST_MAX) % HIST_MAX;      // oldest
    if (now - S.hist[idx].t <= CHART_MS) break;
    S.histCount--;
  }
  if (now - S.lastStrikeMs > 0x40000000UL) S.lastStrikeMs = now - 0x40000000UL;
}

// -- 30-30 all-clear -------------------------------------------------------
void clearOnStrike(Stats &S, uint8_t d, uint32_t now, bool demo) {
  if (d == DIST_OOR || d == DIST_UNKNOWN || d > CLEAR_KM) return;   // no usable distance: cannot start the 30-30 countdown
  bool restart = S.clearRunning;
  S.clearRunning = true;
  S.clearFromMs = now;
  S.clearDoneShow = false;
  LOG.printf("%s[CLEAR] %s: 30:00 (strike at %u km)\n", demo ? "[DEMO] " : "",
                restart ? "countdown restarted" : "countdown started", d);
}

void clearTick(Stats &S, uint32_t now, bool demo) {
  if (S.clearRunning && now - S.clearFromMs >= CLEAR_MS) {
    S.clearRunning = false;
    S.clearDoneShow = true;
    S.clearDoneMs = now;
    LOG.printf("%s[CLEAR] ALL CLEAR: 30 min since the last close strike\n", demo ? "[DEMO] " : "");
  }
  if (S.clearDoneShow && now - S.clearDoneMs >= CLEAR_SHOW_MS) S.clearDoneShow = false;
}

// Record one strike (real or demo) and flash the screen.
void recordStrike(uint32_t e, uint8_t d) {
  CRUMB_EVT(CE_STRIKE);
  uint32_t now = millis();
  screenWake("strike");                  // before the flash, so the flash is seen
  st->strikeCount++;
  st->lastEnergy = e;
  st->lastDist = d;
  st->lastStrikeMs = now;
  if (e > st->maxEnergy) st->maxEnergy = e;
  pushTick(e);
  st->hist[st->histHead].t = now;        // out-of-range strikes are kept too (chart bins);
  st->hist[st->histHead].e = e;          // trend and dots skip them
  st->hist[st->histHead].d = d;
  st->histHead = (st->histHead + 1) % HIST_MAX;
  if (st->histCount < HIST_MAX) st->histCount++;
  binsAdvance(*st, now);
  if (st->binCnt[st->binHead] < 0xFFFF) st->binCnt[st->binHead]++;
  int n;
  bool capped;
  Trend tr = stormStats(*st, now, n, capped);
  if (demoActive) LOG.print("[DEMO] ");
  LOG.print("#"); LOG.print(st->strikeCount);
  if (d == DIST_UNKNOWN) {
    LOG.print(" D:unknown");
  } else {
    LOG.print(" D:"); LOG.print(st->lastDist);
    LOG.print("km");
  }
  if (cfg.miles && d != DIST_OOR && d != DIST_UNKNOWN) {
    LOG.print(" ("); LOG.print(distForDisplay(st->lastDist)); LOG.print("mi)");
  }
  LOG.print(" E:");
  LOG.print(st->lastEnergy);
  LOG.print(" "); LOG.print(n); LOG.print(capped ? "+" : ""); LOG.print(" in 15m");
  if (tr != TREND_NONE) { LOG.print(" "); LOG.print(trendText(tr)); }
  LOG.println();
  clearOnStrike(*st, d, now, demoActive);
  alertUpdate(now);                      // a rise to SHELTER is logged right after the strike line
  flashScreen();
}

// A real strike (sensor or test console): ends demo, records it, updates records, jumps to Main.
void onRealStrike(uint32_t e, uint8_t d) {
  uiDirty = true;
  if (demoActive) exitDemo("real strike");   // real data wins: record it into realStats
  recordStrike(e, d);
  recordsOnStrike(e, d, millis());
  mqttQueueStrike(e, d);                     // real strikes only: the demo never reaches MQTT
  if (curPage != PAGE_MAIN && f2b.state != F2B_TIMING) setPage(PAGE_MAIN, "real strike");
}

// Demo on: fresh empty demoStats, UI switches to it. Real stats are left alone.
void enterDemo() {
  memset(&demoStats, 0, sizeof(demoStats));
  demoStats.demoKm = random(30, 41);
  demoStats.demoDir = -1;
  demoStats.demoNextStrike = millis() + 3000;
  demoStartMs = millis();
  demoActive = true;
  st = &demoStats;
  LOG.println("[DEMO] on");
}

// Demo off: back to realStats untouched. Without a sensor, Main shows NOT FOUND again.
void exitDemo(const char* reason) {
  demoActive = false;
  st = &realStats;
  LOG.printf("[DEMO] off (reason: %s)\n", reason);
}

// Fake strike every ~4-10 s, using the same distance codes the AS3935 reports.
// Distance is a random walk (1-4 km steps) that drifts in to 1 km, then back out to 40 km,
// so the APPROACHING / MOVING AWAY trend shows. Occasional out-of-range and unknown-distance strike.
void demoTick() {
  if ((int32_t)(millis() - demoStats.demoNextStrike) < 0) return;
  demoStats.demoNextStrike = millis() + random(4000, 10001);
  static const uint8_t dists[] = {1, 5, 6, 8, 10, 12, 14, 17, 20, 24, 27, 31, 34, 37, 40};
  uint8_t d;
  if (random(10) == 0) {
    d = DIST_OOR;
  } else if (random(15) == 0) {
    d = DIST_UNKNOWN;
  } else {
    demoStats.demoKm += demoStats.demoDir * (int)random(1, 5);
    if (demoStats.demoKm <= 1)  { demoStats.demoKm = 1;  demoStats.demoDir = 1; }
    if (demoStats.demoKm >= 40) { demoStats.demoKm = 40; demoStats.demoDir = -1; }
    d = dists[0];
    for (size_t i = 1; i < sizeof(dists); i++)      // snap to nearest valid code
      if (abs((int)dists[i] - demoStats.demoKm) < abs((int)d - demoStats.demoKm)) d = dists[i];
  }
  float r = random(1000) / 1000.0f;                 // squared: mostly small, some big
  uint32_t e = 5000 + (uint32_t)(r * r * 2090000.0f);   // 20-bit range
  LOG.printf("[DEMO] strike dist=%u energy=%lu\n", d, (unsigned long)e);
  recordStrike(e, d);
}

// -- Flash on lightning ----------------------------------------------------
void flashScreen() {
  if (!screenOn) { lastFlashMs = millis(); return; }
  CRUMB(CS_PUSH);
  for (int i = 0; i < 2; i++) {
    spr.fillSprite(0x2945);
    spr.pushSprite(0, 0);
    delay(35);
    spr.fillSprite(C_BG);
    spr.pushSprite(0, 0);
    delay(20);
  }
  lastFlashMs = millis();
}

// -- Boot screen (drawn straight to the TFT, before the loop runs) -----------
void drawBoot(const char* msg, uint16_t color) {
  tft.fillScreen(C_BG);
  tft.drawCircle(CX, CY, 118, C_DIMRING);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(C_GOLD, C_BG);
  tft.setTextSize(2);
  tft.drawString("AS3935", CX, CY - 14);
  tft.setTextSize(1);
  tft.setTextColor(color, C_BG);
  tft.drawString(msg, CX, CY + 12);
}

// -- Splash (boot and `splash show`) --------------------------------------------
// Draws onto any TFT_eSPI target: `tft` at boot, `spr` for `splash show` (so `shot` can capture it).
// The glass is a circle (radius 118 around CX,CY), so every line stays within its chord: at size 2 a line is
// at most 16 chars (192 px). Longer dedications wrap at the first space where both lines fit; if none does
// (or there is no space) the text drops to size 1 on one line (31 chars = 186 px).
void drawSplash(TFT_eSPI &g) {
  const char* txt = splashText;
  size_t len = strlen(txt);
  int dy = len ? 0 : 14;                               // title only: centre the group
  g.fillScreen(C_BG);
  g.drawCircle(CX, CY, 118, C_DIMRING);
  g.setTextDatum(MC_DATUM);
  g.fillTriangle(CX + 6, 34 + dy, CX - 8, 56 + dy, CX + 1, 56 + dy, C_GOLD);   // lightning bolt
  g.fillTriangle(CX - 1, 50 + dy, CX + 9, 50 + dy, CX - 5, 72 + dy, C_GOLD);
  g.setTextColor(C_GOLD, C_BG);
  g.setTextSize(3);
  g.drawString("FLASH BEE", CX, 92 + dy);
  if (len) {
    g.setTextColor(C_WHITE, C_BG);
    if (len <= 16) {
      g.setTextSize(2);
      g.drawString(txt, CX, 138);
    } else {
      int best = -1;                                   // split at the first space where both halves fit, so a name
      for (size_t i = 1; i + 1 < len; i++) {           // after "for" stays together: "for" / "Some Long Name"
        if (txt[i] == ' ' && i <= 16 && len - i - 1 <= 16) { best = (int)i; break; }
      }
      if (best > 0) {
        char l1[17], l2[17];
        memcpy(l1, txt, best); l1[best] = '\0';
        strlcpy(l2, txt + best + 1, sizeof(l2));
        g.setTextSize(2);
        g.drawString(l1, CX, 128);
        g.drawString(l2, CX, 152);
      } else {
        g.setTextSize(1);
        g.drawString(txt, CX, 138);
      }
    }
  }
  g.setTextSize(1);
}

uint64_t uptimeMs() {
  uint32_t m = millis();
  if (m < upLast) upHi++;
  upLast = m;
  return ((uint64_t)upHi << 32) | m;
}

// -- Sensor init (boot and recovery share it) -------------------------------------
// PRESET_DEFAULT, gain, antenna, RCO calibration, then the detection thresholds at their defaults and
// the saved MIN_NUM_LIGH. `tune` runs the (slow, ~4 s) antenna sweep and saves the result in NVS: it is for the
// first boot and the `tune` command; later calls re-apply the saved TUN_CAP. Returns true if no transfer failed.
// maskWrite keeps reserved / CL_STAT bits (the original overwrote all of REG0x02).
bool initSensor(bool tune) {
  uint32_t errs = i2cErrTotal;
  presetDefault();                                   // reset all registers to defaults

  applyGain();                                       // AFE gain: OUTDOOR from the profile (or the live override)
  LOG.printf("[AS3935] AFE 0x%02X (%s), profile %s\n", afeValue(), cfg.indoor ? "indoor" : "outdoor", PROFILES[profEff].name);

#if USE_IRQ_TUNING
  if (tune) {
    drawBoot("TUNING ANTENNA...", C_GREY);
    uint8_t cap = tunCap;
    uint32_t hz = lcoHz;
    tuneOk = tuneAntenna(cap, hz);
    irqWired = tuneOk;
    pinMode(IRQ_PIN, INPUT);                         // plain input again (the sweep attached and detached an ISR)
    if (tuneOk) { tunCap = cap; lcoHz = hz; saveTune(); }
  }
#else
  (void)tune;
#endif
  maskWrite(0x08, 0x0F, tunCap);                     // the saved (or default) cap, also after a sweep

  rcoCalOk = calibrateRCO();                         // after tuning: the RCOs are calibrated against the LCO

  const ProfileDef &pd = PROFILES[profEff];          // the current profile decides WD, SR, MASK_DIST (MNL is in cfg.mnl)
  noiseFloor = NF_DEFAULT;
  watchdog = pd.wd;
  spikeRej = pd.sr;
  noiseFault = false;
  maskWrite(0x01, 0x7F, (noiseFloor << 4) | watchdog);
  maskWrite(0x02, 0x0F, spikeRej);
  writeReg3(0, pd.maskDist);                        // LCO_FDIV 16, MASK_DIST per profile
  applyMnl();

  senseLastAdj = nfLastAdj = millis();
  nfLastRaise = millis() - NF_RAISE_INTERVAL;        // first raise is allowed immediately
  cfgCheckMs = millis();
  return i2cErrTotal == errs;
}

// -- Setup ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  // Never block the UI on USB logging. With USB plugged into a computer that is not reading the port,
  // the HWCDC TX buffer fills and each print blocks for tx_timeout_ms (100 ms) x 20 retries = ~2 s.
  // Use 1 ms, never 0: HWCDC::write (core 3.3.7) counts `tries = tx_timeout_ms` down with `tries--` before
  // testing `tries == 0`, so 0 wraps to 2^32 and a full buffer spins ~49 days (TASK_WDT reboot). With 1 it
  // gives up after 1 ms, marks the host as not reading, and later prints are dropped until it reads again.
  Serial.setTxTimeoutMs(1);
  tft.init();                    // also drives TFT_BL HIGH and starts Wire for touch
  tft.fillScreen(C_BG);          // rotation is applied after loadPrefs() below
  // The sprite is one 115,200-byte block. With WiFi linked the largest free block is only 114,676 bytes (the heap
  // is two regions), so a full 240-row sprite no longer fits: take as many rows as fit, never fail silently.
  // The missing bottom rows (the glass is only ~40 px wide there) stay black from the fillScreen above.
  size_t heapBefore = ESP.getMaxAllocHeap();
  sprH = 240;
  while (!spr.createSprite(240, sprH) && sprH > 200) sprH--;   // 240 down to 200 inclusive
  spr.setTextFont(1);
  if (!spr.created()) LOG.printf("[BOOT] sprite FAILED (largest free block was %lu, now %lu, free %lu)\n",
                                 (unsigned long)heapBefore, (unsigned long)ESP.getMaxAllocHeap(), (unsigned long)ESP.getFreeHeap());
  else LOG.printf("[BOOT] sprite 240x%u (largest free block was %lu, now %lu, free %lu)\n", (unsigned)sprH,
             (unsigned long)heapBefore, (unsigned long)ESP.getMaxAllocHeap(), (unsigned long)ESP.getFreeHeap());
  pinMode(TOUCH_PIN, INPUT_PULLUP);
#if USE_IRQ_TUNING
  pinMode(IRQ_PIN, INPUT);       // light sleep reads it and wakes on it (the tuning sweep only runs on the first boot)
#endif
  randomSeed(esp_random());
  memset(&ts, 0, sizeof(ts));
  memset(&f2b, 0, sizeof(f2b));

  loadPrefs();
  netInit();                     // device id, network settings; starts nothing
  profileBoot();                 // profile from NVS: before the first chip write
  tft.setRotation(cfg.rot);      // saved screen rotation (touch is mapped to match in readTouchXY)
  tft.fillScreen(C_BG);
  bootDiag();
  heap_caps_register_failed_alloc_callback(heapFailCb);
  CRUMB(CS_BOOT);
  // Backlight PWM after tft.init(), which set the pin as a plain HIGH output.
  if (!ledcAttach(BL_PIN, BL_PWM_HZ, BL_PWM_BITS)) LOG.println("[SET] backlight PWM attach failed");
  applyBrightness();
  analogReadResolution(12);

  drawSplash(tft);                                   // before startWatchdog(): a plain delay is fine (it yields)
  delay(splashText[0] ? 2500 : 1200);

  drawBoot("INITIALIZING...", C_GREY);

  // Original: Wire.begin(6, 7) (raw GPIO, C3 only). D4/D5 maps correctly on C3, C6 and S3.
  Wire.begin(D4, D5);
  Wire.setClock(100000);

  // Presence: real I2C ACK check (original wrote register addresses with no data
  // and called that calibration; it was only a presence check).
  // Probe the soldered D4/D5 bus first, then the microSD-slot bus (sniffer adapter).
  sensorBus = BUS_WIRE;
  sensorOk = sensorPresent();
  if (!sensorOk) {
#if USE_IRQ_TUNING
    if (IRQ_PIN == SD_SDA_PIN) LOG.println("[AS3935] microSD-slot bus skipped: D2 carries the IRQ wire");
    else
#endif
    {
      softI2cInit();
      sensorBus = BUS_SD;
      sensorOk = sensorPresent();
    }
  }
  if (!sensorOk) {
    sensorBus = BUS_NONE;
    LOG.println("[AS3935] Not found - hold screen for demo");
    drawBoot("NOT FOUND", 0xF800);
    startWatchdog();
    waitSampling(1500);
    return;
  }
  LOG.println(sensorBus == BUS_SD ? "[AS3935] Found on microSD slot (D2/D9)" : "[AS3935] Found on D4/D5");

  irqWired = tunSaved != 0xFF;                       // a saved cap means an earlier sweep measured pulses
  initSensor(tunSaved == 0xFF);                      // sweep once if never tuned, else apply the saved cap

  // Boot status
  char msg[32];
  if (lcoHz) snprintf(msg, sizeof(msg), "ANT %.1fk %s", lcoHz / 1000.0, rcoCalOk ? "CAL OK" : "CAL FAIL");
  else       snprintf(msg, sizeof(msg), "READY %s", rcoCalOk ? "CAL OK" : "CAL FAIL");
  drawBoot(msg, rcoCalOk ? C_GREEN : C_REDORG);
  startWatchdog();
  waitSampling(1500);

  LOG.println("[AS3935] Ready - polling, auto-tune active");
}

// -- Sensor poll (only when sensorOk) -------------------------------------
// INT_NH stays asserted while noisy: rate-limit raises, but keep postponing recovery/decay.
void onNoiseHigh() {
  if (millis() - nfLastRaise >= NF_RAISE_INTERVAL) {
    bool atMax = noiseFloor >= NF_MAX;               // before the raise: 6 -> 7 is not a fault yet
    LOG.println("[NH] Noise floor too high");
    noiseEvents++;
    raiseNoiseFloor();
    nfLastRaise = millis();
    // INT_NH with the floor already at its maximum: the datasheet says the chip cannot work properly.
    if (atMax && !noiseFault) {
      noiseFault = true;
      LOG.println("[NH] noise fault - readings not trustworthy");
    }
  }
  senseLastAdj = millis();
  nfLastAdj = millis();
}

void onDisturber() {
  LOG.println(PROFILES[profEff].freeze ? "[D] Disturber (sensitivity frozen)" : "[D] Disturber - desensitizing");
  disturbers++;
  desensitize();
  senseLastAdj = millis();
  // silent: no flash, no screen update
}

// REG0x03 is read every loop pass, except in IRQ mode: wire on D2 proven by the antenna sweep, sensor on the
// D4/D5 bus. Then it is read only when IRQ is HIGH and has been for AS3935_IRQ_WAIT_US (datasheet: wait 2 ms),
// or every IRQ_FALLBACK_MS as a net for a missed edge and for the I2C health check. INT_NH holds IRQ high while
// noisy: that reads every pass, as before, and onNoiseHigh rate-limits itself.
#define IRQ_FALLBACK_MS 500UL
static bool irqModeActive() {
#if USE_IRQ_TUNING
  return irqWired && sensorBus == BUS_WIRE;
#else
  return false;
#endif
}

// Decay timers: independent of REG0x03, run on every sensor tick.
static void sensorDecay() {
  uint32_t now = millis();
  if (now - senseLastAdj > SENSE_RECOVER_INTERVAL) {
    senseLastAdj = now;
    resensitize();
  }
  if (now - nfLastAdj > NF_DECAY_INTERVAL) {
    nfLastAdj = now;
    lowerNoiseFloor();
  }
}

static void readInterrupt() {
  uint8_t intReg;
  sensorReadMs = millis();
  if (!readRegOk(0x03, intReg)) { reg3Ok = false; return; }   // failure counted; 0xFF must not look like an event
  reg3Last = intReg;                                 // the config check reads MASK_DIST from here
  reg3Ok = true;
  uint8_t reason = intReg & 0x0F;

  if (reason == 0x01) {
    onNoiseHigh();
  } else if (reason == 0x04) {
    onDisturber();
  } else if (reason == 0x08) {
    delay(5);
    uint32_t e;
    uint8_t d;
    if (!readEnergyOk(e) || !readRegOk(0x07, d)) {   // the INT read already cleared the event: it is lost
      LOG.println("[I2C] strike read failed - strike dropped");
      return;
    }
    d &= 0x3F;                                       // 0 = unknown, 1 = overhead, 0x3F = out of range
    if (sensorMuted) LOG.printf("[MUTED] sensor strike ignored: %u km, energy %lu\n", d, (unsigned long)e);
    else onRealStrike(e, d);
  }
}

void pollSensor() {
  if (irqModeActive()) {
    uint32_t nowUs = micros();
    bool high = digitalRead(IRQ_PIN) == HIGH;
    if (!high) irqHighSeen = false;
    else if (!irqHighSeen) { irqHighSeen = true; irqHighUs = nowUs; }
    bool irqReady = high && (uint32_t)(nowUs - irqHighUs) >= AS3935_IRQ_WAIT_US;
    if (irqReady || (!high && millis() - sensorReadMs >= IRQ_FALLBACK_MS)) {   // fallback never inside the 2 ms
      readInterrupt();
      irqHighSeen = false;                           // still HIGH next pass (e.g. noise): wait 2 ms again
    }
  } else {
    readInterrupt();
  }
  sensorDecay();
}

// -- Sensor link: lost detection, recovery, config read-back ---------------------
// Lost: SENSOR_LOST_FAILS failed transfers in a row (i2cNote). Then every SENSOR_RETRY_MS: presence
// check, then the full init on the bus found at boot. Recovered only if the whole init had no failure.
void sensorRecover() {
  if (!sensorPresent()) return;
  uint32_t errs = i2cErrTotal;
  initSensor(false);
  if (i2cErrTotal != errs) return;                   // still failing: stay lost, try again later
  sensorLost = false;
  i2cFails = 0;
  LOG.println("[I2C] sensor back");
}

// Every CONFIG_CHECK_MS: read back what we set. A chip that reset itself (brownout, ESD) answers with
// its defaults. Re-init only on a mismatch: a blind periodic re-init would clear the storm statistics.
void checkSensorConfig() {
  uint8_t r0, r1, r2, r8;
  if (!readRegOk(0x00, r0) || !readRegOk(0x01, r1) || !readRegOk(0x02, r2) || !readRegOk(0x08, r8)) return;
  uint8_t wantMask = PROFILES[profEff].maskDist;
  bool ok = (r0 & 0x3F) == (afeValue() & 0x3E)
         && (r1 & 0x7F) == (uint8_t)((noiseFloor << 4) | (watchdog & 0x0F))
         && (r2 & 0x3F) == (uint8_t)(((cfg.mnl & 3) << 4) | (spikeRej & 0x0F))
         && (r8 & 0x0F) == tunCap
         && (!reg3Ok || ((reg3Last >> 5) & 1) == wantMask);   // REG0x03 comes from the last poll: a second read would eat an interrupt
  if (ok) return;
  LOG.printf("[AS3935] config lost - re-init (REG0=%02X REG1=%02X REG2=%02X REG8=%02X)\n", r0, r1, r2, r8);
  initSensor(false);
}

void sensorTick(uint32_t now) {
  if (!sensorOk) return;
  if (sensorLost) {
    if (now - sensorRetryMs >= SENSOR_RETRY_MS) { sensorRetryMs = now; sensorRecover(); }
    return;
  }
  pollSensor();
  if (!sensorLost && now - cfgCheckMs >= CONFIG_CHECK_MS) { cfgCheckMs = now; checkSensorConfig(); }
}

// -- Reset filters ---------------------------------------------------------------
// Noise floor, WD and SR back to their minimums, chip statistics cleared (REG0x02 bit 6 CL_STAT
// high-low-high, per the datasheet). MNL stays. The 30-30 countdown and the alert stage are NOT touched:
// no filter button may pretend it is safe. The 3 s hold on Records (resetRecords) is the one deliberate way
// to clear them.
void resetFilters() {
  noiseFloor = NF_DEFAULT;
  watchdog = PROFILES[profEff].wd;
  spikeRej = PROFILES[profEff].sr;
  noiseFault = false;
  if (sensorOk) {
    applyWatchdogSpike();
    maskWrite(0x02, 0x40, 0x40);
    maskWrite(0x02, 0x40, 0x00);
    maskWrite(0x02, 0x40, 0x40);
  }
  senseLastAdj = nfLastAdj = millis();
  nfLastRaise = millis() - NF_RAISE_INTERVAL;
  filtersMsgUntil = millis() + 1500;
  LOG.println("[SET] filters reset");
}

// -- Two-stage alert ---------------------------------------------------------------
const char* alertName(AlertStage a) {
  return a == ALERT_SHELTER ? "SHELTER" : a == ALERT_ACTIVITY ? "ACTIVITY" : "IDLE";
}

// Strikes in the last 15 min: any kind (anyN) and without the out-of-range ones (nearN).
// Unknown-distance strikes count in both.
void alertCounts(const Stats &S, uint32_t now, int &anyN, int &nearN) {
  anyN = 0; nearN = 0;
  for (int i = 0; i < S.histCount; i++) {
    int idx = (S.histHead - S.histCount + i + HIST_MAX) % HIST_MAX;
    if (now - S.hist[idx].t > ALERT_WINDOW_MS) continue;
    anyN++;
    if (S.hist[idx].d != DIST_OOR) nearN++;
  }
}

// SHELTER: 30-30 countdown running, or 5+ strikes in 15 min (out-of-range excluded).
// ACTIVITY: otherwise any strike in 15 min. Only time clears it: there is no dismiss.
AlertStage alertCompute(const Stats &S, uint32_t now, char* why, size_t whyN) {
  int anyN, nearN;
  alertCounts(S, now, anyN, nearN);
  AlertStage next = ALERT_IDLE;
  if (S.clearRunning) {
    next = ALERT_SHELTER; snprintf(why, whyN, "30-30 countdown");
  } else if (nearN >= ALERT_SHELTER_N) {
    next = ALERT_SHELTER; snprintf(why, whyN, "%d strikes in 15 min", nearN);
  } else if (anyN >= 1) {
    next = ALERT_ACTIVITY; snprintf(why, whyN, "%d strike%s in 15 min", anyN, anyN == 1 ? "" : "s");
  } else {
    snprintf(why, whyN, "no strikes in 15 min");
  }
  return next;
}

void alertUpdate(uint32_t now) {
  char why[40];
  AlertStage next = alertCompute(*st, now, why, sizeof(why));
  if (next == alertStage) return;
  LOG.printf("[ALERT] %s -> %s (%s%s)\n", alertName(alertStage), alertName(next), why, demoActive ? ", demo" : "");
  bool toShelter = next == ALERT_SHELTER;
  alertStage = next;
  uiDirty = true;
  if (toShelter) screenWake("shelter");
}

// -- Loop ----------------------------------------------------------------
void loop() {
  loopStartUs = micros();
  esp_task_wdt_reset();
  CRUMB(CS_CONSOLE);
  consolePoll();
  crumb.pass++;
  crumb.upS = (uint32_t)(uptimeMs() / 1000ULL);   // also keeps the 64-bit uptime counter across millis wraps
  heapWatch();
  CRUMB(CS_TOUCH);
  touchPoll();
  handleTouch();

  if (demoActive) { CRUMB(CS_DEMO); demoTick(); }
  CRUMB(CS_SENSOR);
  sensorTick(millis());

  CRUMB(CS_STATS);
  uint32_t now = millis();
  binsAdvance(realStats, now);
  ageCap(realStats, now);
  clearTick(realStats, now, false);
  if (demoActive) {
    binsAdvance(demoStats, now);
    ageCap(demoStats, now);
    clearTick(demoStats, now, true);
  }
  CRUMB(CS_ALERT);
  alertUpdate(now);
  CRUMB(CS_SCREEN);
  screenTick(now);
  CRUMB(CS_UITICK);
  f2bTick(now);
  navTick(now);
  CRUMB(CS_BATT);
  battTick(now);
  CRUMB(CS_PERSIST);
  persistTick(now);
  CRUMB(CS_NET);
  netTick(now);
  heapWatch();
  CRUMB(CS_PROFILE);
  profileTick(now);

  CRUMB(CS_DRAW);
  drawUI();
  heapWatch();
  CRUMB(CS_HEALTH);
  healthTick(millis(), micros() - loopStartUs);
  restOrSleep();                 // 50 ms of touch sampling, or one light sleep (power.ino)
}

# Firmware reference

How the firmware works, what changed from gokux's original sketch, and every console command. For building and flashing see [building.md](building.md); for day-to-day use see [using.md](using.md).

## Layout

- `FlashBee/FlashBee.ino`: main sketch (globals, AS3935 driver, strike recording, setup, loop), transcribed from the instructable
- `FlashBee/FlashBee.h`: all constants, structs and enums (kept in a header so Arduino's auto-prototypes always see the types)
- `FlashBee/input.ino`: touch state machine (tap, swipe, long press, hold-to-reset, wake-only touch) and page navigation
- `FlashBee/ui_main.ino`: drawing helpers, page dispatcher, Main page
- `FlashBee/ui_pages.ino`: Storm chart, Flash-to-bang, Records, Settings, Nerd stats pages
- `FlashBee/storage.ino`: settings and records in NVS, brightness, screen power (off/wake)
- `FlashBee/profiles.ino`: operating profiles (STATION, PORTABLE, STORMCHASE) and the AUTO selector
- `FlashBee/net.ino`: WiFi, OTA, MQTT, Home Assistant discovery, remote log and remote commands, `LOG` (the `Print` that every log line goes through)
- `tools/provision.py`: sets WiFi, OTA and MQTT over the USB console
- `FlashBee/battery.ino`: battery reading on D0
- `FlashBee/driver.h`: Seeed_GFX config (`BOARD_SCREEN_COMBO 501`, Round Display)

Arduino appends the extra `.ino` tabs to the main file in alphabetical order, so every global lives in `FlashBee.ino` and every type in `FlashBee.h`.

The sketch folder name has no space because Arduino rejects sketch names with spaces.

The PDF guides and the commands that build them are in [building.md](building.md#documentation).

## Changes from the original

- Serial logging never blocks: `Serial.setTxTimeoutMs(1)`. With USB plugged into a computer that does not read the port, each print used to stall the loop about 2 s and freeze the screen. Not 0: in core 3.3.7 `HWCDC::write` decrements the try count before testing it, so 0 wraps and a full buffer spins until the task watchdog reboots the device.
- Task watchdog (8 s, `WDT_TIMEOUT_MS`) reboots the device if the loop ever hangs. Each boot logs `[BOOT] reset reason` and counts boots, crashes and the last crash reason in NVS (shown on the Nerd stats page; not cleared by a records reset).
- Crash breadcrumb: a 16-byte `Crumb` in RTC memory (`RTC_NOINIT_ATTR`, so it survives a watchdog or panic reset but not power loss) holds the loop stage (console, touch, demo, sensor, stats, alert, screen, uitick, battery, persist, net, ota, profile, draw, push, health, rest, sleep, shot, boot), the page, the uptime in seconds, the last event (strike, touch, page, scr-on, scr-off, wake, nvs, cmd, ota), the loop pass count and whether the net task was inside a network call. The loop only does plain stores. After a crash reset (panic, task or interrupt watchdog) the boot logs `[BOOT] last crumb: stage=... page=... up=...s evt=... pass=... net=...` and saves it to NVS (key `crumb`). `status` prints it as `[STATUS] crumb ...`; `diag reset` clears it. Test: `hang` should give `stage=console`.
- Build stamp: `flash.sh` runs `stamp.sh`, which writes the git-ignored `FlashBee/build_stamp.h` (`BUILD_GIT` = short commit, `-dirty` when tracked files differ; `BUILD_DATE` = UTC time) just before the compile. The board logs `[BOOT] build <sha> <date>`, `status` prints `[STATUS] build <sha> <date>`, and every Home Assistant discovery document carries the sha as the device `sw_version`. A build without the file (plain Arduino IDE) reports `nogit` and the compiler's local date.
- `Wire.begin(6, 7)` became `Wire.begin(D4, D5)`. Same pins on the C3, also correct on the C6 (GPIO22/23) and S3 (GPIO5/6).
- AFE gain uses datasheet values: indoor `0x24`, outdoor `0x1C`. The original `0x12` is AFE_GB 01001, not a datasheet value. Every operating profile uses outdoor (see Operating profiles); `set gain in|out` on the console is a live override for this run. The write is masked (bits 5:1), so the PWD bit stays 0.
- Boot now runs the datasheet sequence: I2C ACK check, PRESET_DEFAULT (0x96 to 0x3C), AFE, antenna tuning, CALIB_RCO (0x96 to 0x3D plus DISP_SRCO pulse), then checks TRCO/SRCO done bits. The boot screen shows `CAL OK` or `CAL FAIL`.
- REG0x02 is written with a mask, so reserved, CL_STAT and MIN_NUM_LIGH bits are kept.
- Noise floor drops one step after 60 s with no noise alarm (floor 1). The original only raised it.
- `increaseSensitivity`/`decreaseSensitivity` renamed `desensitize`/`resensitize`. The original names were backwards. Behavior is the same.
- The screen's tune line shows the noise floor and the operating profile: `NF2 WD2 SR2 PORT` (`STAT`, `PORT`, or `CHAS` in red-orange for STORMCHASE; orange after a live override). The colons were dropped so it fits inside the gauge ring.
- Noise-floor raises are limited to one per 2 s (`NF_RAISE_INTERVAL`). INT_NH stays asserted while the noise is above the floor, so polling used to push NF to 7 in about half a second and leave the device deaf for minutes after a short burst (phone, charger). Noise still postpones the decay and sensitivity recovery timers.
- Sensitivity recovery no longer goes below the datasheet defaults (`WD_MIN` and `SR_MIN`, both 2). It lowers the higher of WDTH/SREJ, or the other one if that is at its minimum, and does nothing (no write, no log line) when both are at minimum. The "Quiet - restoring sensitivity" line only prints when something changes.
- The watchdog/spike write to REG0x01 is masked (`0x7F`), so reserved bit 7 is kept.
- Gauge fix: `drawArc` uses 0 deg = 6 o'clock, clockwise, but the original drew 225 to 135 (gap at the top, fill starting at 1:30) while the ticks used cos/sin angles (0 deg = 3 o'clock) and ran 10:30 to 7:30. Both now sweep 270 deg clockwise from bottom-left (7:30) over the top to bottom-right (4:30), gap at the bottom. Arc: 45 to 315. Ticks: 135 + 27 x i.
- The compile-time `DEMO_MODE` is gone. Demo is a runtime state toggled by a 3 s touch (see Demo mode), shown in magenta, with its own strike state. A missing sensor no longer halts boot.
- The main screen shows the time since the last strike ("12s ago", "4m ago", "1h 05m ago") in grey above the distance. After 15 minutes (`STALE_MS`) the distance turns dim, because the AS3935 purges its distance statistics then.
- Storm trend: the last 256 strikes are kept (`HIST_MAX`; out-of-range ones too, for the chart, but the trend and the count skip them). The screen shows the in-range strike count in the last 15 minutes ("N in 15m", with "+" if the buffer is full and its oldest entry is still inside the 15 minutes). The trend fits a straight line through the in-range distances of the last 10 minutes (`TREND_WINDOW_MS`; at least 3 strikes spanning at least 2 minutes). It shows APPROACHING if the line drops 3 km or more across those strikes, MOVING AWAY if it rises 3 km or more, otherwise STEADY, under the divider. A best-fit line ignores a single odd reading, so a storm that sits still does not flip between APPROACHING and MOVING AWAY. The serial strike line prints the count and trend too.
- Screen distance is in miles or km, chosen on the Settings page and saved (`USE_MILES` is only the first-boot default). km codes from the AS3935 are converted for display only (km x 0.621371, rounded), with "< 1 mi" for overhead and ">25 mi" for out of range. The trend threshold and serial output stay in km (serial also appends the miles in miles mode).
- Pages: swipe left or right (at least 50 px, mostly sideways, under 1 s) to move between six pages: Main, Storm chart, Flash-to-bang, Records, Settings, Nerd stats. Dots at the bottom show which page is up. Serial: `[PAGE]`, `[SWIPE]`, `[TAP]`.
- One touch state machine: touch INT on D7 plus CHSC6X coordinates over I2C (0x2E), read only while INT is LOW. Tap = under 400 ms and under 15 px of travel. Long press = 3 s with the finger within 30 px of where it started. A long press never also counts as a tap or a swipe.
- Storm chart page: distance against time for the last 60 minutes (now at the right, near at the bottom), one dot per in-range strike coloured by energy, a dashed line at 10 mi / 16 km, and a strikes-per-5-minutes bar strip (exact counts, kept apart from the strike buffer).
- Flash-to-bang page: tap at the flash, tap at the thunder; shows seconds and distance (5 s per mile, 3 s per km), the sensor's distance if it saw a strike in the last 60 s, and the last 3 results. Times out after 60 s ("too far to hear"). Serial: `[F2B]`.
- Records page, saved in NVS: closest strike, strongest strike, biggest storm, all strikes, storms (a storm = strikes less than 30 min apart). Real strikes only; strikes with unknown distance count in all but "closest". NVS is written at most once per 10 s, only changed keys. Serial: `[REC]`. A RESET button at the bottom clears the records on a 3 s hold (progress bar, then "RECORDS + ALERT RESET"); a tap does nothing. The hold also clears the live strike history, the 30-30 countdown and the ALL CLEAR banner, so the alert drops to IDLE (`[ALERT] ... -> IDLE`). Settings, profile and filters stay. `resetrec` does the same.
- Settings page: PROFILE (a tap cycles AUTO, STATION, PORTABLE, STORMCHASE; AUTO shows the resolved profile under it), units mi/km, brightness (3 levels, backlight PWM on D6), and SCREEN OFF AFTER (a tap cycles 30 s, 1, 2, 5, 10 min, NEVER). Saved in NVS (STORMCHASE is not). Serial: `[SET]`. Reset records moved to the Records page (see below).
- Nerd stats page: NF/WD/SR, sensor state (OK, LOST, NOT FOUND, or a red ENV TOO NOISY), profile (`AUTO>STATION`), gain and MNL (`*` = live override), alert stage, disturbers / noise-high events since boot, TRCO/SRCO calibration and antenna, uptime, battery mV and %, strikes since boot and in this storm, boots / crashes, WiFi (IP and RSSI, `connecting`, `retry`, or `off`) and MQTT (`connected`, `connecting`, `off`). A RESET FILTERS button (a tap) runs `resetfilters`.
- 30-minute all-clear (NWS 30-30 rule): a real strike at 10 mi / 16 km or closer starts a 30:00 countdown, and every further close strike restarts it. Main shows "ALL CLEAR IN mm:ss" in place of "ENERGY HISTORY", then "ALL CLEAR" in green for 60 s. In the demo it runs on demo strikes, in magenta. Serial: `[CLEAR]`.
- Battery % on Main (small battery icon under the trend line), red-orange under 15 %, "--" with no reading. Read from D0 every 10 s (16 samples, smoothed). Serial: `[BATT]` every 60 s.
- Navigation: a real strike jumps to Main, and any other page returns to Main after 2 minutes without a touch. Neither happens while flash-to-bang is timing.
- The AS3935 can also be wired to the Round Display's microSD slot (D2 = SDA, D9 = SCL) with a sniffer adapter. Boot probes D4/D5 first, then the slot, using a small software I2C master. See [building.md](building.md#solderless-wiring-microsd-slot).
- Screen rotation `rot` (0..3, `tft.setRotation`) is saved in NVS and set with the console `set rot <0-3>`; default 0 is the original orientation. Touch is mapped to the rotated screen in `readTouchXY`. For an enclosure that mounts the Round Display turned 90 degrees. Not tested on hardware yet.
- Without an AS3935, only the Main page shows NOT FOUND. The other pages (flash-to-bang, records, settings) still work.

## Sensor safety and alerts

- **Sensor lost.** Every AS3935 read and write reports success. After 8 failures in a row (`SENSOR_LOST_FAILS`) the log says `[I2C] sensor lost` and Main shows a red SENSOR LOST screen (the other pages keep working; the alert footer stays if a SHELTER was running). While lost, every 3 s (`SENSOR_RETRY_MS`) the firmware checks the chip answers on the bus found at boot and runs the full `initSensor()`; when that finishes without a failed transfer it logs `[I2C] sensor back`. `initSensor()` is the boot sequence: PRESET_DEFAULT, gain, TUN_CAP (swept once and saved in NVS, or the saved cap), RCO calibration, NF/WD/SR defaults, MNL. A read-modify-write (`maskWrite`) writes nothing if its read failed.
- **Config check.** Every 60 s (`CONFIG_CHECK_MS`) it reads back REG0x00 (gain), REG0x01 (NF/WD), REG0x02 (SREJ, MNL) and REG0x08 (TUN_CAP) and compares them with what it set. A mismatch (the chip reset itself after a brownout or ESD) logs `[AS3935] config lost - re-init` and runs `initSensor()`. There is no periodic blind re-init: it would clear the chip's storm statistics.
- **ENV TOO NOISY.** INT_NH while the noise floor is already 7 sets `noiseFault` (`[NH] noise fault - readings not trustworthy`); the datasheet says the chip cannot work properly then. Main's `NF WD SR` line turns into a red ENV TOO NOISY, the Nerd page shows it. It clears when the floor decays below 7 (the 60 s quiet decay) or on reset filters.
- **Distance code 0 = unknown.** The datasheet defines only 1 (overhead) and 0x3F (out of range). A strike with code 0 (`DIST_UNKNOWN`) counts everywhere strikes count (count, energy, ticker, chart bars, records total, storm grouping, the 15-min alert counts) but is left out of everything about distance: the trend, the 30-30 countdown, the closest-strike record, the "in 15m" in-range count and the chart dots. Main shows `--` and "distance unknown".
- **MIN_NUM_LIGHTNING.** `cfg.mnl` 0..3 (REG0x02 bits 5:4): the chip raises a lightning interrupt only after 1, 5, 9 or 16 strikes in 15 min. The operating profile decides it (STATION 1 = 5 strikes, the others 0). Console `set mnl 0-3` is a live override for this run, not saved. Shown on Nerd and in `status`. Only bits 5:4 are written.
- **Two-stage alert** (`AlertStage`, computed every loop from the shown stats, real or demo). SHELTER: the 30-30 countdown is running (strike at 16 km or closer in the last 30 min) or 5+ strikes in the last 15 min, out-of-range ones not counted (unknown-distance ones count). ACTIVITY: otherwise at least one strike of any kind in the last 15 min. IDLE: otherwise. Every change logs `[ALERT] IDLE -> ACTIVITY (reason)`. On Main, SHELTER is a large banner flipping red and amber at 1 Hz (in place of the energy ticker) with a blinking border, and the 30-30 countdown (or "N STRIKES IN 15 MIN") under it; ACTIVITY is a quiet amber label in the same slot as the countdown. In demo both are magenta. There is no dismiss: only time clears SHELTER.
- **Reset filters** (Nerd page button or `resetfilters`): NF 2, WD and SR to their minimums, MNL unchanged, the chip's lightning statistics cleared (REG0x02 CL_STAT high, low, high), `noiseFault` cleared, adapt timers restarted. It does not touch the 30-30 countdown or the alert stage (only the Records reset does). Logs `[SET] filters reset`.
- **Screen off.** After the `cfg.scr` timeout (default 5 min) with no touch and no strike, the backlight goes to duty 0 and the panel gets DISPOFF then SLPIN. It wakes (SLPOUT, 120 ms, DISPON, backlight) on a touch (that touch does nothing else), a strike (before its flash), the stage rising to SHELTER and any console command that draws. It never turns off while the stage is SHELTER, a finger is down, or flash-to-bang is timing. Disturbers and noise do not wake it. Drawing is skipped while off, and the C3 light-sleeps (see "Light sleep"). Serial: `[SCREEN] off|on (reason)`.

## Operating profiles

The indoor/outdoor gain choice is gone: every profile uses OUTDOOR gain (indoors, in a noisy modern home, INDOOR saturates) and starts the noise floor at 2. The profile sets the rest (`PROFILES` in `FlashBee.ino`):

| profile | WD | SR | MNL (REG0x02 bits 5:4) | MASK_DIST (REG0x03 bit 5) | WD/SR auto-adjust |
|---|---|---|---|---|---|
| STATION | 2 | 2 | 1 (chip reports after 5 strikes in 15 min) | 1 (disturbers masked) | on |
| PORTABLE | 2 | 2 | 0 (every strike) | 1 | on |
| STORMCHASE | 1 | 0 | 0 | 0 (disturbers reported) | frozen |

- **Modes:** `AUTO | STATION | PORTABLE | STORMCHASE`, NVS key `prof`, default AUTO. AUTO is STATION while WiFi is associated and PORTABLE otherwise, never STORMCHASE. With an SSID set, boot starts as STATION (pending) and falls to PORTABLE if WiFi is not associated within 60 s; after that it switches only when the association state has been stable for 30 s, so a short WiFi dropout does not flip it. Every change logs `[PROFILE] applied ...`.
- **STORMCHASE is never saved.** Setting it applies it for this run; a reboot returns to the saved mode. It is attended use only: unmasked disturbers and the most sensitive WD/SR give false strikes when left on.
- Applying a profile re-writes REG0x00 (gain), REG0x01 (NF, WD), REG0x02 (SR, MNL) and REG0x03 (MASK_DIST). `initSensor()` (boot, sensor-lost recovery, config read-back re-init) applies the current profile, and the 60 s config read-back compares against it (REG0x03 comes from the last interrupt poll, because reading it clears the interrupt).
- With STATION and PORTABLE the disturbers are masked: the chip no longer reports them, so the `dist/noise` counter on the Nerd page stays at 0 and WD/SR move only through the console `dist` command. The noise-floor raise for INT_NH works in every profile.
- `set gain in|out` and `set mnl 0-3` are live overrides (not saved). The next `profile ...` command, or an AUTO switch, puts the profile's values back. A sensor re-init keeps the override.
- The two-stage alert (ACTIVITY / SHELTER) is the same in every profile.

## Network

Optional and off by default: with no WiFi SSID set (or `wifi off`) the radio is never started and nothing below costs power. Device id = the last 3 bytes of the STA MAC in lowercase hex (e.g. `a1b2c3`); hostname `flashbee-<id>` with mDNS. The board prints its id in the `[NET]` boot line and in `wifi`.

**Setup.** `uv run --with pyserial tools/provision.py` (or `python3 tools/provision.py [port]` if pyserial is installed) asks for the WiFi SSID and password, the OTA password, the MQTT host, user and password (Enter skips a field; passwords are not echoed), sends the matching console lines and prints the replies. It opens the port without resetting the board and writes nothing to disk. The same can be typed by hand: `wifi ssid <name>`, `wifi pass <password>`, `ota pass <password>`, `mqtt host <ip or name>`, `mqtt user <user>`, `mqtt pass <password>`. Values may contain spaces. Settings live in NVS (`wSsid wPass otaPass mHost mPort mUser mPass mPfx haDisc net`) and apply at once. `wifi forget` clears the SSID and password, `mqtt forget` the host, user and password, `ota forget` the OTA password. A factory reset does not touch them.

**Behaviour.** WiFi association is attempted for 20 s, then backs off 5 s doubling to 60 s. MQTT connects with a 1.5 s TCP timeout and a 2 s CONNACK wait and the same backoff; a host name is resolved in a small task so a dead DNS server cannot stall the screen. Nothing blocks the loop for more than about 2 s.

**OTA.** ArduinoOTA, running only while WiFi is up and an OTA password is set. During an update the screen shows UPDATING with a progress bar and the log shows `[OTA] start|end|error`. Upload with `arduino-cli upload --port flashbee-<id>.local --fqbn esp32:esp32:XIAO_ESP32C3 FlashBee --upload-field password=<ota password>` (or the espota script: `python3 espota.py -i flashbee-<id>.local -p 3232 -a <ota password> -f FlashBee.ino.bin`). The default XIAO_ESP32C3 partition scheme has two 1.25 MB app slots; this build is about 1.19 MB (C3) and 1.22 MB (C6), so it fits, with about 118 KB left on the C3. The first network build still has to go in over USB.

**MQTT topics** (prefix `<pfx>`, default `flashbee/<id>`; change with `mqtt prefix <value>`):

| topic | direction | content |
|---|---|---|
| `<pfx>/availability` | publish, retained | `online`; the last will is `offline` |
| `<pfx>/state` | publish, retained | JSON, on connect, every 60 s and at once when the alert stage, profile, sensor status or noise fault changes: `strikes` (since boot), `dist_km`, `dist` (`overhead|km|out_of_range|unknown|none`), `energy`, `last_strike_s`, `alert` (`idle|activity|shelter`), `clear_left_s`, `noise_fault`, `sensor` (`ok|lost|not_found`), `nf`, `wd`, `sr`, `mnl`, `profile`, `profile_mode`, `battery_pct`, `battery_mv`, `rssi`, `uptime_s`. Null where there is no value. |
| `<pfx>/strike` | publish | one JSON per real strike: `dist_km`, `dist`, `energy`, `uptime_s` |
| `<pfx>/cmd` | subscribe | one console line per message (for example `profile station`) |
| `<pfx>/log` | publish | every log line (the same text as the USB serial); a 16-line ring, the oldest line is dropped when full |

State and strike always come from the real strike data: the demo never reaches MQTT. Over `<pfx>/cmd` these are refused with `[CON] not allowed over MQTT`: the `wifi`, `ota` and `mqtt` setters, `factory`, `hang`, `shot`, `tune`, `fault`, `strike`, `burst` (and `mqttcmd`). Everything else runs, including `profile`, `set ...`, `rst`. On every connect the device publishes an empty retained message to `<pfx>/cmd` (clears a retained command, so it cannot replay at each reconnect); empty payloads are ignored. Do not publish commands with the retain flag.

**Home Assistant.** With `ha on` (default) every MQTT connect publishes retained discovery documents under `homeassistant/`, one device `Flash Bee <id>` (manufacturer DIY): sensors Strikes, Distance (km, `distance`), Last strike energy, Last strike (s), Alert (enum idle/activity/shelter), 30-30 countdown (s), Battery, WiFi signal (diagnostic), Noise floor (diagnostic); binary sensors Noise fault and Sensor (both `problem`; the sensor one is on when lost or not found); a select Profile (auto, station, portable, stormchase; it sends `profile <value>` to `<pfx>/cmd`). `unique_id` is `flashbee_<id>_<name>`; availability follows `<pfx>/availability`. `ha off` removes the entities again. `mqtt dump` prints every document and checks that it fits the 1024-byte MQTT buffer.

**Security.** The WiFi, OTA and MQTT passwords are stored in plain text in the device's NVS (anyone with USB access can read the flash) and are never printed, logged, published or drawn: the console shows `set` / `not set`. The MQTT link is plain TCP without TLS, so use it on a trusted network only; give the broker a user and password. OTA is protected by its password (challenge-response, but the image travels unencrypted). A message on `<pfx>/cmd` can change settings (`rst`, `profile`, `set ...`), so protect the broker with ACLs.

**Memory.** WiFi links about 31 KB of static RAM. The 240 x 240 screen buffer needs one 115,200-byte block, and the largest free block is 114,676 bytes, so the sketch takes 238 rows (`[BOOT] sprite 240x238`). The two bottom rows stay black (the glass is only about 40 px wide there). If more RAM is ever linked in, the row count drops further at boot instead of the screen going blank.

## Antenna tuning

The AS3935 IRQ pad is soldered straight to the XIAO's D2 pin (GPIO4 on the C3, not a strapping pin; the same line as the microSD slot's card pin 2). IRQ idles low (push-pull). The battery reading on D0 is not affected. `USE_IRQ_TUNING` is 1 and `IRQ_PIN` is `D2`.

- On the first boot (nothing saved in NVS) the firmware sweeps TUN_CAP 0 to 15, measures each frequency (LCO/128 on IRQ), keeps the setting closest to 500 kHz and saves `tunCap` and `lcoHz` in NVS. Later boots apply the saved cap without sweeping. Target is within 3.5% of 500 kHz.
- The console command `tune` runs the sweep again, saves the result and re-runs the sensor init, so the RCOs are calibrated after the new cap. It blocks sensing for about 4 s and is refused over MQTT.
- If no cap gives a single pulse (IRQ not connected), nothing is saved, the old cap stays and the log says `[ANT] no pulses on IRQ`.
- This unit measured 515.5 kHz at TUN_CAP 0 (+3.1%, near the datasheet's 3.5% limit), so the often-quoted "Seeed ships these near 496 kHz" is not true for every board. The sweep picked TUN_CAP 10: 499.2 kHz (-0.2%).
- Because D2 carries IRQ, the microSD-slot I2C fallback (D2/D9) is skipped when the D4/D5 probe fails (`[AS3935] microSD-slot bus skipped`).

## Light sleep

While the screen is off, the C3 goes into light sleep between loop passes (`power.ino`). It wakes on a strike or any other AS3935 interrupt (IRQ on D2 goes high), on a touch (INT on D7 goes low), and after at most 1 s (`SLEEP_MAX_MS`), so the sensor poll, the noise-floor decay, the config check, the battery reading, deferred NVS writes and the countdowns still run about once a second. A touch wakes the screen (that touch does nothing else, as before), also a tap that is over before the firmware reads the pin. An IRQ wake reads the interrupt register 2 ms later (datasheet wait), so a strike is shown at once.

It sleeps only when all of these hold (the first one that fails is shown as `now=blocked(<reason>)`):

- `sleep on` (default), `USE_IRQ_TUNING` 1, the sensor found on D4/D5 and not lost;
- the screen is off;
- WiFi is off: no SSID, or `wifi off` (no radio, no OTA);
- no computer on the USB port for 2 s (`usb host`). A charger or power bank is fine: only a computer sends the USB frames this checks. `sleep usb on` is a test override;
- no demo, no `burst`, no finger down, flash-to-bang not timing, not SHELTER;
- IRQ is low (an interrupt waiting to be read is read first).

Light sleep switches the USB port of the C3 off: a computer sees the device unplug on every sleep. So the firmware stays awake for 0.5 s every 30 s (`usb listen`); a computer plugged in meanwhile finds the port within 30 s and then keeps the device awake. Tap the screen to make the port appear at once (for example before `flash.sh`). With `sleep usb on` and a Mac attached, the port stayed listed while the board slept (tested): console lines still got through within about 5 s, but most log output was lost. Read the results with `sleep` after `sleep usb off`.

In sleep every pin is isolated (IDF ESD workaround) except the two wake pins, the display pins (CS stays high, the backlight stays driven low) and the I2C pins, which keep their levels.

Console: `sleep` (same as the `[STATUS] sleep` line: switch, override, `now=allowed` or the blocking reason, sleeps since boot, time asleep and % of uptime, wakes by timer / IRQ / touch / other), `sleep on|off`, `sleep usb on|off` (both RAM only, refused over MQTT). While it sleeps, a `[SLEEP]` line at most once a minute gives the sleeps, % asleep and IRQ / touch wakes in that window. To read the counters after a battery run, plug into the computer and open the port without resetting the board (set `rts = False` first and `dtr = False` second after `open()`, as `tools/provision.py` does), then send `sleep`.

Not measured yet: the battery current in light sleep. Expect a floor of about 0.7 mA from the display's 4.7k pull-up on D2 (the microSD card pin 2 line) against the IRQ output, which idles low.

## Demo mode

Demo is a runtime state, toggled from the touchscreen. Every boot starts in real mode (nothing is saved).

- Hold a finger still on the screen for 3 seconds (`LONG_PRESS_MS`), on any page. After 0.5 s a "HOLD: DEMO ON" (or "DEMO OFF") box with a filling bar appears. One long press toggles once; lift the finger before the next one. The finger must stay within 30 px (`HOLD_SLOP_PX`) of where it started, so a slow swipe never starts the demo. The one exception is a hold that starts on the Settings page's RESET button: that one resets the records instead.
- Touch "down" comes from the Round Display's CHSC6X INT line on D7 (LOW while touched). INT is treated as "down" if it was LOW in the last 150 ms, because some controllers pulse it while held. While it is LOW, coordinates are read from the CHSC6X at 0x2E (at most every 15 ms). If no coordinate can be read, the long press still works. Serial prints `[TOUCH] down` and `[TOUCH] up ...` for bench checks.
- In demo the label reads `DEMO - NOT REAL` and the border ring is magenta (2 px). The demo fakes a strike every 4 to 10 seconds with a random energy. Distance is a random walk (1 to 4 km steps, snapped to valid AS3935 codes) that drifts in from 30 to 40 km to 1 km, then back out, so APPROACHING and MOVING AWAY both show. About 1 strike in 10 is out of range. Serial lines carry the `[DEMO]` prefix.
- Demo and real data are separate (`demoStats`, `realStats`). Starting a demo begins with an empty screen. Leaving it brings back the real counts, ticker and trend untouched.
- The sensor keeps being polled during the demo. A real strike ends the demo at once and is shown as real. Noise and disturber handling continue as normal.
- The demo ends by itself after 10 minutes (`DEMO_TIMEOUT_MS`). Serial prints `[DEMO] on` and `[DEMO] off (reason: touch|timeout|real strike)`.
- If the AS3935 is not found, the Main page shows `NOT FOUND` and "hold screen for demo". The sensor is never accessed (`sensorOk` false), but a long press still starts the demo. When it ends, the NOT FOUND screen returns.
- Demo strikes never touch the records, the real all-clear countdown or the real chart, and do not jump back to Main. Every page has the magenta ring during the demo.

## Not changed

- With the IRQ wire proven by the antenna sweep (sensor on D4/D5), the code reads the interrupt register only when IRQ (D2) has been high for 2 ms, plus a fallback read every 500 ms for a missed edge and the I2C health check. Without the wire (or on the microSD-slot bus) it polls every loop pass (about 70 ms). In light sleep the IRQ wire wakes it (see "Light sleep").
- Main redraws every pass (the rings animate). The other pages redraw at most every 250 ms, at once on a touch, strike or alert change, and every pass while a finger is down.

## Test console

Developer feature: line commands over USB serial (115200, end with newline; every command answers
`[CON] ok <cmd>` or `[CON] err <reason>`). Events use the real handlers (`onRealStrike`, `onDisturber`,
`onNoiseHigh`, the touch gesture dispatcher, the Settings setters). A `[HEALTH]` line prints every 60 s.

- `help`, `status`, `records`
- `strike <km> <energy>`: km in 0 (unknown) 1 5 6 8 10 12 14 17 20 24 27 31 34 37 40 63; energy 0..2097151
- `burst <n> <interval_ms>` (interval >= 50), `burst stop`; prints `[CON] burst done`
- `dist`, `noise`: disturber / noise-high event (sensor registers adjust as in real use)
- `mute on|off`: ignore lightning events from the sensor (logged as `[MUTED]`). Desk electronics cause an occasional false strike, which restarts the 30-min all-clear, so long timing tests run muted. RAM only: a reboot clears it.
- `page <0-5>`, `next`, `prev`, `tap <x> <y>`, `swipe l|r`, `hold <x> <y>`, `demo on|off`
- `set gain in|out`, `set mnl 0-3`: live overrides for this run, not saved (`override until the next profile change`; any `profile ...` command puts the profile's values back)
- `set units mi|km`, `set bright 0|1|2`, `set rot 0|1|2|3` (screen rotation, saved; `status` shows `rot=`)
- `set scr 0-5` (screen off after 30 s, 1, 2, 5, 10 min, never), `resetfilters`
- `sleep`, `sleep on|off`, `sleep usb on|off`: light-sleep status and counters, runtime switch, test override to sleep with a USB host (see "Light sleep"; not saved, only `sleep` over MQTT)
- `tune`: antenna sweep on the IRQ wire, saves the best TUN_CAP and LCO frequency in NVS (about 4 s; error if `USE_IRQ_TUNING` is 0 or no pulses are seen)
- `profile` (mode, effective profile, knobs and the chip's REG0x00/01/02, REG0x03 from the last poll), `profile auto|station|portable|stormchase`
- `wifi`, `wifi ssid <name>`, `wifi pass <password>`, `wifi on|off`, `wifi forget` (values take the rest of the line, spaces included; changes apply at once)
- `ota`, `ota pass <password>`, `ota forget`; `mqtt`, `mqtt host|port|user|pass|prefix <value>` (`prefix reset` = default), `mqtt forget`, `mqtt dump` (prints the state, discovery and strike JSON with their sizes); `ha`, `ha on|off`
- `mqttcmd <line>`: test hook, runs a console line through the same queue and guard as a message on `<pfx>/cmd`
- `fault i2c on|off`: every sensor read, write and presence check fails (RAM only). 8 failures give SENSOR LOST; `off` lets the 3 s retry recover it. Needs a sensor.
- `fault nh`: one INT_NH event through `onNoiseHigh`. The NF raise limit applies, so call it about every 2 s: NF climbs to 7, the next call trips ENV TOO NOISY. Works without a sensor. `resetfilters` clears it.
- `fault cfg`: writes a wrong value into REG0x01 and makes the config check due now, so `config lost - re-init` can be seen. Needs a sensor.
- `burst` and `strike` are real strikes: they end the demo and go into the records. Use `demo on` for tests that must not touch the records.
- `resetrec`, `diag reset`, `factory`, `flush`
- `hang` (blocks until the task watchdog reboots), `rst` (software restart)
- `shot`: dumps the screen as `[SHOT] <row> <240 x RGB565 hex>` lines between `begin` and `end`

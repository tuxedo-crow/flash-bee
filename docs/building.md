# Building and flashing

Parts, wiring, the enclosures, flashing the firmware and rebuilding the PDF guides. The full illustrated build is the "Build your own" chapter of `Flash Bee Guide.pdf`.

## Documentation

- `Flash Bee Quick Start.pdf`: two pages for the first five minutes (switching on, the Main page, pages, alerts, charging, demo, safety).
- `Flash Bee Guide.pdf`: the full user guide, plus a "Build your own Flash Bee" chapter (parts, printing, wiring, flashing, first test).

Both are built from `guide/make_guide.py` (reportlab; photos in `guide/images`, gokux's build photos are CC BY-NC-SA 4.0). Rebuild them after UI or hardware changes:

```
python3 guide/make_guide.py guide/images "Flash Bee Guide.pdf"
python3 guide/make_guide.py --quick guide/images "Flash Bee Quick Start.pdf"
```

Without reportlab installed, prefix each with `uv run --with reportlab --with pillow`.

## Flashing

1. Arduino IDE, Boards Manager: install **esp32 by Espressif**. Use 3.x if you might use the C6.
2. Install libraries via Sketch > Include Library > Add .ZIP Library:
   - https://github.com/Seeed-Studio/Seeed_Arduino_RoundDisplay
   - https://github.com/Seeed-Studio/Seeed_GFX
   - PubSubClient (Library Manager, or `arduino-cli lib install PubSubClient`; `flash.sh` installs it)
   - Remove or rename any stock TFT_eSPI library first. Seeed_GFX is a fork of it and they conflict.
3. Open `FlashBee/FlashBee.ino`.
4. Board: **XIAO_ESP32C3** (or XIAO_ESP32C6). `driver.h` uses D-pin names, so no board edit is needed.
5. Round Display's onboard switch: ON.
6. Upload. Serial monitor at 115200.

## Hardware notes

- AS3935 Grove board: remove the JST socket and bridge 0-3V, REG, CS, SI1 (I2C mode, 3.3 V). I2C address 3.
- Wiring: D4 to SDA, D5 to SCL, 3V3 to VCC, GND to GND.
- Battery: on the Round Display's JST 1.25 socket. Only + goes through the slide switch. The switch is in series, so the device must be on to charge.
- The display charges at about 485 mA. Use a cell of at least 500 mAh, or confirm a 400 mAh cell allows a 0.5 A charge.
- Battery reading: the Round Display's KE switch (a 2-toggle DIP between the microSD slot and the XIAO header; set both toggles ON) must be ON. ON connects the battery divider to A0/D0 and the backlight control to D6; the digital side frees both pins. It connects a divider (cell voltage / 2) to D0. The percentage comes from a Li-ion discharge table (`battPctFromMv` in `battery.ino`): 100 % at 2100 mV at D0 (4.20 V), 15 % at 1850 mV (3.70 V), 0 % at 1650 mV (3.30 V), linear between the points, after the reading is scaled by `BATT_CAL_PERMILLE` (1031: this C3 read a full cell as 2039 mV). The table is a generic single-cell curve, not measured on this pack. Below 1500 mV or above 2300 mV, or with noisy samples, the screen shows "--".
- Test with a piezo lighter spark near the sensor.

### Solderless wiring (microSD slot)

- Instead of soldering to D4/D5, plug a microSD "sniffer" adapter into the Round Display's microSD slot and wire the sensor to it. The firmware never uses the SD card.
- CS/DAT3 (card pin 2, XIAO D2) to SDA. DO/DAT0 (card pin 7, XIAO D9) to SCL. VCC to VCC. GND to GND. The display already has 4.7k pull-ups on both lines.
- The sensor keeps I2C address 3. Remove the SD card. Do not use both wirings at once.
- At boot the firmware tries D4/D5 first, then the microSD slot, and logs `[AS3935] Found on D4/D5` or `[AS3935] Found on microSD slot (D2/D9)`. The console `status` line shows `bus=D4D5` or `bus=SD`.
- This bus is bit-banged in software at about 100 kHz, because the C3 has one I2C controller and the touch panel uses it. Not tested on hardware yet.

## Solderless enclosure

`enclosure/flashbee_case.scad` (OpenSCAD, `part` = front, back, lever, fit_test, assembly, exploded). Sliced STL and bgcode for the Core One+ (PETG, 0.20 mm BALANCED) are in `enclosure/exports/`. Outside size 58 x 84.5 x 30 mm. The only soldering left is the four AS3935 bridges (0-3V, REG, CS, SI1). Every JST socket stays on.

Parts:

- XIAO ESP32-C3 with pre-soldered headers, Round Display for XIAO, Grove AS3935 (Grove socket kept).
- microSD sniffer adapter with the 8-pin header already fitted, pins on the side without the gold fingers (SparkFun TOL-09419 size: 47 x 28 mm, 24 mm tongue).
- Grove to 4-pin female jumper cable (20 cm).
- 1S LiPo with a JST 1.25 (PicoBlade) plug, up to 26 x 26 x 12 mm (902025 or 102525). Check the polarity against the display's BAT socket before plugging in.
- 2 x M3 x 10 screws.

Print: front (face down, 55 min), back (31 min), lever (2 min). Print `fit_test` first (8 min): the display must drop into the ring and sit flat under the lip.

Assembly:

1. Bridge the four pads on the AS3935. Set the Round Display's KE switch to ON.
2. Put the display face down into the front. The FPC fold goes to the gap in the ring (right side), the XIAO USB-C points to the top opening.
3. Push the sniffer tongue fully into the display's microSD slot (gold fingers toward the display). Its header lands on the left.
4. Wire the Grove cable to the sniffer: yellow SCL to DO/DAT0, white SDA to CS/CD/DAT3, red VCC to VCC, black GND to GND. Plug the Grove end into the sensor.
5. Put the lever in from the inside: tab out through the slot in the right wall, then lift the hub onto the pin; the fork goes over the display switch knob.
6. Snap the sensor into the lid cradle (components and Grove socket toward the lid, socket on the left). Put the battery in the bay and plug it into the display's JST socket. Coil the spare cable in the chin, left of the sniffer.
7. Hook the lid's top tongue into the groove, close it, and fit the two M3 screws.

The thumb tab works the display's own switch. The charger stays on with the switch off, so the device charges while it is off.

Not checked on hardware: the knob travel and the lever fork (from the footprint drawing), the sensor board size (42 x 23 mm estimate), the sniffer outline (SparkFun drawing), and which screen rotation is upright. The display is turned 90 degrees from the original build, so if the picture is sideways, send `set rot 1` (or `set rot 3`) on the serial console.

## Travel case

`enclosure/flashbee_travel_case.scad` is a protective clamshell for the assembled device (gokux's case). It prints open and flat in one piece with no supports: two 12 mm deep trays joined by a five-knuckle print-in-place hinge, closed by two 10 x 5 x 3 mm block magnets glued into bosses on the front walls. Inside 54.6 x 99.9 x 24.0 mm; as printed 141.4 x 104.4 x 16.7 mm.

Sliced files for the Prusa Core One HF 0.4 are in `enclosure/exports/` (`flashbee_travel_case_*`), made for a semi-rigid flexible PETG with the Generic PLA Silk profile. Print `fit_test` first: it has one hinge pair and both magnet wells.

Not printed yet: the hinge gaps, the fit around the device and the magnet hold are untested. Open the case gently the first time to break any stringing in the hinge.

## Enclosure concept (v3)

`enclosure/concepts/v3/` is a looks-only study (not printable) of how the solderless case could get back the shape of gokux's original. See its README.

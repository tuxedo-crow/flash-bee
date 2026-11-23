# Using Flash Bee

What the screens mean and what to do when it warns you. The same material, with pictures, is in `Flash Bee Quick Start.pdf` and `Flash Bee Guide.pdf`.

**Turning it on and off.** Slide the switch on the bottom of the case. The screen lights up and the device starts listening for lightning. Slide it back to switch off.

**Pages.** Swipe left or right across the screen to change pages. The dots at the bottom show where you are. If you leave it on another page for 2 minutes, it goes back to the main page by itself. When real lightning is detected, it jumps to the main page too.

1. **Main.** Until lightning is detected, it shows "--" and "waiting..." while green rings move in toward the centre. That means it is listening, not stuck. The rings turn red during SHELTER. They do not point anywhere: the sensor cannot tell the direction of a strike. It can show that for days between storms. After a strike:
   - The big number is the distance to the strike. "OVERHEAD" means less than 1 mile. ">25 mi" means too far to measure.
   - "STRIKES" counts strikes since it was switched on. "ENERGY" is how strong the last one was.
   - "12s ago" tells you how long ago the last strike was. After 15 minutes without a strike the distance turns dim grey: it is an old reading then.
   - APPROACHING, MOVING AWAY or STEADY tells you where the storm is going.
   - The small battery shows how full the battery is. It turns red-orange under 15 %. "--" means no reading: the battery is not connected (switch the device on), or the small KE switch on the screen board is not ON.
   - `STAT`, `PORT` or `CHAS` at the top shows the sensor profile (see Settings). A small WiFi symbol right of the battery shows when WiFi is on (filled = connected).
2. **Storm chart.** The last hour. Each dot is a strike: left is an hour ago, right is now, low is close, high is far. The dashed red line is 10 miles. Bars at the bottom count strikes in each 5 minutes.
3. **Flash-to-bang.** Time the thunder yourself. Tap the big circle when you see the flash. Tap again when you hear the thunder. It shows the seconds and the distance (sound travels about 1 mile in 5 seconds). If the sensor heard that strike, it shows its distance too, so you can compare. Tap once more to start again. After 60 seconds without thunder it says "too far to hear".
4. **Records.** All-time records: closest strike, strongest strike, biggest storm, all strikes, number of storms. They stay when you switch off. Only real strikes count. To clear them, hold your finger on RESET for 3 seconds until the bar is full. A tap does nothing, so you cannot do it by accident.
5. **Settings.** Tap a button to change it:
   - **Profile.** Tap to step through AUTO, STATION, PORTABLE and STORMCHASE. Leave it on **AUTO**: it picks STATION at home (while the WiFi is connected) and PORTABLE away from home. STORMCHASE (red) is for watching a storm yourself: it reports every flicker, so it gives false strikes if you leave it on, and it forgets itself when you switch off. The main page shows the profile at the top (`STAT`, `PORT`, `CHAS`).
   - **Units.** Miles or km.
   - **Brightness.** Three levels. Lower saves battery.
   - **Screen off after.** How long the screen stays on without a touch or a strike (30 seconds to 10 minutes, or never). A touch or a strike lights it again; the first touch only wakes it. It stays on while the alert says SHELTER.
6. **Nerd stats.** Sensor numbers, battery voltage, how long it has been on. RESET FILTERS puts the sensor's noise settings back to normal (use it if the sensor stays deaf after a noisy spell). It does not change the alert.

**Alert.** At the bottom of the main page: a small orange ACTIVITY means lightning was sensed in the last 15 minutes. A big flashing SHELTER (red and orange, with a flashing ring) means go indoors: a strike 10 miles or closer in the last 30 minutes, or 5 or more strikes in the last 15. It cannot be dismissed; it goes away by itself. "ENV TOO NOISY" at the top means the sensor is swamped by electrical noise and cannot be trusted. "SENSOR LOST" means the sensor stopped answering; it keeps trying to reconnect.

**All clear.** Lightning can strike 10 miles from a storm. When a strike is 10 miles or closer, the main page shows "ALL CLEAR IN 30:00" in red and counts down. Every new close strike starts it again from 30:00. When it reaches zero, "ALL CLEAR" shows in green: 30 minutes without close lightning. That is the rule weather services use for going back outside.

**Demo.** To see how it looks without a storm, hold a finger still on the screen for 3 seconds on any page. A bar fills up. The words "DEMO - NOT REAL" and a magenta ring show that the strikes are fake. Hold 3 seconds again to stop. The demo also stops by itself after 10 minutes, if a real strike is detected, or when you switch off. Demo strikes never go into your records.

**Network.** Optional. Without a WiFi name set the radio is never started. See [firmware.md](firmware.md#network) for the technical side; on the Nerd page the WIFI row shows the address and signal.

**Charging.** Plug in the USB-C cable. The device must be switched on to charge. While it is plugged in, the percentage reads high and reaches 100 % before the pack is really full, so leave it on the charger a while longer; the number is only accurate on battery.

**Battery.** The percentage follows a Li-ion curve: 100 % at 4.20 V, 15 % at 3.70 V, 0 % at 3.30 V. The battery icon turns red-orange under 15 %. A charge lasts about 10 hours with WiFi on (one measured night on the 400 mAh pack: unplugged about 9.3 hours and still running). Runtime with WiFi off is not measured yet. The Nerd page shows the cell voltage next to the percentage.

**Safety.** Lightning can strike 10 miles from a storm. If strikes show 10 miles or closer, or you hear thunder, go indoors and wait 30 minutes after the last one.

**Important.** This is a hobby device, not a safety device. It can miss strikes. When you hear thunder, go indoors.

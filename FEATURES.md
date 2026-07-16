# Feature tour

Everything below works today (firmware v1.3) with the parts listed in the
[README](README.md). The device is fully standalone: no phone, no subscription,
no internet — a phone only adds comfort (app streaming, pit-mode web app).

---

## On track

### 1. Automatic lap timing
Ride across the start/finish line: the lap is detected, timed and displayed —
nothing to press, ever. The firmware traces a 40 m virtual gate over the line
and interpolates the exact crossing moment between two GPS fixes, which gives
**~0.1-0.3 s repeatability** out of a 5 Hz receiver. Wrong-direction passes,
slow passes (< 15 km/h) and double detections (< 30 s) are filtered out, so
the pit lane and the grid don't create ghost laps.

**Use it:** first time on a track, do one sighting lap; on the LINE page,
long-press while crossing the real line at > 10 km/h. Done forever — the line
is stored (see feature 5).

### 2. Live predictive delta
The bottom-left of the RACE page shows the gap between the lap you're riding
and your all-time best lap on this track, compared **at the same distance
along the lap**, refreshed 5×/s: `-0.42` = you're up, `+0.87` = you're down.
When you're gaining, the number displays in inverted video — readable in one
glance at 150 km/h. Because the reference lap is saved to flash, the delta is
live **from lap 1** of any later track day.

### 3. Sectors and theoretical best
The reference lap is split into 3 equal-distance sectors. Every completed lap
shows the per-sector deltas vs your best sectors on the end-of-lap flash
(`-0.2 +0.5 -0.1`): you instantly know *where* you gained or lost. The SESSION
page shows the **theoretical best** — the lap you'd do by combining your best
sectors. Chasing it is the whole game of a track day.

### 4. End-of-lap flash
Each completed lap takes over the screen for 4 seconds, in inverted video:
lap number, big lap time, delta vs best (or `RECORD!`), sector deltas. Sized
to be read from a tank-mounted 0.96" screen with a helmet on.

### 5. Named tracks with persistent records
Each start line lives in a track file in internal flash: name, line, all-time
best, best sectors and the full reference-lap trace. At power-on, the nearest
stored track within 5 km is loaded automatically — arrive at MSP, turn the
device on, it says `TRACK MSP` and your records are armed. Rename, select or
delete tracks from the pit-mode web app or the serial console (`T`, `N`).

### 6e. Live pit telemetry (LoRa)
With a 150 ฿ LoRa radio in the box and a second ESP32 in the pits, a friend or
coach follows you live from the pit wall: predictive delta in big (inverted
when you're up), current/last/best lap, session, speed, lean, tire temps —
refreshed every second with 1-2 km of range. The pit box has no buttons: power
it and it listens. Fully customizable packet (`src/pitlink_proto.h`).

### 6d. Tire temperatures (2× MLX90614 IR)
Two contactless IR sensors (front fender, tail) read the tread temperature
live: TIRES page, per-lap averages in the CSV (`tire_f_c`, `tire_r_c`) and
RaceChrono channels (PID 0x102). Know when your tires are in their window,
how many warm-up laps they need, and whether they overheat late in a session.
On a dedicated I2C bus so the long cable runs never disturb the display.

### 6c. Lean angle & G-forces (MPU6050)
A 50 ฿ GY-521 IMU on the I2C bus adds the **live lean angle** (with L/R side),
the **max lean per lap on each side** and the **braking G peak** — on the LEAN
page, in the lap CSV (`lean_max_deg`) and as RaceChrono channels (PID 0x101).
One-command calibration (`g`, bike upright). The estimate blends the roll gyro
with speed × yaw-rate kinematics — the honest approach for a bike, where a
plain accelerometer reads ~0° mid-corner.

### 6b. ECU telemetry (Kawasaki KDS)
With an L9637D transceiver (~40 ฿) on the bike's diagnostic plug, the device
reads **RPM, throttle position, gear, coolant temperature and speed** straight
from the ECU. The ECU page shows the gear big, RPM and a live throttle bar —
and everything is streamed to RaceChrono as extra channels, so your session
analysis shows *"in turn 3 you only opened 60% throttle and short-shifted"*.
Wiring, bike verification steps and RaceChrono channel formulas are in the
README (Step 7).

### 6. Automatic sessions
A stop longer than 5 minutes (pit stop, red flag, lunch) starts a new session
on the next crossing: the lap list and session stats reset, the track records
stay. The SESSION page shows the session number, lap list (best marked `*`),
session vmax, average lap and theoretical best. The screen also dims by itself
after 1 minute without movement and wakes on any button or when you ride off.

---

## Off track

### 7. Lap log (CSV)
Every lap is appended to `/laps.csv` in internal flash:
`date, time_utc, track, session, lap, time_s, vmax_kmh`. Retrieve it over USB
(serial command `d`) or over WiFi (pit mode) — then open it in any
spreadsheet. Survives power-off; ~years of track days fit in flash.

### 8. Pit mode: the phone web app + OTA updates
Long-press on the GPS page: the device reboots as a WiFi hotspot
(**LapTimer ESP32** / password **laptimer**) serving a full web app at
**http://192.168.4.1** — add it to the iPhone home screen once and it opens
full-screen with its own checkered-flag icon, like a native app:

- **Live tab**: real-time dashboard (delta/current lap in big, speeds, temps)
  — a phone on the hotspot doubles as a pit board,
- **Sessions tab**: per-session lap-time charts, best/average/vmax/lean and
  the full lap table, straight from the on-device log,
- **Tracks tab**: select / rename / delete stored tracks,
- **System tab**: CSV download (or one-tap sync via an iOS Shortcut — recipe
  in the README), **OTA firmware update from the browser**, remote exit.

Once the box is sealed and mounted on the bike, you never need the USB cable
again. No app store, no account, no backend: the app lives inside the device.

### 9. Phone app streaming (Bluetooth)
The device can feed a lap-timing app in parallel with the on-board timer:

- **RaceChrono** (iOS + Android) — default build, BLE, "RaceChrono DIY"
  protocol. Full analysis on the phone: traces, sectors, comparisons.
- **TrackAddict, Harry's LapTimer, LapTrophy & others** (Android) — flash the
  `esp32dev-nmea` build: the device becomes a standard Bluetooth NMEA GPS.

See the README for pairing steps. iOS locks Bluetooth GPS to certified
hardware, so on iPhone only the RaceChrono path works.

---

## For the workbench

### 10. Simulation mode
`pio run -e esp32dev-sim -t upload`: the device rides a virtual track by
itself — laps, deltas, sectors, flashes, app streaming all work with no GPS
and no sky view. Perfect to test the display, the buttons and the phone apps
from the couch.

### 11. Replay mode
`pio run -e esp32dev-replay -t upload`: NMEA lines streamed to the USB port
are treated as GPS input, so a recorded session can be re-run against the
timing code (e.g. `pv -q -L 900 session.nmea > /dev/cu.usbserial-XXXX`).

### 12. Unit-tested timing core
The timing logic has no Arduino dependency and runs on your computer:
`pio test -e native` — 9 tests covering crossings, interpolation, midnight
wrap, debounce, sessions, sectors and the predictive delta, in ~2 s.

### 13. Serial console
115200 baud, type `h`: info, track management, CSV dump, resets, manual line
entry, pit-mode toggle. Handy for bench work; everything it does is also
reachable from the buttons or the web app.

---

# Future features and the shopping list

## To finish the current build (v1 hardware)

Already covered in the [README parts list](README.md#1-parts-list-and-what-each-part-does),
reminder of what is **not** in the original Shopee order:

| Buy | ~Price | Needed for |
|---|---|---|
| LiPo 3.7 V ≥ 1000 mAh | 100-200 ฿ | running on the bike (a power bank on USB-C works meanwhile) |
| MT3608 boost converter | 15-30 ฿ | stable 5 V from the LiPo |
| Slide switch | 10-20 ฿ | on/off |
| Waterproof 12 mm push button | 20-40 ฿ | glove-friendly control (BOOT button works meanwhile) |
| Female-female Dupont wires | ~20 ฿ | module-to-module wiring (check what shipped) |
| Quad Lock universal adapter | varies | tank mounting |
| Soldering iron + multimeter | 350-700 ฿ | assembly (multimeter is mandatory for the MT3608 step) |

## To unlock new features

Each line = one purchase → one feature. "Code status" says how much firmware
work remains once the part is on the desk.

| Buy | ~Price | Feature unlocked | Code status |
|---|---|---|---|
| **u-blox NEO-M8N or M10 GPS module** | 250-400 ฿ | 10-25 Hz fixes → lap precision from ~0.2 s down to **~0.05 s**, sharper predictive delta | ✅ Works as-is: same wiring, same UBX config — just raise `GPS_MEAS_RATE_MS` |
| **L9637D + 510Ω/10kΩ + 4-pin plug** | ~150 ฿ | **ECU telemetry**: RPM, throttle, gear, coolant → screen + RaceChrono channels | ✅ Firmware ready (v1.4) — wire it and it links (README Step 7) |
| **MLX90614 IR sensor ×2** | 300-500 ฿ | **Tire temperatures** front/rear: live page, per-lap CSV averages, RaceChrono | ✅ Firmware ready (v1.6) — wire + re-address one (README Step 9) |
| **SX1278 LoRa 433 ×2 + 2nd ESP32 + 2nd OLED** | ~600 ฿ | **Live pit telemetry**: coach screen with delta, laps, temps, 1-2 km range | ✅ Firmware ready (v1.7) — bike auto-detects, pit box = `pio run -e pitbox` |
| **MPU6050 IMU** (accelerometer + gyro) | 40-80 ฿ | **Lean angle** display and per-lap max, G-force logging | ✅ Firmware ready (v1.5) — plug it on the I2C bus, calibrate with `g` |
| **2.42" SSD1309 OLED** | 150-250 ฿ | Twice the screen: bigger digits, delta readable further into the fairing | ✅ One line to change (U8g2 constructor) |
| **2 × 100 kΩ resistors** | ~5 ฿ | **Battery gauge** on the top bar (voltage divider into an ADC pin) | 🔧 Small addition (~30 lines) |
| **Second push button** | ~20 ฿ | One-press manual markers (pit-in, traffic) or quicker page UX | 🔧 Small addition |
| **Active GPS antenna** (if your module has a u.FL socket) | 100-150 ฿ | Better fix under marginal sky (garages, grandstands) | ✅ Plug and play |

### Deliberately *not* on the list

- **microSD module** — internal flash already stores years of laps, and pit
  mode downloads them over WiFi.
- **Buzzer** — inaudible with a helmet at speed; the inverted-video flash does
  the job.
- **Commercial GPS lap timer** — that's the whole point of this repo. 🏁

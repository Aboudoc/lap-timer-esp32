# ESP32 GPS Lap Timer 🏍️⏱️

A DIY, fully standalone GPS lap timer for motorcycle track days (built for a
Ninja 400 at MSP Bangkok). Tank-mounted via Quad Lock, it automatically detects
every pass over the start/finish line and shows:

- the **current lap time** in big digits,
- a **live predictive delta** vs your all-time best lap, updated as you ride
  (`-0.42` = you are currently faster than your best), available from lap 1
  thanks to the persisted reference lap,
- on every completed lap: the **lap time** full-screen for 4 s with the delta
  vs best and the **3 sector deltas** (`-0.2 +0.5 -0.1` — where you gained,
  where you lost),
- **named tracks**: line, all-time best, best sectors and reference lap are
  stored per track and the nearest track is auto-selected at startup,
- **automatic sessions**: a stop longer than 5 minutes starts a new session,
  with per-session stats (average, theoretical best),
- it **logs every lap to a CSV file** in its internal flash,
- a **pit mode** turns it into a WiFi hotspot with a web app: download the lap
  log, manage tracks and **update the firmware over the air** — no cable,
- and it can **stream the GPS to your phone over Bluetooth**: to the RaceChrono
  app (BLE, iOS + Android) or as a generic Bluetooth NMEA GPS for TrackAddict,
  Harry's LapTimer and mock-location apps (Android).

The start line is set once, while riding, with a single button press — then it
is remembered for every future session. Real-world precision: ~0.1-0.3 s
(5 Hz GPS + crossing interpolation).

➡ **[FEATURES.md](FEATURES.md)** — the full feature tour, and the
purchase-to-feature roadmap (what to buy to unlock what).
➡ **[TODO.md](TODO.md)** — the project roadmap (A → E) with the shopping
list each project needs.

---

## 1. Parts list and what each part does

### Electronics

| Part | Role | Approx. price |
|---|---|---|
| **ESP32 DevKit** (CH340, USB-C) | The brain. Reads the GPS, computes lap times, drives the display. Programmed over USB. | ~160 ฿ |
| **GY-NEO-6M GPS** + antenna | Provides position, speed, heading and precise time, 5 times per second once configured. | ~110 ฿ |
| **0.96" SSD1306 OLED (I2C)** | Display. High contrast, sunlight-readable, near-zero power draw. | ~85 ฿ |
| **TP4056** (micro-USB) | LiPo battery charger: you recharge the device like a phone. | ~15 ฿ |
| **3.7 V LiPo battery ≥1000 mAh** | Powers everything. 1500 mAh ≈ 6-8 h runtime, plenty for a track day. | ~150 ฿ |
| **MT3608 boost converter** | Steps the battery's 3.7 V up to a stable 5 V. Without it the ESP32 + GPS would brown out (voltage too marginal). | ~20 ฿ |
| **Slide switch** | On/off between the battery and everything else. | ~10 ฿ |
| **12 mm waterproof push button** *(optional)* | Change pages / set the line with gloves on. The ESP32's BOOT button does the same job to get started. | ~25 ฿ |
| **Dupont wires** + **ABS enclosure** | Prototyping wiring and protective case. | ~50 ฿ |

> ⚠️ **Dupont wires**: to connect two modules that both have male pins
> (ESP32 ↔ OLED/GPS) you need **female-female** wires. Male-female is for
> breadboards. Check what you have; an F-F pack costs ~20 ฿.

> 💡 **No battery yet?** A small power bank on the ESP32's USB-C port replaces
> the whole battery/TP4056/MT3608 chain — zero power-related soldering to get
> started.

### Tools

| Tool | What for |
|---|---|
| Soldering iron + solder | Solder the GPS header pins (shipped loose), the battery wires, and the final vibration-proof assembly |
| Multimeter | **Mandatory**: set the MT3608 to 5 V before connecting the ESP32 (it ships outputting >20 V!) |
| Hot glue, 3M foam tape, heat shrink | Secure connectors and the antenna against motorcycle vibrations |
| Quad Lock universal adapter | Sticks to the back of the enclosure → the device clips onto the tank mount |

---

## 2. How it works (the principle)

The GPS reports a position 5 times per second. The firmware defines a
**virtual gate**, 40 m wide, centered on the start/finish line, perpendicular
to the direction of travel:

```
                       │◄────── 40 m ──────►│
      ─ ─ ─ ─ ─ ─ ─ ─ ─┼────────────────────┼─ ─ ─ ─ ─ ─
                            ▲    ▲
                       t₁ ● │    │           t₁, t₂: two consecutive GPS fixes (0.2 s)
                            │    │           The exact crossing moment is interpolated
                       t₂ ● │    │           between them → precision ≈ 0.1 s
                        direction of travel
```

When the segment between two consecutive positions cuts the gate **in the
right direction**, a lap is counted. The exact crossing moment is interpolated
between the two fixes (using GPS time, millisecond-accurate) — that is what
makes the precision much better than "one fix every 0.2 s".

Built-in guards: 30 s minimum lap time, 15 km/h minimum speed, wrong-direction
crossings ignored → no false laps in the pits or while stopped on the grid.

**Predictive delta:** while you ride, the firmware records a distance → time
trace of the lap. Whenever a lap becomes your all-time best on this track, its
trace becomes the reference — and it is saved to flash. From then on, your
progress is compared against the reference *at the same distance along the
lap*, five times per second — the bottom-left of the RACE page shows `-0.42`
(you're up) or `+0.87` (you're down), inverted video when you're gaining.
Since the reference survives power-off, the delta is live from lap 1 on your
next track day.

**Sectors:** the reference lap is split into 3 equal-distance sectors. Every
completed lap shows the delta of each sector vs your best sectors, and the
SESSION page shows the **theoretical best** (sum of your best sectors) — the
lap you could do if you nailed every sector.

**Tracks:** each start line lives in a named track file (line, all-time best,
best sectors, reference lap). At startup, the nearest stored track within 5 km
is loaded automatically. Manage tracks from the pit-mode web app or the `T`
serial command.

**Sessions:** if no lap is completed for more than 5 minutes (red flag, pit
stop, lunch), the next crossing starts a fresh session automatically — lap
list and session stats reset, track records stay.

---

## 3. Assembly, step by step

Every step has a checkpoint ✓ — only move on when it passes.

### Step 0 — Set up the toolchain (nothing wired yet)

1. Install [VS Code](https://code.visualstudio.com/) and the **PlatformIO IDE**
   extension (or just the CLI: `pip install platformio`).
2. Clone this repo and open the folder.
3. Plug the ESP32 in over USB.

✓ *Checkpoint:* `pio device list` (or the PlatformIO icon) shows a port like
`/dev/cu.usbserial-*` or `/dev/cu.wchusbserial-*` (macOS; the CH340 driver is
built into recent macOS versions).

### Step 1 — The display, in simulation mode

Wire the OLED to the ESP32 (4 wires, ESP32 **powered off**):

| OLED | ESP32 |
|---|---|
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO21 |
| SCL | GPIO22 |

Flash the **simulation mode** — the device drives around a virtual track, no
GPS needed:

```bash
pio run -e esp32dev-sim -t upload
```

✓ *Checkpoint:* splash screen, then the timer runs by itself; roughly every
40 s the screen inverts with a lap time. The **BOOT** button switches pages
(short press) and resets the session (long press on the SESSION page). This is
exactly the behavior you will get on track.

### Step 2 — The GPS

Still powered off, wire the GPS (solder its header pins first if they shipped
loose):

| GPS NEO-6M | ESP32 |
|---|---|
| VCC | VIN (5V) |
| GND | GND |
| TX | GPIO16 |
| RX | GPIO17 |

Flash the real firmware and open the serial console:

```bash
pio run -e esp32dev -t upload
pio device monitor
```

Go **outside** (or near a window, antenna facing the sky) and allow 1-5 min
for the first fix (later fixes take seconds).

✓ *Checkpoint:* the console prints `[GPS] configured: 57600 baud, 200 ms/measurement`;
the GPS page shows `FIX`, `5.0Hz`, 7+ satellites, HDOP < 2, and your position.
The RACE page shows your speed.

### Step 3 — Full-scale test (bicycle or scooter)

1. Pick a loop (parking lot, city block).
2. LINE page → ride faster than 10 km/h → **long press** while passing over
   your imaginary "line" → `LINE SET!`.
3. Do laps (laps must be > 30 s and cross the line at > 15 km/h).

✓ *Checkpoint:* every pass flashes the lap time; the SESSION page lists the
laps; `d` in the serial console dumps the CSV.

### Step 4 — Battery power

⚠️ *The only step where you can fry something. Multimeter mandatory.*

1. Solder the battery to the TP4056: `B+`/`B-` (mind the polarity!).
2. Solder TP4056 `OUT+` → switch → MT3608 `IN+`, and `OUT-` → `IN-`.
3. **Before connecting the ESP32**: power on, measure the MT3608 output with
   the multimeter and turn the little potentiometer screw until it reads
   **5.0-5.2 V** (it takes many turns, that's normal).
4. Power off, solder MT3608 `OUT+` → ESP32 **VIN** and `OUT-` → **GND**.

```
LiPo ──► TP4056 ──► switch ──► MT3608 (set to 5 V) ──► ESP32 VIN + GND
          ▲
     micro-USB = recharge
```

✓ *Checkpoint:* everything runs on battery, no USB cable. Recharge through the
TP4056's micro-USB (red LED = charging, blue/green = full). Flashing over USB
with the battery connected is fine, no conflict.

### Step 5 — Enclosure and bike mounting

1. Cut the enclosure: display window (plastic film or acrylic glued behind),
   micro-USB opening (recharging), waterproof button hole (GPIO25 → button →
   GND), switch hole.
2. **GPS antenna facing the sky**, nothing metallic above it, mounted on 3M
   foam tape (anti-vibration). The GPS module can stay inside the box if the
   lid is plastic.
3. Hot glue on every Dupont connector (or solder them: more reliable on a
   vibrating motorcycle), silicone on cable pass-throughs.
4. Quad Lock universal adapter glued/screwed to the back → clips onto the tank
   mount.

✓ *Final checkpoint:* shake the box — nothing moves, nothing rattles.

### Step 6 — Track day

1. Power on in the paddock, clear sky view, 2-3 min before going out.
2. First time on this track: LINE page, **long press while crossing the actual
   line** during the sighting lap. It is stored for every following session
   and track day.
3. Ride. Everything is automatic — don't touch anything.
4. In the evening: plug in USB, `pio device monitor`, type `d`, copy your
   times (times are UTC; Bangkok = UTC+7).

### Step 7 — ECU bridge (optional, Kawasaki)

Reads **RPM, throttle, gear, coolant temp and speed** from the bike's
diagnostic plug (Kawasaki KDS protocol over K-line), shows them on the ECU
page and streams them to RaceChrono. Parts: an **L9637D** K-line transceiver,
a **510 Ω** and a **10 kΩ** resistor, and the bike-side 4-pin connector.

**First, verify the bike (before soldering anything):**

1. Find the diagnostic connector: on the Ninja 400 it is a **white 4-pin
   plug** under the seat, taped to the harness near the battery/ECU.
2. With a multimeter, ignition ON:
   - one pin has continuity to the battery minus → **GND**;
   - one pin sits at battery voltage (~12 V) → **+12 V supply**;
   - the pin that idles around **10-12 V** and is neither of the above is the
     **K-line** → you're good, this firmware applies as-is;
   - if instead you find **two pins around 2.5 V each**, the bike uses
     **CAN**: don't wire the L9637D — the bridge then needs a CAN transceiver
     and a small firmware change (open an issue / ask).
3. Wire the L9637D as shown in Step 5's diagram box below:

```
ESP32 GPIO27 (TX) ──────────── L9637D TX
ESP32 GPIO26 (RX) ──────────── L9637D RX ──[10k]── 3V3 (pull-up)
Bike K-line pin  ──────────── L9637D K   ──[510R]── +12V
Bike +12V (fused) ──────────── L9637D VS
Bike GND ── ESP32 GND ──────── L9637D GND      (one common ground!)
```

✓ *Checkpoint:* ignition ON (engine off is fine): the serial console prints
`[KDS] ECU link established` and the ECU page shows gear, RPM and the
throttle bar. Blip the throttle: the bar follows. If nothing after ~10 s, the
firmware retries every 5 s — check TX/RX and the ground.

**RaceChrono channels:** the engine data is streamed as a CAN frame with
PID `0x100` (bytes: 0-1 RPM big-endian, 2 throttle %, 3 gear, 4 coolant
°C + 40, 5 speed km/h). In RaceChrono: add channels on the DIY device with
PID 256 and the formulas `bytesToUInt(raw, 0, 2)` (RPM),
`bytesToUInt(raw, 2, 1)` (throttle %), `bytesToUInt(raw, 3, 1)` (gear),
`bytesToUInt(raw, 4, 1) - 40` (coolant °C).

---

## 4. Usage — quick reference

### Pages (short press = next page)

| Page | Content | Long press |
|---|---|---|
| **RACE** | current lap (big), speed, sats, live delta + Best (or Last/Best) | — |
| **SESSION** | session #, laps (best `*`), vmax, theoretical best, average | reset session |
| **ECU** | gear (big), RPM, throttle bar, coolant temp (KDS bridge) | — |
| **GPS** | fix, satellites, HDOP, rate Hz, position | **toggle pit mode (WiFi)** |
| **LINE** | active track, line status and distance | set the line here |

On the RACE top bar, `B` in front of the satellite count means a phone app is
connected over Bluetooth, `W` means the pit-mode hotspot is up. The screen
dims by itself after 1 minute without movement (any button or riding wakes it).

### Pit mode: WiFi web app + firmware updates

Long-press on the GPS page (or serial command `w`): the device reboots into a
WiFi hotspot — network **"LapTimer ESP32"**, password **`laptimer`**, then
open **http://192.168.4.1** in the phone browser:

- download or erase the lap log (CSV),
- list / select / rename / delete tracks,
- **flash a new firmware over the air**: menu *Firmware update*, upload
  `.pio/build/esp32dev/firmware.bin` — no USB cable, no computer at the track.

Bluetooth is off while pit mode is active. Long-press the GPS page again to
reboot back into normal mode.

While the timer is running, the display auto-returns to RACE after 15 s. Long
presses are disabled on RACE and GPS to prevent accidental actions while
riding.

### Serial commands (115200 baud)

| Command | Effect |
|---|---|
| `h` | help |
| `i` | info: track, line, session, records, GPS state |
| `T` / `T <id>` | list stored tracks / select one |
| `N <name>` | rename the active track |
| `d` | dump the CSV log of every lap |
| `x` | erase the CSV log |
| `r` | reset the session |
| `z` | erase the active track records (best, sectors, reference) |
| `w` | toggle pit mode (WiFi hotspot, reboots) |
| `L <lat> <lon> <hdg> [half-width]` | create a track manually (e.g. from Google Maps) |

### Main settings (`src/config.h`)

| Constant | Default | Role |
|---|---|---|
| `LINE_HALF_WIDTH_M` | 20 m | half-width of the detection gate |
| `MIN_LAP_MS` | 30 s | minimum lap time (double-detection guard) |
| `MIN_CROSS_SPEED_KMH` | 15 km/h | minimum speed for a valid crossing |
| `GPS_MEAS_RATE_MS` | 200 ms | GPS rate (5 Hz = NEO-6M maximum) |
| `NUM_SECTORS` | 3 | automatic sectors per lap |
| `SESSION_GAP_MS` | 5 min | stop longer than this = new session |
| `TRACK_MATCH_KM` | 5 km | auto-select range for stored tracks |
| `BT_MODE` | RACECHRONO | Bluetooth mode: `RACECHRONO` (BLE), `NMEA` (Bluetooth Classic), `OFF` |
| `BT_DEVICE_NAME` | LapTimer ESP32 | name the phone sees (Bluetooth + WiFi) |
| `PIT_WIFI_PASS` | laptimer | pit-mode hotspot password |
| `DIM_AFTER_MS` | 1 min | screen dimming delay when parked |
| `PIN_*` | — | full pinout |

Display mounted upside down? Change `U8G2_R0` to `U8G2_R2` in `src/display.h`.

### Phone apps over Bluetooth (optional)

The on-board timer is fully standalone — a phone is never required. But the
device can also feed a lap-timing app, in one of two modes (build-time choice,
the two Bluetooth stacks cannot coexist):

| App | iOS | Android | Mode to flash |
|---|---|---|---|
| RaceChrono | ✅ | ✅ | `RACECHRONO` (default build) |
| TrackAddict | ❌ Apple MFi lock | ✅ | `NMEA` |
| Harry's LapTimer | ❌ | ✅ | `NMEA` |
| LapTrophy & others | ❌ | ✅ via mock location | `NMEA` |

**RaceChrono mode** (default, BLE, lightest on battery):

```bash
pio run -e esp32dev -t upload
```

In the [RaceChrono](https://racechrono.com/) app: **Settings → Add other
device → RaceChrono DIY → "LapTimer ESP32"**, then use it as the GPS receiver
for your sessions — lap analysis, sectors and traces on your phone, recorded
in parallel with the on-board timer.

**Generic NMEA mode** (Bluetooth Classic, Android only):

```bash
pio run -e esp32dev-nmea -t upload
```

The device then behaves like any off-the-shelf Bluetooth GPS receiver
(standard GGA/RMC sentences at 5 Hz). Pair it in the **Android Bluetooth
settings** first, then:

- **TrackAddict**: Settings → GPS → Bluetooth GPS → select "LapTimer ESP32".
- **LapTrophy** and apps without external-GPS support: install a
  mock-location bridge app (e.g. "Bluetooth GPS"), enable it as mock location
  provider in the Android developer options → every app on the phone now uses
  the lap timer's GPS.

Both modes also work in **simulation mode** (`esp32dev-sim` is RaceChrono +
sim) — handy to learn the apps from your couch. Note: NMEA mode draws more
current than BLE (Bluetooth Classic radio, CPU at 160 MHz vs 80 MHz).

---

## 5. The code, briefly

```
src/
├── main.cpp      Orchestrator: main loop, buttons, serial commands, track
│                 auto-selection, simulation/replay modes
├── config.h      Every setting and the pinout
├── fix.h         GpsFix struct (plain data, shared with the unit tests)
├── gps.cpp/.h    Drives the NEO-6M: automatic baud detection, UBX
│                 configuration (5 Hz, useless sentences disabled), clean
│                 "fix" snapshots via TinyGPS++
├── laptimer.cpp/.h  The timing core (no Arduino dependency): gate-crossing
│                 geometry, exact-time interpolation, sessions, sectors,
│                 predictive delta (lap traces + persisted reference lap)
├── ble_racechrono.cpp/.h  "RaceChrono DIY" BLE device (NimBLE): streams the
│                 5 Hz GPS to the RaceChrono app
├── bt_nmea.cpp/.h   Generic Bluetooth Classic NMEA GPS (SPP): GGA/RMC
│                 sentences for TrackAddict & co on Android
├── kds_proto.h   Kawasaki KDS protocol (framing, parsing, unit conversions —
│                 pure logic, unit-tested on the host)
├── kds.cpp/.h    K-line driver: ISO 14230 fast init, echo handling,
│                 non-blocking register polling (RPM, throttle, gear...)
├── pit.cpp/.h    Pit mode: WiFi hotspot, embedded web app (lap log, track
│                 management) and OTA firmware updates
├── display.cpp/.h   The 4 OLED pages + end-of-lap flash (U8g2 library)
├── storage.cpp/.h   Persistence in LittleFS: one file per track (line,
│                 records, reference trace) + the CSV lap log
└── button.cpp/.h    Debouncing, short vs long press detection

test/test_laptimer/  Host-side unit tests of the timing core (9 cases:
                  crossings, interpolation, midnight, sessions, sectors,
                  predictive delta) — run with: pio test -e native
```

The interesting parts:

- **`gps.cpp`** talks to the NEO-6M in u-blox binary (UBX) at boot: it probes
  several baud rates until it hears the module, switches it to 57600 baud,
  disables the NMEA sentences it doesn't need (keeps RMC + GGA only) and
  raises the rate to 5 Hz. If the module doesn't answer, everything still
  works in a degraded 1 Hz mode.
- **`laptimer.cpp`** works in local coordinates (meters around the line). A
  lap = the segment between two fixes moves from "before" to "past" the line,
  inside the gate, in the right direction. The exact time is linearly
  interpolated between the GPS times of both fixes — hence roughly
  tenth-of-a-second precision despite the 5 Hz rate.
- **Four build environments**: `esp32dev` (real), `esp32dev-sim` (built-in
  virtual track to test without a GPS), `esp32dev-nmea` (Bluetooth Classic
  GPS), and `esp32dev-replay` — in replay mode, any NMEA lines pasted or
  streamed to the USB serial port are treated as GPS input, so you can re-run
  a recorded session against the timing code
  (e.g. `pv -q -L 900 session.nmea > /dev/cu.usbserial-XXXX`).
- **Host-side unit tests**: the timing core has no Arduino dependency and is
  covered by 9 unit tests that run on your computer in two seconds:
  `pio test -e native`.

---

## 6. Troubleshooting

| Symptom | Likely cause |
|---|---|
| Black screen | SDA/SCL swapped, or module at address 0x3D (rare): add `u8g2_.setI2CAddress(0x3D * 2);` in `Display::begin()` |
| No serial port | Charge-only USB cable (use a data cable), or missing CH340 driver (built into recent macOS) |
| `no NMEA detected` in the console | TX/RX swapped (cross the wires), or GPS not powered |
| No fix after 10 min | Antenna without sky view (concrete, metal above); first cold start = up to 5 min outdoors |
| GPS page shows ~1.0Hz | The 5 Hz configuration didn't get through: check the RX wire (GPIO17 → GPS RX), reboot outdoors |
| Random reboots on battery | MT3608 badly adjusted or battery empty — measure the voltage on VIN |
| Laps not detected | Line set at the wrong spot/heading (set it again), or laps < 30 s (`MIN_LAP_MS`) |
| Not visible in RaceChrono | NMEA or OFF build flashed instead of the default, pit mode active (Bluetooth off), or another phone is already connected |
| Not visible in TrackAddict | RaceChrono build flashed instead of `esp32dev-nmea`, or not paired in the Android Bluetooth settings first |
| No data in the app despite connection | The device only streams once it has a GPS fix — wait for `FIX` on the GPS page |
| No delta on the RACE page | Normal on the first-ever lap of a track — the delta needs a reference lap. On later track days it is live from lap 1 |
| Wrong track auto-selected | Two stored lines within 5 km of each other: select manually (`T <id>` or the pit-mode web app) |
| Stuck in WiFi mode after an OTA update | Expected — long-press the GPS page (or `w`) to reboot back into normal mode |
| Screen suddenly dark | Parking dim (1 min without movement) — press a button or start riding |
| ECU page stays `---` | Ignition off, K-line not wired (see Step 7), TX/RX swapped, or the bike is CAN-based (check with the multimeter) |
| Throttle % looks wrong | Calibrate: log the raw values at closed/full throttle and adjust `THROTTLE_RAW_*` in `src/kds_proto.h` |

---

## 7. Possible upgrades

Everything code-only is built in. What remains needs hardware — see the
**[purchase-to-feature roadmap in FEATURES.md](FEATURES.md#future-features-and-the-shopping-list)**
(u-blox M8N/M10 for 10-25 Hz precision, MPU6050 for lean angle, bigger OLED,
battery gauge...).

---

## Safety

Mount the device firmly (anything that comes loose at 150 km/h is a
projectile), never fiddle with it while riding — everything is automatic once
the line is set — and check the track's rules about on-board devices.

*A ~600 ฿ DIY project, no warranty of any kind — have fun and stay safe.* 🏁

# ESP32 GPS Lap Timer 🏍️⏱️

A DIY, fully standalone GPS lap timer for motorcycle track days (built for a
Ninja 400 at MSP Bangkok). Tank-mounted via Quad Lock, it automatically detects
every pass over the start/finish line and shows:

- the **current lap time** in big digits,
- a **live predictive delta** vs your best lap, updated as you ride
  (`-0.42` = you are currently faster than your best lap),
- on every completed lap: the **lap time** full-screen for 4 s with the
  **delta vs your best lap** (`-0.32 vs best`),
- last lap, best lap, speed, lap count,
- it **logs every lap to a CSV file** in its internal flash (retrievable over USB),
- and it can **stream the GPS to the RaceChrono app** over Bluetooth LE
  ("RaceChrono DIY" protocol) for full session analysis on your phone.

The start line is set once, while riding, with a single button press — then it
is remembered for every future session. Real-world precision: ~0.1-0.3 s
(5 Hz GPS + crossing interpolation).

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
trace of the lap. Whenever a lap becomes your session best, its trace becomes
the reference. From then on, your progress is compared against the reference
*at the same distance along the lap*, five times per second — the bottom-left
of the RACE page shows `-0.42` (you're up) or `+0.87` (you're down), inverted
video when you're gaining. Available from lap 2 onwards.

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

---

## 4. Usage — quick reference

### Pages (short press = next page)

| Page | Content | Long press |
|---|---|---|
| **RACE** | current lap (big), speed, sats, live delta + Best (or Last/Best) | — |
| **SESSION** | recent laps (best marked `*`), session vmax | reset session |
| **GPS** | fix, satellites, HDOP, rate Hz, position | — |
| **LINE** | line status and distance | set the line here |

On the RACE top bar, `B` in front of the satellite count means a RaceChrono
client is connected.

While the timer is running, the display auto-returns to RACE after 15 s. Long
presses are disabled on RACE and GPS to prevent accidental actions while
riding.

### Serial commands (115200 baud)

| Command | Effect |
|---|---|
| `h` | help |
| `i` | info: line, session, all-time best, GPS state |
| `d` | dump the CSV log of every lap |
| `x` | erase the CSV log |
| `r` | reset the session |
| `z` | erase the all-time best |
| `L <lat> <lon> <hdg> [half-width]` | set the line manually (e.g. from Google Maps) |

### Main settings (`src/config.h`)

| Constant | Default | Role |
|---|---|---|
| `LINE_HALF_WIDTH_M` | 20 m | half-width of the detection gate |
| `MIN_LAP_MS` | 30 s | minimum lap time (double-detection guard) |
| `MIN_CROSS_SPEED_KMH` | 15 km/h | minimum speed for a valid crossing |
| `GPS_MEAS_RATE_MS` | 200 ms | GPS rate (5 Hz = NEO-6M maximum) |
| `ENABLE_BLE_RACECHRONO` | 1 | RaceChrono BLE streaming (0 = off, saves ~10-15 mA) |
| `BLE_DEVICE_NAME` | LapTimer ESP32 | name shown in the RaceChrono app |
| `PIN_*` | — | full pinout |

Display mounted upside down? Change `U8G2_R0` to `U8G2_R2` in `src/display.h`.

### RaceChrono over Bluetooth (optional)

The device advertises itself as a **RaceChrono DIY** GPS receiver. In the
[RaceChrono](https://racechrono.com/) app: **Settings → Add other device →
RaceChrono DIY → "LapTimer ESP32"**, then use it as the GPS receiver for your
sessions — you get lap analysis, sectors and traces on your phone, recorded in
parallel with the on-board timer. The phone can stay in your pocket or in the
pits (leave RaceChrono recording).

It also works in **simulation mode**: flash `esp32dev-sim` and RaceChrono
receives the virtual laps — handy to learn the app from your couch.

---

## 5. The code, briefly

```
src/
├── main.cpp      Orchestrator: main loop, buttons, serial commands,
│                 simulation mode
├── config.h      Every setting and the pinout
├── gps.cpp/.h    Drives the NEO-6M: automatic baud detection, UBX
│                 configuration (5 Hz, useless sentences disabled), clean
│                 "fix" snapshots via TinyGPS++
├── laptimer.cpp/.h  The algorithm: gate-crossing geometry, exact-time
│                 interpolation, laps/best/deltas, and the distance-matched
│                 predictive delta (lap traces + reference lap)
├── ble_racechrono.cpp/.h  "RaceChrono DIY" BLE device (NimBLE): streams the
│                 5 Hz GPS to the RaceChrono app
├── display.cpp/.h   The 4 OLED pages + end-of-lap flash (U8g2 library)
├── storage.cpp/.h   Persistence: line + best in NVS, CSV log in LittleFS
│                 (internal flash)
└── button.cpp/.h    Debouncing, short vs long press detection
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
- **Two build environments**: `esp32dev` (real) and `esp32dev-sim` (built-in
  virtual track — same timing code, generated positions — to test without a
  GPS).

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
| Not visible in RaceChrono | `ENABLE_BLE_RACECHRONO` set to 0, or another phone is already connected (one client at a time) |
| No delta on the RACE page | Normal on lap 1 — the delta needs a completed reference lap (from lap 2) |

---

## 7. Possible upgrades

Already built in: live predictive delta, RaceChrono BLE streaming. Still on
the table (need hardware):

- u-blox **M8N/M10** GPS (10-25 Hz, multi-constellation): drop-in replacement,
  same wiring, same code → noticeably better precision (~200-400 ฿).
- **2.42" SSD1309 OLED**: same library, a one-line change.
- Battery gauge (2×100 kΩ voltage divider into an ADC pin).
- Persist the reference lap across power cycles (predictive delta available
  from lap 1 of the next session).

---

## Safety

Mount the device firmly (anything that comes loose at 150 km/h is a
projectile), never fiddle with it while riding — everything is automatic once
the line is set — and check the track's rules about on-board devices.

*A ~600 ฿ DIY project, no warranty of any kind — have fun and stay safe.* 🏁

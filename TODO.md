# TODO — roadmap & shopping list

The plan, in one page: what is ordered, what still needs to be bought and
why, and the next projects (A → E) with the exact purchases each one needs.

---

## Already ordered ✅

| Part | Why it's needed | Status |
|---|---|---|
| ESP32 DevKit (CH340, USB-C) | the brain of everything | arriving ~22-23 Jul |
| GY-NEO-6M GPS | position/speed/time, 5 Hz — the v1 timing source | arriving ~19 Jul |
| 0.96" SSD1306 OLED | the display | arriving ~17-18 Jul |
| TP4056 charger | recharges the future LiPo like a phone | arriving |
| ABS enclosure, Dupont wires, USB-C cable | integration | arriving |
| **GY-521 MPU6050** (IMU) | lean angle + G-forces (project E below) | ordered 16 Jul |
| **GY-NEO-M8N GPS** | 10 Hz upgrade → ~0.05-0.1 s lap precision, plug-in replacement | ordered 16 Jul |

## Still to buy — to finish the v1 build

| Part | ~Price | Why |
|---|---|---|
| LiPo 3.7 V ≥ 1000 mAh | 100-200 ฿ | powers the device on the bike (6-8 h). Until then: power bank on USB-C |
| MT3608 boost converter | 15-30 ฿ | stable 5 V from the LiPo — without it the ESP32+GPS brown out |
| Slide switch | 10-20 ฿ | on/off without unplugging |
| Waterproof 12 mm push button | 20-40 ฿ | glove-friendly control (BOOT button works meanwhile) |
| Female-female Dupont wires | ~20 ฿ | module-to-module wiring (the ordered pack is male-female) |
| Quad Lock universal adapter | varies | mounts the box on the tank |
| Soldering iron + solder | 200-400 ฿ | GPS/OLED header pins ship loose, battery wires |
| Multimeter | 150-300 ฿ | **mandatory**: set the MT3608 to 5 V before connecting anything |
| Resistor assortment kit (1/4 W) | 50-100 ฿ | 510 Ω + 10 kΩ for project A, 2×100 kΩ for the battery gauge |

---

# Next projects

## A. ECU bridge — Kawasaki KDS ✅ *firmware done (v1.4), waiting for parts*

Live **RPM, throttle, gear, coolant temp** from the bike's diagnostic plug:
shown on the ECU page and streamed to RaceChrono as extra channels
(*"in turn 3 you only opened 60% throttle"*). See README Step 7 for the
bike verification procedure (K-line vs CAN) and the wiring.

**To buy:**

| Part | ~Price | Note |
|---|---|---|
| L9637D transceiver ×2 | 30-60 ฿ | the K-line interface chip (one spare — wiring mistakes kill them) |
| Kawasaki 4-pin diagnostic connector | 50-150 ฿ | search `Kawasaki diagnostic connector 4 pin` |
| Resistor kit | — | already in the build list above (510 Ω + 10 kΩ) |
| Prototype PCB board *(optional)* | ~20 ฿ | clean soldered assembly |

## B. Tire temperature (infrared) ✅ *firmware done (v1.6), parts to order*

Two contactless IR sensors aimed at the tires (front fender + tail): live
TIRES page, per-lap average temperatures in the CSV, RaceChrono channels
(PID 0x102). Re-address one sensor with the `M` serial command, wire both on
the dedicated tire I2C bus (GPIO 32/33). See README Step 9.

**To buy:**

| Part | ~Price | Note |
|---|---|---|
| MLX90614 IR sensor ×2 | 150-250 ฿ each | I2C, same bus as the OLED. Search `MLX90614` |
| Wire + heat shrink | ~30 ฿ | runs to the fender and the tail |

## C. Video overlay generator 💻 *desktop tool — no purchase, can start anytime*

A tool on the Mac that merges track videos (GoPro/phone) with the lap timer
data: TV-style overlay with the running lap time, live delta, speed, and —
once project E is done — lean angle. Uses the CSV and traces the device
already records. **Nothing to buy.**

## D. Live pit telemetry (LoRa) ✅ *firmware done (v1.7), parts to order*

The bike broadcasts once per second (1-2 km range); the pit box (second
ESP32 + OLED + LoRa, firmware `pio run -e pitbox`) shows the live delta in
big, laps, speed, lean and temps to a friend or coach. See README Step 10.
⚠️ Never power a LoRa module without its antenna.

**To buy:**

| Part | ~Price | Note |
|---|---|---|
| LoRa module (SX1276/SX1278 433 MHz) ×2 | 150-300 ฿ each | one on the bike, one in the pits. Search `SX1278 LoRa 433` |
| Second ESP32 DevKit | ~160 ฿ | the pit-side receiver |
| Second 0.96" OLED (or bigger) | 85-250 ฿ | the pit display |
| Any power bank | — | powers the pit box |

## E. Lean angle & G-forces (MPU6050) ✅ *firmware done (v1.5), sensor ordered*

Live lean angle (L/R) with per-lap max on both sides, braking G peak — LEAN
page, CSV column, RaceChrono channels (PID 0x101). Calibration with the `g`
serial command, bike upright. See README Step 8.

**To buy: nothing — the sensor is on its way.**

### Bonus idea (no letter yet): GoPro auto-record
The ESP32 can drive a GoPro over BLE (Open GoPro API): recording starts above
30 km/h, stops back in the pits. No purchase if you own a GoPro.

---

## Suggested order of battle

1. Parts arrive → assemble the v1 (README steps 0-6).
2. **A**, **E**, **B** and **D** as soon as the parts arrive — all four
   firmwares are ready, it's wiring + calibration only (README steps 7-10).
3. **C** (video overlay) once the camera is bought — the only project left
   to code, and it's a desktop tool.

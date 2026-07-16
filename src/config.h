#pragma once
#include <stdint.h>  // no Arduino dependency: also included by host-side unit tests

// ============================ Pins ============================
constexpr int PIN_GPS_RX   = 16;  // ESP32 RX2  <- GPS TX pin
constexpr int PIN_GPS_TX   = 17;  // ESP32 TX2  -> GPS RX pin
constexpr int PIN_I2C_SDA  = 21;  // OLED SDA
constexpr int PIN_I2C_SCL  = 22;  // OLED SCL
constexpr int PIN_BTN_BOOT = 0;   // on-board BOOT button
constexpr int PIN_BTN_EXT  = 25;  // optional external button (to GND), -1 to disable

// ============================ GPS ============================
constexpr uint32_t GPS_TARGET_BAUD    = 57600;  // baud rate after configuration
constexpr uint16_t GPS_MEAS_RATE_MS   = 200;    // measurement period: 200 ms = 5 Hz
constexpr uint32_t GPS_FIX_MAX_GAP_MS = 1500;   // beyond this: reception gap, no interpolation

// ================= Bluetooth -> phone apps =================
// BT_MODE_RACECHRONO : BLE "RaceChrono DIY" device -> RaceChrono app
//                      (iOS + Android). Lightest on the battery.
// BT_MODE_NMEA       : Bluetooth Classic, generic NMEA GPS -> TrackAddict,
//                      Harry's LapTimer, mock-location apps (Android only;
//                      iOS locks Bluetooth GPS to MFi-certified hardware).
// BT_MODE_OFF        : no Bluetooth at all, lowest power draw.
// The two Bluetooth stacks cannot coexist, so this is a build-time choice:
// the default build is RACECHRONO, `pio run -e esp32dev-nmea` builds NMEA.
#define BT_MODE_OFF        0
#define BT_MODE_RACECHRONO 1
#define BT_MODE_NMEA       2
#ifndef BT_MODE
#define BT_MODE BT_MODE_RACECHRONO
#endif
constexpr char BT_DEVICE_NAME[] = "LapTimer ESP32";

// ===================== Lap detection =====================
constexpr float    LINE_HALF_WIDTH_M     = 20.0f;   // half-width of the start/finish gate
constexpr uint32_t MIN_LAP_MS            = 30000;   // minimum lap time (debounce)
constexpr float    MIN_CROSS_SPEED_KMH   = 15.0f;   // minimum speed for a valid crossing
constexpr float    MIN_SETLINE_SPEED_KMH = 10.0f;   // minimum speed to set the line (reliable GPS course)
constexpr float    MAX_FIX_JUMP_M        = 250.0f;  // absurd position jump -> ignored
constexpr int      MAX_LAPS              = 120;     // laps stored per session
constexpr int      TRACE_MAX_SAMPLES     = 1500;    // predictive-delta reference buffer (5 min @ 5 Hz)
constexpr int      NUM_SECTORS           = 3;       // automatic sectors (split by distance)
constexpr uint32_t SESSION_GAP_MS        = 300000;  // crossing after 5 min of no lap = new session

// ===================== Tracks =====================
constexpr int   MAX_TRACKS     = 20;     // named tracks stored in flash
constexpr float TRACK_MATCH_KM = 5.0f;   // auto-select the stored track within this range at startup

// ================= Pit mode (WiFi) =================
// Long press on the GPS page: reboots into a WiFi hotspot with a web page to
// download the lap log, manage tracks and flash firmware updates (OTA).
constexpr char PIT_WIFI_PASS[] = "laptimer";  // WPA2 password of the hotspot (8+ chars)

// ============ ECU bridge (Kawasaki KDS, K-line) ============
// Reads RPM, throttle, gear, coolant temp and speed from the bike's
// diagnostic plug through an L9637D transceiver, shows them on the ECU page
// and streams them to RaceChrono as CAN channels. Harmless when nothing is
// wired (retries quietly); set to 0 to remove entirely.
#define ENABLE_KDS 1
constexpr int      PIN_KDS_RX          = 26;    // <- L9637D RX (10k pull-up to 3V3)
constexpr int      PIN_KDS_TX          = 27;    // -> L9637D TX
constexpr uint32_t KDS_BAUD            = 10400;
constexpr uint32_t KDS_INTERBYTE_MS    = 10;    // pacing between request bytes
constexpr uint32_t KDS_POLL_GAP_MS     = 40;    // gap between requests
constexpr uint32_t KDS_TIMEOUT_MS      = 300;   // response timeout
constexpr uint32_t KDS_START_TIMEOUT_MS = 600;  // init response timeout
constexpr uint32_t KDS_RETRY_MS        = 5000;  // re-init backoff
constexpr uint32_t ECU_CAN_ID          = 0x100; // fabricated CAN id for RaceChrono
constexpr uint32_t ECU_CAN_PERIOD_MS   = 250;   // stream rate to RaceChrono

// ============ IMU (MPU6050): lean angle & G-forces ============
// GY-521 board on the same I2C bus as the OLED, mounted flat, X axis facing
// forward. Harmless when absent (probes quietly); set to 0 to remove.
#define ENABLE_IMU 1
constexpr uint8_t  IMU_I2C_ADDR          = 0x68;
constexpr float    IMU_KIN_TAU_S         = 3.0f;   // kinematic correction time constant
constexpr float    IMU_KIN_MIN_SPEED_KMH = 30.0f;  // below: correct with gravity instead
constexpr uint32_t IMU_PROBE_MS          = 5000;   // re-probe period when absent
constexpr uint32_t IMU_CAN_ID            = 0x101;  // fabricated CAN id for RaceChrono

// ============ Tire temperature (2x MLX90614 IR) ============
// Both sensors live on the ESP32's second I2C bus so the long cable runs
// (front fender, tail) never disturb the display bus. The two sensors ship
// at the same address: re-address the REAR one to 0x5B with the 'M' serial
// command (one sensor connected alone). Harmless when absent.
#define ENABLE_TIRES 1
constexpr int      PIN_TIRE_SDA    = 32;
constexpr int      PIN_TIRE_SCL    = 33;
constexpr uint8_t  TIRE_FRONT_ADDR = 0x5A;   // factory default
constexpr uint8_t  TIRE_REAR_ADDR  = 0x5B;   // re-addressed
constexpr uint32_t TIRE_SAMPLE_MS  = 250;    // alternating front/rear
constexpr uint32_t TIRE_PROBE_MS   = 5000;   // re-probe period when absent
constexpr uint32_t TIRE_CAN_ID     = 0x102;  // fabricated CAN id for RaceChrono

// ============ LoRa pit telemetry (SX1276/SX1278) ============
// The bike broadcasts a compact telemetry packet once per second; a second
// ESP32 + OLED + LoRa in the pits (firmware: pio run -e pitbox) shows the
// live lap time, delta and alerts to a friend/coach. Harmless when absent.
// NEVER power the radio without its antenna screwed on (kills the PA).
#define ENABLE_LORA 1
constexpr long     LORA_FREQ_HZ   = 433000000L;  // check local regulations
constexpr int      PIN_LORA_CS    = 5;
constexpr int      PIN_LORA_RST   = 4;
constexpr int      PIN_LORA_DIO0  = 34;  // input-only pin is fine (IRQ line)
constexpr uint8_t  LORA_SYNC_WORD = 0x4C;        // private network id
constexpr int      LORA_TX_DBM    = 14;
constexpr uint32_t LORA_PERIOD_MS = 1000;
constexpr uint32_t LORA_RETRY_MS  = 10000;       // re-probe when module absent
constexpr uint32_t PIT_STALE_MS   = 5000;        // pit display: data too old

// ================= Screen dimming =================
constexpr uint32_t DIM_AFTER_MS  = 60000;  // dim after 1 min without movement or button
constexpr float    DIM_SPEED_KMH = 5.0f;   // "movement" threshold

// ========================= Interface =========================
constexpr uint32_t LAP_FLASH_MS      = 4000;  // how long the lap time stays on screen after a lap
constexpr uint32_t LONG_PRESS_MS     = 1000;
constexpr uint32_t DEBOUNCE_MS       = 40;
constexpr uint32_t DISPLAY_PERIOD_MS = 100;   // display refresh (10 Hz)
constexpr uint32_t PAGE_TIMEOUT_MS   = 15000; // auto-return to RACE page while timing

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t MS_PER_DAY  = 86400000UL;

#pragma once
#include <Arduino.h>

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

// ================= Bluetooth (RaceChrono) =================
// 1 = stream the GPS over BLE with the "RaceChrono DIY" protocol (the device
// shows up in the RaceChrono app as a GPS receiver). 0 = disable, saves
// ~10-15 mA of battery.
#define ENABLE_BLE_RACECHRONO 1
constexpr char BLE_DEVICE_NAME[] = "LapTimer ESP32";

// ===================== Lap detection =====================
constexpr float    LINE_HALF_WIDTH_M     = 20.0f;   // half-width of the start/finish gate
constexpr uint32_t MIN_LAP_MS            = 30000;   // minimum lap time (debounce)
constexpr float    MIN_CROSS_SPEED_KMH   = 15.0f;   // minimum speed for a valid crossing
constexpr float    MIN_SETLINE_SPEED_KMH = 10.0f;   // minimum speed to set the line (reliable GPS course)
constexpr float    MAX_FIX_JUMP_M        = 250.0f;  // absurd position jump -> ignored
constexpr int      MAX_LAPS              = 120;     // laps stored per session
constexpr int      TRACE_MAX_SAMPLES     = 1500;    // predictive-delta reference buffer (5 min @ 5 Hz)

// ========================= Interface =========================
constexpr uint32_t LAP_FLASH_MS      = 4000;  // how long the lap time stays on screen after a lap
constexpr uint32_t LONG_PRESS_MS     = 1000;
constexpr uint32_t DEBOUNCE_MS       = 40;
constexpr uint32_t DISPLAY_PERIOD_MS = 100;   // display refresh (10 Hz)
constexpr uint32_t PAGE_TIMEOUT_MS   = 15000; // auto-return to RACE page while timing

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t MS_PER_DAY  = 86400000UL;

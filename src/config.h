#pragma once
#include <Arduino.h>

// ============================ Broches ============================
constexpr int PIN_GPS_RX   = 16;  // RX2 de l'ESP32  <- broche TX du GPS
constexpr int PIN_GPS_TX   = 17;  // TX2 de l'ESP32  -> broche RX du GPS
constexpr int PIN_I2C_SDA  = 21;  // OLED SDA
constexpr int PIN_I2C_SCL  = 22;  // OLED SCL
constexpr int PIN_BTN_BOOT = 0;   // bouton BOOT integre a la carte
constexpr int PIN_BTN_EXT  = 25;  // bouton externe optionnel (vers GND), -1 pour desactiver

// ============================ GPS ============================
constexpr uint32_t GPS_TARGET_BAUD    = 57600;  // baud vise apres configuration
constexpr uint16_t GPS_MEAS_RATE_MS   = 200;    // periode de mesure : 200 ms = 5 Hz
constexpr uint32_t GPS_FIX_MAX_GAP_MS = 1500;   // au-dela : trou de reception, pas d'interpolation

// ===================== Detection de tour =====================
constexpr float    LINE_HALF_WIDTH_M     = 20.0f;   // demi-largeur de la porte depart/arrivee
constexpr uint32_t MIN_LAP_MS            = 30000;   // temps au tour minimum (anti-rebond)
constexpr float    MIN_CROSS_SPEED_KMH   = 15.0f;   // vitesse mini pour valider un franchissement
constexpr float    MIN_SETLINE_SPEED_KMH = 10.0f;   // vitesse mini pour definir la ligne (cap GPS fiable)
constexpr float    MAX_FIX_JUMP_M        = 250.0f;  // saut de position aberrant -> ignore
constexpr int      MAX_LAPS              = 120;     // tours memorises par session

// ========================= Interface =========================
constexpr uint32_t LAP_FLASH_MS      = 4000;  // duree d'affichage du temps en fin de tour
constexpr uint32_t LONG_PRESS_MS     = 1000;
constexpr uint32_t DEBOUNCE_MS       = 40;
constexpr uint32_t DISPLAY_PERIOD_MS = 100;   // rafraichissement ecran (10 Hz)
constexpr uint32_t PAGE_TIMEOUT_MS   = 15000; // retour auto page RACE quand le chrono tourne

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr uint32_t MS_PER_DAY  = 86400000UL;

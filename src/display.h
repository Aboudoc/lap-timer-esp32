#pragma once
#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"
#include "laptimer.h"

enum class Page : uint8_t { Race, Session, Gps, Line, COUNT };

// "Flat" view of the GPS state, filled by main.cpp (real GPS or simulation).
struct GpsView {
  bool     hasFix = false;
  int      sats = 0;
  float    hdop = 99.9f;
  float    rateHz = 0;
  uint32_t baud = 0;
  double   lat = 0, lon = 0;
  float    speedKmh = 0;
  uint32_t fixAgeMs = 0;
  bool     sim = false;
  bool     ble = false;  // a RaceChrono client is connected
};

class Display {
 public:
  void begin();
  void splash(const char* version);
  void render(Page page, const LapTimer& t, const GpsView& g, uint32_t now, uint32_t bestEverMs);
  void notify(const char* msg, uint32_t durationMs = 1800);

 private:
  void drawCentered(const char* s, int y);
  void drawRace(const LapTimer& t, const GpsView& g, uint32_t now);
  void drawLapFlash(const LapTimer& t, uint32_t bestEverMs);
  void drawSession(const LapTimer& t);
  void drawGps(const GpsView& g);
  void drawLinePage(const LapTimer& t, const GpsView& g);
  void drawNotify();

  U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_{U8G2_R0, U8X8_PIN_NONE, PIN_I2C_SCL, PIN_I2C_SDA};
  char     notif_[24] = {0};
  uint32_t notifUntil_ = 0;
};

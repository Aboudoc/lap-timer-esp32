#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"
#include "fix.h"

// Drives the NEO-6M: baud rate detection, UBX configuration (5 Hz, RMC+GGA
// only, fast baud), then produces clean GpsFix snapshots.
class GpsModule {
 public:
  void begin(HardwareSerial& serial);
  bool update();               // call in the main loop; true when a new valid fix is available
  bool processChar(char c);    // feed one NMEA char directly (replay mode)
  const GpsFix& fix() const { return fix_; }

  bool     hasFix();
  int      sats();
  float    hdop();
  float    lastSpeedKmh();
  uint32_t fixAgeMs();
  void     dateStr(char* out, size_t n);  // "2026-07-20" (UTC)
  bool     dateYmd(int& y, int& m, int& d);
  uint32_t baud() const { return baud_; }
  float    measuredRateHz() const { return rateHz_; }

 private:
  void     configureModule();
  uint32_t detectBaud();
  bool     waitForNmea(uint32_t timeoutMs);
  void     sendUbx(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len);
  void     setNmeaRate(uint8_t msgId, uint8_t rate);

  HardwareSerial* serial_ = nullptr;
  TinyGPSPlus     gps_;
  GpsFix          fix_;
  uint32_t        baud_ = 0;
  uint32_t        lastEmitMsOfDay_ = 0xFFFFFFFF;
  uint32_t        lastFixLocalMs_ = 0;
  float           rateHz_ = 0;
};

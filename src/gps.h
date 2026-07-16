#pragma once
#include <Arduino.h>
#include <TinyGPS++.h>
#include "config.h"

// Instantane d'une position GPS, emis une fois par epoque de mesure.
struct GpsFix {
  double   lat = 0, lon = 0;
  float    speedKmh  = 0;
  float    courseDeg = 0;      // cap sol (0 = nord, 90 = est)
  uint32_t msOfDay   = 0;      // heure GPS (UTC) en ms depuis minuit
  uint32_t localMs   = 0;      // millis() a la reception
  bool     valid     = false;
};

// Pilote le NEO-6M : detection du baud, configuration UBX (5 Hz, RMC+GGA
// uniquement, baud rapide), puis production de GpsFix propres.
class GpsModule {
 public:
  void begin(HardwareSerial& serial);
  bool update();  // a appeler en boucle ; true si un nouveau fix valide est disponible
  const GpsFix& fix() const { return fix_; }

  bool     hasFix();
  int      sats();
  float    hdop();
  float    lastSpeedKmh();
  uint32_t fixAgeMs();
  void     dateStr(char* out, size_t n);  // "2026-07-20" (UTC)
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

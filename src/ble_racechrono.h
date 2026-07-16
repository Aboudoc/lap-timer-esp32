#pragma once
#include <Arduino.h>
#include "config.h"
#include "gps.h"

#if BT_MODE == BT_MODE_RACECHRONO

class NimBLECharacteristic;

// Exposes the GPS as a "RaceChrono DIY" BLE device (service 0x1ff8), so the
// RaceChrono app can use it as an external 5 Hz receiver and record full
// session analytics. Protocol: github.com/aollin/racechrono-ble-diy-device
class BleRaceChrono {
 public:
  void begin(const char* deviceName);
  // Push one fix to the app. year/month/day from the GPS (year < 2000 =
  // unknown date -> the time characteristic is skipped).
  void onFix(const GpsFix& fix, int sats, float hdop, bool hasFix,
             int year, int month, int day);
  bool connected() const { return connected_; }
  void setConnected(bool c) { connected_ = c; }  // used by the server callbacks

 private:
  void buildMainPacket(uint8_t out[20], const GpsFix& fix, int sats, float hdop,
                       bool hasFix) const;

  NimBLECharacteristic* mainChar_ = nullptr;
  NimBLECharacteristic* timeChar_ = nullptr;
  uint8_t       syncBits_ = 0;
  uint32_t      lastHourDate_ = 0xFFFFFFFF;
  volatile bool connected_ = false;
};

#endif  // BT_MODE == BT_MODE_RACECHRONO

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
  // Push a fabricated CAN frame (ECU channels). RaceChrono maps the payload
  // bytes to channels with user-defined formulas.
  void sendCan(uint32_t id, const uint8_t* data, uint8_t len);
  bool connected() const { return connected_; }

  // Used by the BLE callbacks.
  void setConnected(bool c) { connected_ = c; }
  void onFilterWrite(const uint8_t* data, size_t len);

 private:
  void buildMainPacket(uint8_t out[20], const GpsFix& fix, int sats, float hdop,
                       bool hasFix) const;
  bool canAllowed(uint32_t id) const;

  NimBLECharacteristic* mainChar_ = nullptr;
  NimBLECharacteristic* timeChar_ = nullptr;
  NimBLECharacteristic* canChar_ = nullptr;
  uint8_t       syncBits_ = 0;
  uint32_t      lastHourDate_ = 0xFFFFFFFF;
  volatile bool connected_ = false;

  // CAN filter state (written by the app: deny all / allow all / add PID).
  bool     canAllowAll_ = true;
  uint32_t canAllowed_[8] = {0};
  uint8_t  canAllowedCount_ = 0;
};

#endif  // BT_MODE == BT_MODE_RACECHRONO

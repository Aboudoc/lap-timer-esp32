#pragma once
#include <Arduino.h>
#include "config.h"
#include "gps.h"

#if BT_MODE == BT_MODE_NMEA

#include <BluetoothSerial.h>

// Exposes the GPS as a generic Bluetooth Classic NMEA receiver (SPP serial
// port). Works with any Android app that accepts an external Bluetooth GPS:
// TrackAddict, Harry's LapTimer, "Bluetooth GPS"-style mock-location apps...
// (iOS restricts Bluetooth GPS to MFi-certified hardware — out of DIY reach.)
class BtNmea {
 public:
  void begin(const char* deviceName);
  void onFix(const GpsFix& fix, int sats, float hdop, bool hasFix,
             int year, int month, int day);
  bool connected() { return bt_.hasClient(); }

 private:
  BluetoothSerial bt_;
};

#endif  // BT_MODE == BT_MODE_NMEA

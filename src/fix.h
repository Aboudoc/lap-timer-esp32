#pragma once
#include <stdint.h>

// Snapshot of a GPS position, emitted once per measurement epoch.
// Plain data, no Arduino dependency: the timing core and the host-side unit
// tests both build against this.
struct GpsFix {
  double   lat = 0, lon = 0;
  float    speedKmh  = 0;
  float    courseDeg = 0;   // course over ground (0 = north, 90 = east)
  float    altM      = 0;   // altitude above sea level, meters
  bool     altValid  = false;
  uint32_t msOfDay   = 0;   // GPS time (UTC) in ms since midnight
  uint32_t localMs   = 0;   // millis() when received
  bool     valid     = false;
};

#include "bt_nmea.h"

#if BT_MODE == BT_MODE_NMEA

#include <math.h>

namespace {

// XOR of every character between '$' and '*'.
uint8_t nmeaChecksum(const char* s) {
  uint8_t cs = 0;
  while (*s) cs ^= (uint8_t)(*s++);
  return cs;
}

// NMEA coordinate: "ddmm.mmmmm,N" (latitude) / "dddmm.mmmmm,E" (longitude).
// Integer arithmetic with explicit bounds: exact rounding, no truncation.
void fmtCoord(char* out, size_t n, double deg, bool isLat) {
  char hemi = isLat ? (deg >= 0 ? 'N' : 'S') : (deg >= 0 ? 'E' : 'W');
  double a = fabs(deg);
  int d = (int)a;
  long frac = lround((a - (double)d) * 60.0 * 100000.0);  // minutes in 1e-5 units
  if (frac >= 6000000L) {  // rounding pushed the minutes to 60 -> carry
    frac -= 6000000L;
    d++;
  }
  unsigned dd = (unsigned)d % 1000u;
  unsigned long mi = ((unsigned long)frac / 100000UL) % 100UL;
  unsigned long mf = (unsigned long)frac % 100000UL;
  if (isLat) {
    snprintf(out, n, "%02u%02lu.%05lu,%c", dd, mi, mf, hemi);
  } else {
    snprintf(out, n, "%03u%02lu.%05lu,%c", dd, mi, mf, hemi);
  }
}

}  // namespace

void BtNmea::begin(const char* deviceName) {
  if (!bt_.begin(deviceName)) {
    Serial.println("[BT] Bluetooth Classic init failed");
    return;
  }
  Serial.printf("[BT] discoverable as \"%s\" (generic NMEA GPS, pair from Android)\n",
                deviceName);
}

void BtNmea::onFix(const GpsFix& fix, int sats, float hdop, bool hasFix,
                   int year, int month, int day) {
  if (!bt_.hasClient()) return;
  if (!hasFix || !fix.valid) return;

  char tim[16], lat[20], lon[20], body[128], line[144];
  uint32_t s = fix.msOfDay / 1000UL;
  snprintf(tim, sizeof(tim), "%02lu%02lu%02lu.%02lu",
           (unsigned long)(s / 3600UL), (unsigned long)((s / 60UL) % 60UL),
           (unsigned long)(s % 60UL),
           (unsigned long)((fix.msOfDay % 1000UL) / 10UL));
  fmtCoord(lat, sizeof(lat), fix.lat, true);
  fmtCoord(lon, sizeof(lon), fix.lon, false);

  // GGA: fix quality, satellites, HDOP, altitude.
  snprintf(body, sizeof(body), "GPGGA,%s,%s,%s,1,%02d,%.1f,%.1f,M,,M,,",
           tim, lat, lon, sats, hdop, fix.altValid ? fix.altM : 0.0f);
  snprintf(line, sizeof(line), "$%s*%02X\r\n", body, nmeaChecksum(body));
  bt_.write((const uint8_t*)line, strlen(line));

  // RMC: position, speed (knots), course, date.
  if (year >= 2000) {
    snprintf(body, sizeof(body), "GPRMC,%s,A,%s,%s,%.2f,%.2f,%02d%02d%02d,,,A",
             tim, lat, lon, fix.speedKmh / 1.852f, fix.courseDeg,
             day, month, year % 100);
    snprintf(line, sizeof(line), "$%s*%02X\r\n", body, nmeaChecksum(body));
    bt_.write((const uint8_t*)line, strlen(line));
  }
}

#endif  // BT_MODE == BT_MODE_NMEA

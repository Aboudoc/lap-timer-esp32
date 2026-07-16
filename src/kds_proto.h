#pragma once
#include <stdint.h>
#include <stddef.h>

// Kawasaki KDS = KWP2000 (ISO 14230) over K-line, 10400 baud.
// Pure logic (framing, parsing, unit conversions): no Arduino dependency,
// covered by the host-side unit tests. Protocol details and register
// formulas come from the Kawaduino / KDS2Bluetooth community work.

// Live engine data, filled by KdsBus (or faked in simulation mode).
struct EcuData {
  int      rpm = 0;
  int      gear = -1;         // -1 unknown, 0 = neutral
  float    throttlePct = -1;  // -1 unknown
  float    coolantC = -1000;
  float    speedKmh = -1;
  bool     link = false;
  uint32_t updatedMs = 0;
};

namespace kds {

constexpr uint8_t ECU_ADDR       = 0x11;
constexpr uint8_t TESTER_ADDR    = 0xF2;
constexpr uint8_t SID_START_COMM = 0x81;  // positive response: 0xC1
constexpr uint8_t SID_READ_REG   = 0x21;  // positive response: 0x61 <reg> <data>

constexpr uint8_t REG_THROTTLE = 0x04;
constexpr uint8_t REG_COOLANT  = 0x06;
constexpr uint8_t REG_RPM      = 0x09;
constexpr uint8_t REG_GEAR     = 0x0B;
constexpr uint8_t REG_SPEED    = 0x0C;

// Builds a tester->ECU frame: fmt(0x80|len) tgt src payload checksum.
// Returns the frame length, 0 on error.
inline size_t buildFrame(const uint8_t* payload, uint8_t len, uint8_t* out, size_t outMax) {
  if (len == 0 || len > 63 || outMax < (size_t)len + 4) return 0;
  out[0] = (uint8_t)(0x80 | len);
  out[1] = ECU_ADDR;
  out[2] = TESTER_ADDR;
  for (uint8_t i = 0; i < len; i++) out[3 + i] = payload[i];
  uint8_t cs = 0;
  for (size_t i = 0; i < (size_t)len + 3; i++) cs = (uint8_t)(cs + out[i]);
  out[len + 3] = cs;
  return (size_t)len + 4;
}

// Incremental parser for ECU responses (same framing, addresses swapped).
struct Parser {
  uint8_t buf[72];
  uint8_t n = 0;
};

inline void reset(Parser& p) { p.n = 0; }

// Feed one byte. Returns 1 when a valid frame is complete, -1 on a framing
// or checksum error (caller resyncs), 0 while incomplete.
inline int feed(Parser& p, uint8_t b) {
  if (p.n == 0 && (b & 0xC0) != 0x80) return -1;  // not a format byte
  if (p.n >= sizeof(p.buf)) {
    p.n = 0;
    return -1;
  }
  p.buf[p.n++] = b;
  uint8_t len = p.buf[0] & 0x3F;
  if (len == 0) {
    p.n = 0;
    return -1;
  }
  uint8_t total = (uint8_t)(len + 4);
  if (p.n < total) return 0;
  uint8_t cs = 0;
  for (uint8_t i = 0; i + 1 < total; i++) cs = (uint8_t)(cs + p.buf[i]);
  if (cs != p.buf[total - 1]) {
    p.n = 0;
    return -1;
  }
  return 1;
}

inline uint8_t payloadLen(const Parser& p) { return p.buf[0] & 0x3F; }
inline const uint8_t* payload(const Parser& p) { return p.buf + 3; }

// ---- Register value conversions ----
inline int rpmFrom(uint8_t a, uint8_t b) { return (int)a * 100 + b; }
inline int gearFrom(uint8_t a) { return a; }  // 0 = neutral
inline float speedFrom(uint8_t a, uint8_t b) { return (float)(((int)a << 8) + b) / 2.0f; }
inline float coolantFrom(uint8_t a) { return ((float)a - 48.0f) / 1.6f; }

// Raw throttle range as measured on Kawasaki twins (closed ~210, WOT ~894).
// If the % looks off on the Ninja 400, log the raw values and adjust here.
constexpr int THROTTLE_RAW_CLOSED = 210;
constexpr int THROTTLE_RAW_WOT    = 894;
inline float throttleFrom(uint8_t a, uint8_t b) {
  int raw = ((int)a << 8) + b;
  float pct = (float)(raw - THROTTLE_RAW_CLOSED) * 100.0f /
              (float)(THROTTLE_RAW_WOT - THROTTLE_RAW_CLOSED);
  if (pct < 0.0f) pct = 0.0f;
  if (pct > 100.0f) pct = 100.0f;
  return pct;
}

}  // namespace kds

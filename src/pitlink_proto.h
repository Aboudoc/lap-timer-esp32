#pragma once
#include <stdint.h>
#include <string.h>
#include <stddef.h>

// Radio packet shared by the bike (transmitter) and the pit box (receiver).
// Pure data, unit-tested; both ends are ESP32s so the packed layout matches.

constexpr uint32_t PIT_MAGIC = 0x3154504CUL;  // "LPT1"

// Flag bits.
constexpr uint8_t PIT_F_TIMING   = 0x01;  // clock running
constexpr uint8_t PIT_F_HASPRED  = 0x02;  // predictive delta valid
constexpr uint8_t PIT_F_LAPFLASH = 0x04;  // a lap was just completed

struct PitPacket {
  uint32_t magic = PIT_MAGIC;
  uint8_t  seq = 0;
  uint8_t  session = 0;
  uint16_t lapCount = 0;
  uint32_t currentLapMs = 0;
  uint32_t lastLapMs = 0;
  uint32_t bestLapMs = 0;
  int32_t  predDeltaMs = 0;
  uint8_t  flags = 0;
  uint8_t  speedKmh = 0;
  int8_t   leanDeg = 0;     // + = right
  uint8_t  tireF = 255;     // deg C + 40, 255 = n/a
  uint8_t  tireR = 255;
  uint8_t  coolant = 255;   // deg C + 40, 255 = n/a
  uint16_t rpm = 0;
  char     track[10] = {0};
} __attribute__((packed));

inline size_t pitEncode(const PitPacket& p, uint8_t* out) {
  memcpy(out, &p, sizeof(PitPacket));
  return sizeof(PitPacket);
}

inline bool pitDecode(const uint8_t* in, size_t n, PitPacket& out) {
  if (n != sizeof(PitPacket)) return false;
  memcpy(&out, in, sizeof(PitPacket));
  return out.magic == PIT_MAGIC;
}

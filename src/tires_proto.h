#pragma once
#include <stdint.h>

// MLX90614 IR thermometer helpers (SMBus PEC + temperature conversion).
// Pure logic, covered by the host-side unit tests.

struct TireData {
  float    frontC = -1000, rearC = -1000;  // object (tread) temperature
  bool     frontOk = false, rearOk = false;
  uint32_t updatedMs = 0;
};

namespace mlx {

// SMBus PEC: CRC-8, polynomial 0x07, init 0.
inline uint8_t crc8(const uint8_t* data, uint8_t len) {
  uint8_t crc = 0;
  for (uint8_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (uint8_t b = 0; b < 8; b++) {
      crc = (uint8_t)((crc & 0x80) ? (crc << 1) ^ 0x07 : (crc << 1));
    }
  }
  return crc;
}

// Raw RAM word -> degrees Celsius. The MSB high bit flags an error.
inline bool tempFromRaw(uint16_t raw, float& outC) {
  if (raw & 0x8000) return false;
  outC = (float)raw * 0.02f - 273.15f;
  return true;
}

}  // namespace mlx

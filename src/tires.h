#pragma once
#include <Arduino.h>
#include "config.h"
#include "tires_proto.h"

#if ENABLE_TIRES

// Two MLX90614 IR sensors (front fender + tail) on the second I2C bus.
// Alternates front/rear reads, verifies the SMBus PEC, probes quietly.
class Tires {
 public:
  void begin();
  void loop();
  const TireData& data() const { return data_; }

  // Writes a new SMBus address into the EEPROM of the ONE sensor connected
  // to the bus (uses the broadcast address). Power-cycle afterwards.
  bool readdress(uint8_t newAddr);

 private:
  bool readObjectC(uint8_t addr, float& outC);
  bool writeEepromWord(uint8_t cmd, uint16_t value);

  TireData data_;
  bool     frontSeen_ = false, rearSeen_ = false;
  uint32_t lastSampleMs_ = 0;
  uint32_t lastProbeMs_ = 0;
  bool     frontTurn_ = true;
};

#endif  // ENABLE_TIRES

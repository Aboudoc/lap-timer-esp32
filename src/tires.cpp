#include "tires.h"

#if ENABLE_TIRES

#include <Wire.h>

static const uint8_t RAM_TOBJ1   = 0x07;  // object temperature
static const uint8_t EEPROM_ADDR = 0x2E;  // SMBus address cell

void Tires::begin() {
  Wire1.begin(PIN_TIRE_SDA, PIN_TIRE_SCL, 100000);  // MLX90614 max is 100 kHz
  lastProbeMs_ = millis() - TIRE_PROBE_MS;
}

bool Tires::readObjectC(uint8_t addr, float& outC) {
  Wire1.beginTransmission(addr);
  Wire1.write(RAM_TOBJ1);
  if (Wire1.endTransmission(false) != 0) return false;
  if (Wire1.requestFrom((int)addr, 3) != 3) return false;
  uint8_t lsb = (uint8_t)Wire1.read();
  uint8_t msb = (uint8_t)Wire1.read();
  uint8_t pec = (uint8_t)Wire1.read();
  uint8_t frame[5] = {(uint8_t)(addr << 1), RAM_TOBJ1, (uint8_t)((addr << 1) | 1), lsb, msb};
  if (mlx::crc8(frame, 5) != pec) return false;
  return mlx::tempFromRaw((uint16_t)((msb << 8) | lsb), outC);
}

void Tires::loop() {
  uint32_t now = millis();
  if (now - lastSampleMs_ < TIRE_SAMPLE_MS) return;
  lastSampleMs_ = now;

  bool probing = (now - lastProbeMs_ >= TIRE_PROBE_MS);
  if (probing) lastProbeMs_ = now;

  // Alternate front/rear; only poll absent sensors during a probe round.
  uint8_t addr = frontTurn_ ? TIRE_FRONT_ADDR : TIRE_REAR_ADDR;
  bool seen = frontTurn_ ? frontSeen_ : rearSeen_;
  if (seen || probing) {
    float c;
    bool ok = readObjectC(addr, c);
    if (frontTurn_) {
      if (ok) { data_.frontC = c; data_.updatedMs = now; }
      else if (frontSeen_) Serial.println("[TIRES] front sensor lost");
      data_.frontOk = ok;
      frontSeen_ = ok;
    } else {
      if (ok) { data_.rearC = c; data_.updatedMs = now; }
      else if (rearSeen_) Serial.println("[TIRES] rear sensor lost");
      data_.rearOk = ok;
      rearSeen_ = ok;
    }
  }
  frontTurn_ = !frontTurn_;
}

bool Tires::writeEepromWord(uint8_t cmd, uint16_t value) {
  uint8_t lsb = (uint8_t)(value & 0xFF);
  uint8_t msb = (uint8_t)(value >> 8);
  uint8_t frame[4] = {0x00, cmd, lsb, msb};  // broadcast address 0x00
  uint8_t pec = mlx::crc8(frame, 4);
  Wire1.beginTransmission((uint8_t)0x00);
  Wire1.write(cmd);
  Wire1.write(lsb);
  Wire1.write(msb);
  Wire1.write(pec);
  bool ok = Wire1.endTransmission() == 0;
  delay(10);  // EEPROM write time
  return ok;
}

bool Tires::readdress(uint8_t newAddr) {
  if (newAddr < 0x08 || newAddr > 0x77) return false;
  // Erase the address cell, then write the new one (single sensor on the bus!).
  if (!writeEepromWord(EEPROM_ADDR, 0x0000)) return false;
  if (!writeEepromWord(EEPROM_ADDR, newAddr)) return false;
  Serial.printf("[TIRES] sensor re-addressed to 0x%02X — power-cycle it now\n", newAddr);
  return true;
}

#endif  // ENABLE_TIRES

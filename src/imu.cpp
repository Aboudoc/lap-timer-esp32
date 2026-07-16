#include "imu.h"

#if ENABLE_IMU

#include <Wire.h>

// MPU6050 registers
static const uint8_t REG_PWR_MGMT_1  = 0x6B;
static const uint8_t REG_CONFIG      = 0x1A;
static const uint8_t REG_GYRO_CFG    = 0x1B;
static const uint8_t REG_ACCEL_CFG   = 0x1C;
static const uint8_t REG_ACCEL_XOUT  = 0x3B;

static const float ACCEL_LSB_PER_G   = 8192.0f;  // +/-4 g range
static const float GYRO_LSB_PER_DPS  = 65.5f;    // +/-500 dps range

bool Imu::writeReg(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  return Wire.endTransmission() == 0;
}

bool Imu::probe() {
  if (!writeReg(REG_PWR_MGMT_1, 0x01)) return false;  // wake, PLL clock
  writeReg(REG_CONFIG, 0x03);      // DLPF 44 Hz
  writeReg(REG_GYRO_CFG, 0x08);    // +/-500 dps
  writeReg(REG_ACCEL_CFG, 0x08);   // +/-4 g
  Serial.println("[IMU] MPU6050 online");
  return true;
}

void Imu::begin() {
  Wire.begin(PIN_I2C_SDA, PIN_I2C_SCL);
  present_ = probe();
  data_.present = present_;
  lastProbeMs_ = millis();
}

bool Imu::readRaw(float& axG, float& ayG, float& azG,
                  float& gxDps, float& gyDps, float& gzDps) {
  Wire.beginTransmission(IMU_I2C_ADDR);
  Wire.write(REG_ACCEL_XOUT);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)IMU_I2C_ADDR, 14) != 14) return false;
  uint8_t b[14];
  for (int i = 0; i < 14; i++) b[i] = (uint8_t)Wire.read();
  int16_t ax = (int16_t)((b[0] << 8) | b[1]);
  int16_t ay = (int16_t)((b[2] << 8) | b[3]);
  int16_t az = (int16_t)((b[4] << 8) | b[5]);
  // b[6..7] = temperature, unused
  int16_t gx = (int16_t)((b[8] << 8) | b[9]);
  int16_t gy = (int16_t)((b[10] << 8) | b[11]);
  int16_t gz = (int16_t)((b[12] << 8) | b[13]);
  axG = ax / ACCEL_LSB_PER_G;
  ayG = ay / ACCEL_LSB_PER_G;
  azG = az / ACCEL_LSB_PER_G;
  gxDps = gx / GYRO_LSB_PER_DPS;
  gyDps = gy / GYRO_LSB_PER_DPS;
  gzDps = gz / GYRO_LSB_PER_DPS;
  return true;
}

void Imu::loop(float speedKmh) {
  uint32_t nowMs = millis();
  if (!present_) {
    if (nowMs - lastProbeMs_ >= IMU_PROBE_MS) {
      lastProbeMs_ = nowMs;
      present_ = probe();
      data_.present = present_;
    }
    return;
  }

  uint32_t nowUs = micros();
  if (nowUs - lastSampleUs_ < 5000) return;  // ~200 Hz max
  float dt = (nowUs - lastSampleUs_) / 1e6f;
  if (dt > 0.05f) dt = 0.05f;
  lastSampleUs_ = nowUs;

  float axG, ayG, azG, gxDps, gyDps, gzDps;
  if (!readRaw(axG, ayG, azG, gxDps, gyDps, gzDps)) {
    present_ = false;
    data_.present = false;
    Serial.println("[IMU] lost, will re-probe");
    return;
  }
  axG -= oax_;
  ayG -= oay_;
  gxDps -= bgx_;
  gyDps -= bgy_;
  gzDps -= bgz_;

  // Reference lean: kinematic (speed x yaw rate) when moving, gravity when slow.
  float leanRad = est_.leanDeg * imu::DEG2RAD_F;
  float yawEarthDps = gyDps * sinf(leanRad) + gzDps * cosf(leanRad);
  float ref;
  if (speedKmh > IMU_KIN_MIN_SPEED_KMH) {
    ref = imu::kinematicLeanDeg(speedKmh / 3.6f, yawEarthDps);
  } else {
    ref = imu::gravityRollDeg(ayG, azG);
  }
  est_.update(gxDps, ref, dt / IMU_KIN_TAU_S, dt);

  data_.leanDeg = est_.leanDeg;
  data_.gLong = axG;
  data_.gLat = ayG;
  data_.updatedMs = nowMs;
  if (est_.leanDeg > data_.maxLeanR) data_.maxLeanR = est_.leanDeg;
  if (-est_.leanDeg > data_.maxLeanL) data_.maxLeanL = -est_.leanDeg;
  if (-axG > data_.maxBrakeG) data_.maxBrakeG = -axG;
}

void Imu::resetLapPeaks() {
  data_.maxLeanL = 0;
  data_.maxLeanR = 0;
  data_.maxBrakeG = 0;
}

bool Imu::calibrate() {
  if (!present_) return false;
  float sax = 0, say = 0, sgx = 0, sgy = 0, sgz = 0;
  const int N = 300;
  int got = 0;
  for (int i = 0; i < N; i++) {
    float axG, ayG, azG, gxDps, gyDps, gzDps;
    if (readRaw(axG, ayG, azG, gxDps, gyDps, gzDps)) {
      sax += axG;
      say += ayG;
      sgx += gxDps;
      sgy += gyDps;
      sgz += gzDps;
      got++;
    }
    delay(2);
  }
  if (got < N / 2) return false;
  oax_ = sax / got;
  oay_ = say / got;
  bgx_ = sgx / got;
  bgy_ = sgy / got;
  bgz_ = sgz / got;
  est_.leanDeg = 0;
  Serial.printf("[IMU] calibrated: accel off %.3f/%.3f g, gyro bias %.2f/%.2f/%.2f dps\n",
                oax_, oay_, bgx_, bgy_, bgz_);
  return true;
}

void Imu::getCal(float out[5]) const {
  out[0] = bgx_; out[1] = bgy_; out[2] = bgz_; out[3] = oax_; out[4] = oay_;
}

void Imu::setCal(const float in[5]) {
  bgx_ = in[0]; bgy_ = in[1]; bgz_ = in[2]; oax_ = in[3]; oay_ = in[4];
}

#endif  // ENABLE_IMU

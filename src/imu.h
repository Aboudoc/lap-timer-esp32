#pragma once
#include <Arduino.h>
#include "config.h"
#include "imu_math.h"

#if ENABLE_IMU

// MPU6050 driver (raw registers over I2C, no library) + lean estimation.
// Probes quietly: works with or without the sensor plugged in.
class Imu {
 public:
  void begin();
  void loop(float speedKmh);
  const ImuData& data() const { return data_; }
  void resetLapPeaks();

  // Bike upright and still: measures gyro biases + accel level offsets
  // (~0.6 s, blocking). Persist the result with getCal/setCal.
  bool calibrate();
  void getCal(float out[5]) const;
  void setCal(const float in[5]);

 private:
  bool probe();
  bool readRaw(float& axG, float& ayG, float& azG,
               float& gxDps, float& gyDps, float& gzDps);
  bool writeReg(uint8_t reg, uint8_t val);

  ImuData data_;
  imu::LeanEstimator est_;
  bool     present_ = false;
  uint32_t lastProbeMs_ = 0;
  uint32_t lastSampleUs_ = 0;
  // calibration: gyro biases (dps) and accel level offsets (g)
  float bgx_ = 0, bgy_ = 0, bgz_ = 0, oax_ = 0, oay_ = 0;
};

#endif  // ENABLE_IMU

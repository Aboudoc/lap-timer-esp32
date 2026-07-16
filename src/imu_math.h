#pragma once
#include <stdint.h>
#include <math.h>

// Lean-angle estimation math. Pure logic (no Arduino dependency), covered by
// the host-side unit tests.
//
// Conventions: body frame X = forward, Y = left, Z = up.
// Positive lean/roll = leaning RIGHT.

struct ImuData {
  bool  present = false;
  float leanDeg = 0;                  // + = right
  float gLong = 0;                    // body X accel in g (+ = accelerating)
  float gLat = 0;                     // body Y accel in g (includes gravity when leaned)
  float maxLeanL = 0, maxLeanR = 0;   // per-lap peaks, degrees (absolute)
  float maxBrakeG = 0;                // per-lap braking peak, g
  uint32_t updatedMs = 0;
};

namespace imu {

constexpr float RAD2DEG_F = 57.29577951f;
constexpr float DEG2RAD_F = 0.017453293f;

// Steady-state cornering lean from speed and earth-frame yaw rate:
// tan(lean) = v * yawRate / g. Negative yaw (right turn) -> positive (right) lean.
inline float kinematicLeanDeg(float speedMs, float yawRateDps) {
  return -atan2f(speedMs * yawRateDps * DEG2RAD_F, 9.81f) * RAD2DEG_F;
}

// Roll from the gravity direction — only meaningful at low lateral dynamics
// (standstill, straights): in a steady corner the resultant points along the
// bike's vertical and this reads ~0.
inline float gravityRollDeg(float ayG, float azG) {
  if (azG < 0.05f) azG = 0.05f;
  return atan2f(ayG, azG) * RAD2DEG_F;
}

// Complementary filter: integrate the roll gyro for the fast dynamics, and
// slowly pull toward the kinematic (or gravity) estimate to cancel drift.
struct LeanEstimator {
  float leanDeg = 0;

  void update(float rollRateDps, float leanRefDeg, float refWeight, float dt) {
    leanDeg += rollRateDps * dt;
    if (refWeight > 1.0f) refWeight = 1.0f;
    if (refWeight > 0.0f) leanDeg += (leanRefDeg - leanDeg) * refWeight;
    if (leanDeg > 70.0f) leanDeg = 70.0f;
    if (leanDeg < -70.0f) leanDeg = -70.0f;
  }
};

}  // namespace imu

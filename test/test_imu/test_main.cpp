// Host-side unit tests of the lean-angle math:
//   pio test -e native
#include "../../src/imu_math.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static void test_kinematic_lean_45_deg() {
  // tan(lean) = v * yaw / g. 30 m/s and yaw = -18.73 dps (right turn)
  // -> v*yaw = 9.81 -> 45 degrees to the right (positive).
  float lean = imu::kinematicLeanDeg(30.0f, -18.7325f);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 45.0f, lean);
  // Left turn: same magnitude, negative sign.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, -45.0f, imu::kinematicLeanDeg(30.0f, 18.7325f));
  // Straight: zero.
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, imu::kinematicLeanDeg(40.0f, 0.0f));
}

static void test_gravity_roll() {
  // Upright: ay=0, az=1 -> 0 deg.
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 0.0f, imu::gravityRollDeg(0.0f, 1.0f));
  // Leaned right 30 deg at rest: ay = sin(30) = 0.5, az = cos(30) = 0.866.
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 30.0f, imu::gravityRollDeg(0.5f, 0.866f));
}

static void test_estimator_integrates_gyro() {
  imu::LeanEstimator e;
  // 10 dps roll for 2 s with no reference correction -> 20 deg.
  for (int i = 0; i < 200; i++) e.update(10.0f, 0.0f, 0.0f, 0.01f);
  TEST_ASSERT_FLOAT_WITHIN(0.5f, 20.0f, e.leanDeg);
}

static void test_estimator_converges_to_reference() {
  imu::LeanEstimator e;
  // No roll rate, reference at 40 deg, tau = 2 s: after 10 s it's there.
  float dt = 0.01f, tau = 2.0f;
  for (int i = 0; i < 1000; i++) e.update(0.0f, 40.0f, dt / tau, dt);
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 40.0f, e.leanDeg);
}

static void test_estimator_clamps() {
  imu::LeanEstimator e;
  for (int i = 0; i < 3000; i++) e.update(100.0f, 0.0f, 0.0f, 0.01f);
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 70.0f, e.leanDeg);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_kinematic_lean_45_deg);
  RUN_TEST(test_gravity_roll);
  RUN_TEST(test_estimator_integrates_gyro);
  RUN_TEST(test_estimator_converges_to_reference);
  RUN_TEST(test_estimator_clamps);
  return UNITY_END();
}

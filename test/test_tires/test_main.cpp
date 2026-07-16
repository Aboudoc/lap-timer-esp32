// Host-side unit tests of the MLX90614 helpers:
//   pio test -e native
#include "../../src/tires_proto.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static void test_crc8_known_vector() {
  // CRC-8 poly 0x07 of the single byte 0xB4 is 0x05 (hand-computed).
  uint8_t b = 0xB4;
  TEST_ASSERT_EQUAL_HEX8(0x05, mlx::crc8(&b, 1));
}

static void test_crc8_self_check() {
  // Appending a frame's own CRC must yield a CRC of 0.
  uint8_t frame[6] = {0xB4, 0x07, 0xB5, 0xD1, 0x3A, 0};
  frame[5] = mlx::crc8(frame, 5);
  TEST_ASSERT_EQUAL_HEX8(0x00, mlx::crc8(frame, 6));
}

static void test_temp_conversion() {
  // 0x3AF7 = 15095 -> 15095 * 0.02 K = 301.9 K = 28.75 C.
  float c = 0;
  TEST_ASSERT_TRUE(mlx::tempFromRaw(0x3AF7, c));
  TEST_ASSERT_FLOAT_WITHIN(0.05f, 28.75f, c);
  // Typical hot tire: 90 C = 363.15 K -> raw 18157.
  TEST_ASSERT_TRUE(mlx::tempFromRaw(18158, c));
  TEST_ASSERT_FLOAT_WITHIN(0.1f, 90.0f, c);
}

static void test_error_flag_rejected() {
  float c = 0;
  TEST_ASSERT_FALSE(mlx::tempFromRaw(0x8000 | 0x3AF7, c));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_crc8_known_vector);
  RUN_TEST(test_crc8_self_check);
  RUN_TEST(test_temp_conversion);
  RUN_TEST(test_error_flag_rejected);
  return UNITY_END();
}

// Host-side unit tests of the pit telemetry packet:
//   pio test -e native
#include "../../src/pitlink_proto.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static void test_packet_size_is_stable() {
  // Both radios exchange this exact layout: catch accidental growth.
  TEST_ASSERT_EQUAL_UINT32(42, sizeof(PitPacket));
}

static void test_roundtrip() {
  PitPacket p;
  p.seq = 42;
  p.session = 2;
  p.lapCount = 7;
  p.currentLapMs = 61234;
  p.lastLapMs = 83456;
  p.bestLapMs = 82100;
  p.predDeltaMs = -420;
  p.flags = PIT_F_TIMING | PIT_F_HASPRED;
  p.speedKmh = 142;
  p.leanDeg = -38;
  p.tireF = 68 + 40;
  p.tireR = 74 + 40;
  p.coolant = 82 + 40;
  p.rpm = 9800;
  strncpy(p.track, "MSP", sizeof(p.track) - 1);

  uint8_t buf[64];
  size_t n = pitEncode(p, buf);
  TEST_ASSERT_EQUAL_UINT32(sizeof(PitPacket), n);

  PitPacket q;
  TEST_ASSERT_TRUE(pitDecode(buf, n, q));
  TEST_ASSERT_EQUAL_UINT8(42, q.seq);
  TEST_ASSERT_EQUAL_UINT16(7, q.lapCount);
  TEST_ASSERT_EQUAL_INT32(-420, q.predDeltaMs);
  TEST_ASSERT_EQUAL_INT8(-38, q.leanDeg);
  TEST_ASSERT_EQUAL_UINT16(9800, q.rpm);
  TEST_ASSERT_EQUAL_STRING("MSP", q.track);
  TEST_ASSERT_TRUE(q.flags & PIT_F_HASPRED);
  TEST_ASSERT_FALSE(q.flags & PIT_F_LAPFLASH);
}

static void test_bad_magic_rejected() {
  PitPacket p;
  uint8_t buf[64];
  size_t n = pitEncode(p, buf);
  buf[0] ^= 0xFF;  // corrupt the magic
  PitPacket q;
  TEST_ASSERT_FALSE(pitDecode(buf, n, q));
}

static void test_wrong_length_rejected() {
  PitPacket p, q;
  uint8_t buf[64];
  size_t n = pitEncode(p, buf);
  TEST_ASSERT_FALSE(pitDecode(buf, n - 1, q));
  TEST_ASSERT_FALSE(pitDecode(buf, n + 1, q));
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_packet_size_is_stable);
  RUN_TEST(test_roundtrip);
  RUN_TEST(test_bad_magic_rejected);
  RUN_TEST(test_wrong_length_rejected);
  return UNITY_END();
}

// Host-side unit tests of the Kawasaki KDS protocol layer:
//   pio test -e native
#include "../../src/kds_proto.h"

#include <unity.h>

void setUp() {}
void tearDown() {}

static void test_build_read_rpm_frame() {
  uint8_t payload[2] = {kds::SID_READ_REG, kds::REG_RPM};
  uint8_t out[12];
  size_t n = kds::buildFrame(payload, 2, out, sizeof(out));
  TEST_ASSERT_EQUAL_UINT32(6, n);
  // 0x80|2, ECU 0x11, tester 0xF2, 0x21, 0x09, checksum
  TEST_ASSERT_EQUAL_HEX8(0x82, out[0]);
  TEST_ASSERT_EQUAL_HEX8(0x11, out[1]);
  TEST_ASSERT_EQUAL_HEX8(0xF2, out[2]);
  TEST_ASSERT_EQUAL_HEX8(0x21, out[3]);
  TEST_ASSERT_EQUAL_HEX8(0x09, out[4]);
  TEST_ASSERT_EQUAL_HEX8((0x82 + 0x11 + 0xF2 + 0x21 + 0x09) & 0xFF, out[5]);
}

static void test_parse_rpm_response() {
  // ECU -> tester: fmt 0x84, tgt 0xF2, src 0x11, payload 61 09 12 34, checksum.
  uint8_t frame[8] = {0x84, 0xF2, 0x11, 0x61, 0x09, 0x12, 0x34, 0};
  uint8_t cs = 0;
  for (int i = 0; i < 7; i++) cs = (uint8_t)(cs + frame[i]);
  frame[7] = cs;

  kds::Parser p;
  int r = 0;
  for (int i = 0; i < 8; i++) r = kds::feed(p, frame[i]);
  TEST_ASSERT_EQUAL_INT(1, r);
  TEST_ASSERT_EQUAL_UINT8(4, kds::payloadLen(p));
  TEST_ASSERT_EQUAL_HEX8(0x61, kds::payload(p)[0]);
  TEST_ASSERT_EQUAL_HEX8(0x09, kds::payload(p)[1]);
  TEST_ASSERT_EQUAL_INT(1852, kds::rpmFrom(kds::payload(p)[2], kds::payload(p)[3]));
}

static void test_checksum_error_rejected() {
  uint8_t frame[8] = {0x84, 0xF2, 0x11, 0x61, 0x09, 0x12, 0x34, 0x00};  // bad cs
  kds::Parser p;
  int r = 0;
  for (int i = 0; i < 8; i++) r = kds::feed(p, frame[i]);
  TEST_ASSERT_EQUAL_INT(-1, r);
  TEST_ASSERT_EQUAL_UINT8(0, p.n);  // parser reset, ready to resync
}

static void test_resync_after_garbage() {
  kds::Parser p;
  TEST_ASSERT_EQUAL_INT(-1, kds::feed(p, 0x00));  // not a format byte
  TEST_ASSERT_EQUAL_INT(-1, kds::feed(p, 0x55));
  uint8_t frame[6] = {0x82, 0xF2, 0x11, 0xC1, 0xEA, 0};
  uint8_t cs = 0;
  for (int i = 0; i < 5; i++) cs = (uint8_t)(cs + frame[i]);
  frame[5] = cs;
  int r = 0;
  for (int i = 0; i < 6; i++) r = kds::feed(p, frame[i]);
  TEST_ASSERT_EQUAL_INT(1, r);
  TEST_ASSERT_EQUAL_HEX8(0xC1, kds::payload(p)[0]);
}

static void test_conversions() {
  TEST_ASSERT_EQUAL_INT(12500, kds::rpmFrom(125, 0));
  TEST_ASSERT_EQUAL_INT(3, kds::gearFrom(3));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 40.0f, kds::speedFrom(0x00, 0x50));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 50.0f, kds::coolantFrom(128));
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, kds::throttleFrom(0, 0));          // below closed
  TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, kds::throttleFrom(0x03, 0x7E));  // raw 894 = WOT
  TEST_ASSERT_FLOAT_WITHIN(1.0f, 50.0f, kds::throttleFrom(0x02, 0x28));    // raw 552 ~ mid
}

static void test_frame_too_long_rejected() {
  uint8_t payload[64];
  uint8_t out[80];
  TEST_ASSERT_EQUAL_UINT32(0, kds::buildFrame(payload, 64, out, sizeof(out)));
  TEST_ASSERT_EQUAL_UINT32(0, kds::buildFrame(payload, 4, out, 6));  // buffer too small
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_build_read_rpm_frame);
  RUN_TEST(test_parse_rpm_response);
  RUN_TEST(test_checksum_error_rejected);
  RUN_TEST(test_resync_after_garbage);
  RUN_TEST(test_conversions);
  RUN_TEST(test_frame_too_long_rejected);
  return UNITY_END();
}

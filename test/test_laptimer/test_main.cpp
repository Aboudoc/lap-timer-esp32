// Host-side unit tests of the timing core (no hardware needed):
//   pio test -e native
#include "../../src/laptimer.cpp"

#include <unity.h>

static const double LAT0 = 13.7563;
static const double LON0 = 100.5018;
static const double DEG = 0.017453292519943295;

void setUp() {}
void tearDown() {}

// A fix at (xE meters east, yN meters north) of the line center.
static GpsFix mkFix(float xE, float yN, uint32_t msOfDay, uint32_t localMs,
                    float spd = 100, float course = 90) {
  GpsFix f;
  f.lat = LAT0 + yN / 111194.9;
  f.lon = LON0 + xE / (111194.9 * cos(LAT0 * DEG));
  f.speedKmh = spd;
  f.courseDeg = course;
  f.msOfDay = msOfDay % MS_PER_DAY;
  f.localMs = localMs;
  f.valid = true;
  return f;
}

static StartLine eastLine() {
  StartLine l;
  l.lat = LAT0;
  l.lon = LON0;
  l.headingDeg = 90;  // crossed riding east
  l.isSet = true;
  return l;
}

// Walks a straight segment in `steps` fixes of 1 s each. Returns the updated
// time. Reports completed laps through *laps.
static void walk(LapTimer& t, float x0, float y0, float x1, float y1, int steps,
                 uint32_t* ms, int* laps = nullptr) {
  for (int i = 1; i <= steps; i++) {
    float x = x0 + (x1 - x0) * i / steps;
    float y = y0 + (y1 - y0) * i / steps;
    *ms += 1000;
    if (t.onFix(mkFix(x, y, *ms, *ms)) && laps) (*laps)++;
  }
}

// Seeds the position history with a fix just before the line, so the next
// walk() step produces a proper crossing segment.
static void prime(LapTimer& t, uint32_t* ms) {
  t.onFix(mkFix(-10, 0, *ms, *ms));
}

// One rectangular lap: cross the gate eastwards, big detour, come back.
// ~44 s, ~860 m. Starts and ends at (-10, 0).
static void rectangleLap(LapTimer& t, uint32_t* ms, int* laps) {
  walk(t, -10, 0, 10, 0, 1, ms, laps);      // cross the line
  walk(t, 10, 0, 210, 0, 10, ms, laps);     // east straight
  walk(t, 210, 0, 210, 200, 10, ms, laps);  // north
  walk(t, 210, 200, -10, 200, 11, ms, laps);  // west, far from the gate
  walk(t, -10, 200, -10, 0, 10, ms, laps);  // south, back before the line
}

static void test_basic_lap_and_interpolation() {
  LapTimer t;
  t.setLine(eastLine());

  // Crossing exactly halfway between (-10,0) and (+10,0): cross at ms 10500...
  // fixes at 10000 (before) and 11000 (after) -> cross interpolated at 10500.
  t.onFix(mkFix(-10, 0, 10000, 10000));
  t.onFix(mkFix(10, 0, 11000, 11000));
  TEST_ASSERT_TRUE(t.timing());
  TEST_ASSERT_EQUAL_INT(0, t.lapCount());

  // Second crossing 40 s later, same geometry -> lap of exactly 40000 ms.
  t.onFix(mkFix(-10, 0, 50000, 50000));
  bool lap = t.onFix(mkFix(10, 0, 51000, 51000));
  TEST_ASSERT_TRUE(lap);
  TEST_ASSERT_EQUAL_INT(1, t.lapCount());
  TEST_ASSERT_EQUAL_UINT32(40000, t.lastLapMs());
  TEST_ASSERT_EQUAL_UINT32(40000, t.allTimeBestMs());
  TEST_ASSERT_TRUE(t.lastLapWasRecord());
  TEST_ASSERT_TRUE(t.newAllTimeBest());
}

static void test_wrong_direction_ignored() {
  LapTimer t;
  t.setLine(eastLine());
  // East to west: wrong way.
  t.onFix(mkFix(10, 0, 10000, 10000, 100, 270));
  t.onFix(mkFix(-10, 0, 11000, 11000, 100, 270));
  TEST_ASSERT_FALSE(t.timing());
}

static void test_outside_gate_ignored() {
  LapTimer t;
  t.setLine(eastLine());
  // Crossing 50 m north of the center: outside the 20 m half-width.
  t.onFix(mkFix(-10, 50, 10000, 10000));
  t.onFix(mkFix(10, 50, 11000, 11000));
  TEST_ASSERT_FALSE(t.timing());
}

static void test_min_lap_debounce() {
  LapTimer t;
  t.setLine(eastLine());
  t.onFix(mkFix(-10, 0, 10000, 10000));
  t.onFix(mkFix(10, 0, 11000, 11000));  // clock starts
  // Re-crossing 10 s later: under MIN_LAP_MS, must be ignored.
  t.onFix(mkFix(-10, 0, 20000, 20000));
  bool lap = t.onFix(mkFix(10, 0, 21000, 21000));
  TEST_ASSERT_FALSE(lap);
  TEST_ASSERT_EQUAL_INT(0, t.lapCount());
  TEST_ASSERT_TRUE(t.timing());
}

static void test_midnight_wrap() {
  LapTimer t;
  t.setLine(eastLine());
  // Clock starts 5 s before midnight, lap completes 35 s after.
  uint32_t m1 = MS_PER_DAY - 5000;
  t.onFix(mkFix(-10, 0, m1, 1000));
  t.onFix(mkFix(10, 0, m1 + 1000, 2000));  // cross at midnight - 4.5 s
  t.onFix(mkFix(-10, 0, 34000, 41000));
  bool lap = t.onFix(mkFix(10, 0, 35000, 42000));  // cross at 00:00:34.5
  TEST_ASSERT_TRUE(lap);
  TEST_ASSERT_EQUAL_UINT32(39000, t.lastLapMs());
}

static void test_session_gap_starts_new_session() {
  LapTimer t;
  t.setLine(eastLine());
  uint32_t ms = 10000;
  int laps = 0;
  prime(t, &ms);
  rectangleLap(t, &ms, &laps);          // starts the clock
  rectangleLap(t, &ms, &laps);          // lap 1
  TEST_ASSERT_EQUAL_INT(1, t.lapCount());
  TEST_ASSERT_EQUAL_INT(1, t.sessionIndex());

  // 6 minutes in the pits, then a new crossing.
  ms += 360000;
  t.onFix(mkFix(-10, 0, ms, ms));
  ms += 1000;
  bool lap = t.onFix(mkFix(10, 0, ms, ms));
  TEST_ASSERT_FALSE(lap);
  TEST_ASSERT_EQUAL_INT(2, t.sessionIndex());
  TEST_ASSERT_EQUAL_INT(0, t.lapCount());   // session wiped
  TEST_ASSERT_TRUE(t.timing());             // but the clock is running
  TEST_ASSERT_GREATER_THAN_UINT32(0, t.allTimeBestMs());  // track data kept
}

static void test_sectors_and_theoretical_best() {
  LapTimer t;
  t.setLine(eastLine());
  uint32_t ms = 10000;
  int laps = 0;
  prime(t, &ms);
  rectangleLap(t, &ms, &laps);  // starts the clock
  rectangleLap(t, &ms, &laps);  // lap 1: becomes the reference (no sectors yet)
  TEST_ASSERT_EQUAL_INT(1, laps);
  TEST_ASSERT_FALSE(t.hasSectors());

  rectangleLap(t, &ms, &laps);  // lap 2: sectors measured against the reference
  TEST_ASSERT_EQUAL_INT(2, laps);
  TEST_ASSERT_TRUE(t.hasSectors());

  // Same pace on both laps: each sector delta should be tiny, and the
  // sectors must add up to the lap time.
  uint32_t sum = 0;
  for (int k = 0; k < NUM_SECTORS; k++) {
    TEST_ASSERT_INT32_WITHIN(300, 0, t.lastSectorDeltaMs(k));
    sum += t.lastSectorMs(k);
  }
  TEST_ASSERT_EQUAL_UINT32(t.lastLapMs(), sum);
  TEST_ASSERT_GREATER_THAN_UINT32(0, t.theoreticalBestMs());
  TEST_ASSERT_TRUE(t.theoreticalBestMs() <= t.lastLapMs());
}

static void test_predictive_delta_near_zero_at_same_pace() {
  LapTimer t;
  t.setLine(eastLine());
  uint32_t ms = 10000;
  int laps = 0;
  prime(t, &ms);
  rectangleLap(t, &ms, &laps);  // starts the clock
  rectangleLap(t, &ms, &laps);  // lap 1 -> reference

  // Half of lap 2 at the same pace: the live delta should stay close to 0.
  walk(t, -10, 0, 10, 0, 1, &ms, &laps);
  walk(t, 10, 0, 210, 0, 10, &ms, &laps);
  walk(t, 210, 0, 210, 200, 10, &ms, &laps);
  TEST_ASSERT_TRUE(t.hasPredDelta());
  TEST_ASSERT_INT32_WITHIN(500, 0, t.predDeltaMs());
}

static void test_reference_reload_gives_delta_on_lap_one() {
  LapTimer big;
  big.setLine(eastLine());
  uint32_t ms = 10000;
  int laps = 0;
  prime(big, &ms);
  rectangleLap(big, &ms, &laps);
  rectangleLap(big, &ms, &laps);
  TEST_ASSERT_TRUE(big.refN() > 10);

  // Simulate a fresh boot that reloads the persisted reference.
  LapTimer t2;
  t2.setLine(eastLine());
  memcpy(t2.refDistBuffer(), big.refDist(), big.refN() * sizeof(float));
  memcpy(t2.refTimeBuffer(), big.refTms(), big.refN() * sizeof(uint32_t));
  t2.commitReference(big.refN());
  t2.setAllTimeBest(big.allTimeBestMs());

  uint32_t ms2 = 5000;
  int laps2 = 0;
  prime(t2, &ms2);
  walk(t2, -10, 0, 10, 0, 1, &ms2, &laps2);   // clock starts
  walk(t2, 10, 0, 210, 0, 10, &ms2, &laps2);  // some distance into lap 1
  TEST_ASSERT_TRUE(t2.hasPredDelta());        // delta live from lap 1
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_basic_lap_and_interpolation);
  RUN_TEST(test_wrong_direction_ignored);
  RUN_TEST(test_outside_gate_ignored);
  RUN_TEST(test_min_lap_debounce);
  RUN_TEST(test_midnight_wrap);
  RUN_TEST(test_session_gap_starts_new_session);
  RUN_TEST(test_sectors_and_theoretical_best);
  RUN_TEST(test_predictive_delta_near_zero_at_same_pace);
  RUN_TEST(test_reference_reload_gives_delta_on_lap_one);
  return UNITY_END();
}

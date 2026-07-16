#include "laptimer.h"
#include <math.h>

// Difference between two "ms since midnight" times, robust across midnight.
static uint32_t dayDiff(uint32_t later, uint32_t earlier) {
  return (later + MS_PER_DAY - earlier) % MS_PER_DAY;
}

void fmtLapTime(char* buf, size_t n, uint32_t ms, bool centis) {
  unsigned long m = ms / 60000UL;
  unsigned long s = (ms / 1000UL) % 60UL;
  if (centis) {
    snprintf(buf, n, "%lu:%02lu.%02lu", m, s, (unsigned long)((ms / 10UL) % 100UL));
  } else {
    snprintf(buf, n, "%lu:%02lu.%lu", m, s, (unsigned long)((ms / 100UL) % 10UL));
  }
}

void LapTimer::setLine(const StartLine& line) {
  line_ = line;
  float h = line.headingDeg * (float)DEG_TO_RAD;
  dirX_ = sinf(h);  // east component
  dirY_ = cosf(h);  // north component
  mPerDegLat_ = 111194.9f;
  mPerDegLon_ = 111194.9f * cosf((float)(line.lat * DEG_TO_RAD));
  resetSession();
}

void LapTimer::resetSession() {
  timing_ = false;
  lapCount_ = 0;
  lastLapMs_ = 0;
  lastLapMaxSpeed_ = 0;
  bestMs_ = 0;
  bestIdx_ = -1;
  lastDeltaBest_ = 0;
  curMaxSpeed_ = 0;
  sessMaxSpeed_ = 0;
  hasPrev_ = false;
  distToLine_ = NAN;
}

// Local coordinates in meters, centered on the line (x = east, y = north).
void LapTimer::toLocal(double lat, double lon, float& x, float& y) const {
  x = (float)((lon - line_.lon) * mPerDegLon_);
  y = (float)((lat - line_.lat) * mPerDegLat_);
}

// Tests whether segment a->b crosses the gate in the right direction.
// tOut = fraction of the segment where the crossing happens (0..1).
bool LapTimer::crossed(const GpsFix& a, const GpsFix& b, float& tOut) const {
  float ax, ay, bx, by;
  toLocal(a.lat, a.lon, ax, ay);
  toLocal(b.lat, b.lon, bx, by);

  // Signed distance along the crossing heading: negative = before the line,
  // positive = past it. A lap = strict negative -> positive transition,
  // which also enforces the crossing direction.
  float sa = ax * dirX_ + ay * dirY_;
  float sb = bx * dirX_ + by * dirY_;
  if (!(sa < 0.0f && sb >= 0.0f)) return false;

  float dx = bx - ax, dy = by - ay;
  if (dx * dx + dy * dy > MAX_FIX_JUMP_M * MAX_FIX_JUMP_M) return false;

  float t = sa / (sa - sb);

  // Lateral position at the crossing point: must fall inside the gate.
  float ux = dirY_, uy = -dirX_;
  float lateral = (ax + t * dx) * ux + (ay + t * dy) * uy;
  if (fabsf(lateral) > line_.halfWidthM) return false;

  if (fmaxf(a.speedKmh, b.speedKmh) < MIN_CROSS_SPEED_KMH) return false;

  tOut = t;
  return true;
}

bool LapTimer::onFix(const GpsFix& fix) {
  if (!fix.valid) return false;
  bool lapDone = false;

  if (line_.isSet) {
    float x, y;
    toLocal(fix.lat, fix.lon, x, y);
    distToLine_ = sqrtf(x * x + y * y);
  }

  if (timing_) {
    if (fix.speedKmh > curMaxSpeed_) curMaxSpeed_ = fix.speedKmh;
    if (fix.speedKmh > sessMaxSpeed_) sessMaxSpeed_ = fix.speedKmh;
  }

  float t;
  if (hasPrev_ && line_.isSet &&
      fix.localMs - prev_.localMs <= GPS_FIX_MAX_GAP_MS &&
      crossed(prev_, fix, t)) {
    // Interpolate the exact crossing moment between the two fixes: this is
    // what gives a precision far better than 1/5th of a second.
    uint32_t dtFix        = dayDiff(fix.msOfDay, prev_.msOfDay);
    uint32_t crossMsOfDay = (prev_.msOfDay + (uint32_t)(t * dtFix)) % MS_PER_DAY;
    uint32_t crossLocalMs = prev_.localMs + (uint32_t)(t * (fix.localMs - prev_.localMs));

    if (!timing_) {
      // First crossing: the clock starts.
      timing_ = true;
      lastCrossMsOfDay_ = crossMsOfDay;
      lastCrossLocalMs_ = crossLocalMs;
      curMaxSpeed_ = fix.speedKmh;
    } else {
      uint32_t lapMs = dayDiff(crossMsOfDay, lastCrossMsOfDay_);
      if (lapMs >= MIN_LAP_MS) {
        if (lapCount_ < MAX_LAPS) {
          laps_[lapCount_].ms = lapMs;
          laps_[lapCount_].maxSpeedKmh = curMaxSpeed_;
        }
        lastLapMs_ = lapMs;
        lastLapMaxSpeed_ = curMaxSpeed_;
        lastDeltaBest_ = (bestMs_ > 0) ? (int32_t)lapMs - (int32_t)bestMs_ : 0;
        if (bestMs_ == 0 || lapMs < bestMs_) {
          bestMs_ = lapMs;
          bestIdx_ = lapCount_;
        }
        lapCount_++;
        lastCrossMsOfDay_ = crossMsOfDay;
        lastCrossLocalMs_ = crossLocalMs;
        curMaxSpeed_ = fix.speedKmh;
        lapDone = true;
      }
    }
  }

  prev_ = fix;
  hasPrev_ = true;
  return lapDone;
}

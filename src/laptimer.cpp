#include "laptimer.h"
#include <stdio.h>
#include <string.h>

#ifndef DEG_TO_RAD
#define DEG_TO_RAD 0.017453292519943295
#endif

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
  clearTrackData();  // the gate moved: old reference/records no longer apply
  resetSession();
}

void LapTimer::clearTrackData() {
  ref_.n = 0;
  refTotalDist_ = 0;
  allTimeBest_ = 0;
  newAllTimeBest_ = false;
  sectorsImproved_ = false;
  for (int k = 0; k < NUM_SECTORS; k++) bestSectorMs_[k] = 0;
  computeBoundaries();
}

void LapTimer::resetSession() {
  timing_ = false;
  sessionIdx_ = 1;
  lapCount_ = 0;
  lastLapMs_ = 0;
  lastLapMaxSpeed_ = 0;
  bestMs_ = 0;
  bestIdx_ = -1;
  sessionTotalMs_ = 0;
  lastDeltaBest_ = 0;
  lastLapWasRecord_ = false;
  curMaxSpeed_ = 0;
  sessMaxSpeed_ = 0;
  hasPrev_ = false;
  distToLine_ = NAN;
  cur_.n = 0;
  curDist_ = 0;
  refWalk_ = 0;
  predDelta_ = 0;
  predValid_ = false;
  lastLapHasSectors_ = false;
  secIdx_ = 0;
  lastSplitElapsed_ = 0;
  prevPtDist_ = 0;
  prevPtElapsed_ = 0;
}

// A long stop happened (pits, lunch): keep the track data, wipe the session.
void LapTimer::newSessionReset() {
  sessionIdx_++;
  lapCount_ = 0;
  lastLapMs_ = 0;
  lastLapMaxSpeed_ = 0;
  bestMs_ = 0;
  bestIdx_ = -1;
  sessionTotalMs_ = 0;
  lastDeltaBest_ = 0;
  lastLapWasRecord_ = false;
  sessMaxSpeed_ = 0;
  lastLapHasSectors_ = false;
}

void LapTimer::commitReference(uint16_t n) {
  ref_.n = (n <= TRACE_MAX_SAMPLES) ? n : TRACE_MAX_SAMPLES;
  refWalk_ = 0;
  computeBoundaries();
}

void LapTimer::setBestSectors(const uint32_t* s) {
  for (int k = 0; k < NUM_SECTORS; k++) bestSectorMs_[k] = s[k];
}

uint32_t LapTimer::theoreticalBestMs() const {
  uint32_t sum = 0;
  for (int k = 0; k < NUM_SECTORS; k++) {
    if (bestSectorMs_[k] == 0) return 0;
    sum += bestSectorMs_[k];
  }
  return sum;
}

// Local coordinates in meters, centered on the line (x = east, y = north).
void LapTimer::toLocal(double lat, double lon, float& x, float& y) const {
  x = (float)((lon - line_.lon) * mPerDegLon_);
  y = (float)((lat - line_.lat) * mPerDegLat_);
}

void LapTimer::traceAppend(float d, uint32_t ms) {
  if (cur_.n < TRACE_MAX_SAMPLES) {
    cur_.dist[cur_.n] = d;
    cur_.tMs[cur_.n] = ms;
    cur_.n++;
  }
}

// Starts the trace of a new lap. The lap begins on the line, so the first
// stored point is (0, 0); the fix that revealed the crossing already sits a
// partial segment past the line.
void LapTimer::traceRestart(float initialDist, uint32_t initialElapsed) {
  cur_.n = 0;
  traceAppend(0.0f, 0);
  curDist_ = 0;
  refWalk_ = 0;
  secIdx_ = 0;
  lastSplitElapsed_ = 0;
  prevPtDist_ = 0;
  prevPtElapsed_ = 0;
  for (int k = 0; k < NUM_SECTORS; k++) curSectorMs_[k] = 0;
  curDist_ = initialDist;
  advancePoint(curDist_, initialElapsed);
}

// Moves the lap forward to (distance d, elapsed time): records the trace
// point and detects sector-boundary crossings with linear interpolation.
void LapTimer::advancePoint(float d, uint32_t elapsed) {
  while (secIdx_ < NUM_SECTORS - 1 && refTotalDist_ > 0 && d >= secBoundary_[secIdx_]) {
    float bd = secBoundary_[secIdx_];
    float f = (d > prevPtDist_) ? (bd - prevPtDist_) / (d - prevPtDist_) : 0.0f;
    if (f < 0.0f) f = 0.0f;
    if (f > 1.0f) f = 1.0f;
    uint32_t tb = prevPtElapsed_ + (uint32_t)(f * (float)(elapsed - prevPtElapsed_));
    curSectorMs_[secIdx_] = tb - lastSplitElapsed_;
    lastSplitElapsed_ = tb;
    secIdx_++;
  }
  prevPtDist_ = d;
  prevPtElapsed_ = elapsed;
  traceAppend(d, elapsed);
}

// The lap that just finished is the new all-time best: it becomes the
// reference for the predictive delta and the sector boundaries.
void LapTimer::adoptReference() {
  memcpy(ref_.dist, cur_.dist, cur_.n * sizeof(float));
  memcpy(ref_.tMs, cur_.tMs, cur_.n * sizeof(uint32_t));
  ref_.n = cur_.n;
  computeBoundaries();
}

void LapTimer::computeBoundaries() {
  refTotalDist_ = ref_.n ? ref_.dist[ref_.n - 1] : 0.0f;
  for (int k = 0; k < NUM_SECTORS - 1; k++) {
    secBoundary_[k] = refTotalDist_ * (float)(k + 1) / (float)NUM_SECTORS;
  }
}

// Called at lap completion: closes the last sector, computes deltas vs the
// best sectors and updates them.
void LapTimer::finishLapSectors(uint32_t lapMs) {
  lastLapHasSectors_ = (refTotalDist_ > 0 && secIdx_ == NUM_SECTORS - 1);
  if (!lastLapHasSectors_) return;
  curSectorMs_[NUM_SECTORS - 1] = lapMs - lastSplitElapsed_;
  for (int k = 0; k < NUM_SECTORS; k++) {
    lastLapSectorMs_[k] = curSectorMs_[k];
    lastSectorDelta_[k] = bestSectorMs_[k]
        ? (int32_t)curSectorMs_[k] - (int32_t)bestSectorMs_[k] : 0;
    if (bestSectorMs_[k] == 0 || curSectorMs_[k] < bestSectorMs_[k]) {
      bestSectorMs_[k] = curSectorMs_[k];
      sectorsImproved_ = true;
    }
  }
}

// Elapsed time of the reference lap at distance d, linearly interpolated.
// refWalk_ only moves forward: O(1) amortized per fix.
uint32_t LapTimer::refTimeAtDist(float d) {
  if (ref_.n == 0) return 0;
  while (refWalk_ + 1 < ref_.n && ref_.dist[refWalk_ + 1] < d) refWalk_++;
  if (refWalk_ + 1 >= ref_.n) return ref_.tMs[ref_.n - 1];
  float d0 = ref_.dist[refWalk_], d1 = ref_.dist[refWalk_ + 1];
  uint32_t t0 = ref_.tMs[refWalk_], t1 = ref_.tMs[refWalk_ + 1];
  if (d1 <= d0) return t0;
  float f = (d - d0) / (d1 - d0);
  if (f < 0.0f) f = 0.0f;
  if (f > 1.0f) f = 1.0f;
  return t0 + (uint32_t)(f * (float)(t1 - t0));
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

  // Local position and length of the segment since the previous fix.
  float segLen = 0;
  bool haveSeg = false;
  if (line_.isSet) {
    float x, y;
    toLocal(fix.lat, fix.lon, x, y);
    distToLine_ = sqrtf(x * x + y * y);
    if (hasPrev_) {
      float px, py;
      toLocal(prev_.lat, prev_.lon, px, py);
      segLen = sqrtf((x - px) * (x - px) + (y - py) * (y - py));
      haveSeg = true;
    }
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
      traceRestart(segLen * (1.0f - t), dayDiff(fix.msOfDay, crossMsOfDay));
    } else {
      uint32_t lapMs = dayDiff(crossMsOfDay, lastCrossMsOfDay_);
      if (lapMs > SESSION_GAP_MS) {
        // Long stop (pits, lunch break): this crossing opens a new session.
        newSessionReset();
        lastCrossMsOfDay_ = crossMsOfDay;
        lastCrossLocalMs_ = crossLocalMs;
        curMaxSpeed_ = fix.speedKmh;
        traceRestart(segLen * (1.0f - t), dayDiff(fix.msOfDay, crossMsOfDay));
      } else if (lapMs >= MIN_LAP_MS) {
        advancePoint(curDist_ + segLen * t, lapMs);  // close the lap exactly on the line
        finishLapSectors(lapMs);
        if (lapCount_ < MAX_LAPS) {
          laps_[lapCount_].ms = lapMs;
          laps_[lapCount_].maxSpeedKmh = curMaxSpeed_;
        }
        lastLapMs_ = lapMs;
        lastLapMaxSpeed_ = curMaxSpeed_;
        sessionTotalMs_ += lapMs;
        lastDeltaBest_ = allTimeBest_ ? (int32_t)lapMs - (int32_t)allTimeBest_ : 0;
        if (bestMs_ == 0 || lapMs < bestMs_) {
          bestMs_ = lapMs;
          bestIdx_ = lapCount_;
        }
        lastLapWasRecord_ = (allTimeBest_ == 0 || lapMs < allTimeBest_);
        if (lastLapWasRecord_) {
          allTimeBest_ = lapMs;
          adoptReference();
          newAllTimeBest_ = true;
        }
        lapCount_++;
        lastCrossMsOfDay_ = crossMsOfDay;
        lastCrossLocalMs_ = crossLocalMs;
        curMaxSpeed_ = fix.speedKmh;
        traceRestart(segLen * (1.0f - t), dayDiff(fix.msOfDay, crossMsOfDay));
        lapDone = true;
      } else if (haveSeg) {
        // Debounced crossing: ignored, but the distance still counts.
        curDist_ += segLen;
        advancePoint(curDist_, dayDiff(fix.msOfDay, lastCrossMsOfDay_));
      }
    }
  } else if (timing_ && haveSeg) {
    curDist_ += segLen;
    advancePoint(curDist_, dayDiff(fix.msOfDay, lastCrossMsOfDay_));
  }

  // Live delta vs the reference lap, compared at equal distance.
  if (timing_ && ref_.n >= 2) {
    predDelta_ = (int32_t)dayDiff(fix.msOfDay, lastCrossMsOfDay_) -
                 (int32_t)refTimeAtDist(curDist_);
    predValid_ = true;
  } else {
    predValid_ = false;
  }

  prev_ = fix;
  hasPrev_ = true;
  return lapDone;
}

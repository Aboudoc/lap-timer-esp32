#pragma once
#include <stdint.h>
#include <math.h>
#include "config.h"
#include "fix.h"

// Start/finish line: one point + the crossing heading.
// The "gate" is a segment of 2 x halfWidthM, centered on (lat, lon),
// perpendicular to the heading. A lap = crossing the gate in the right direction.
struct StartLine {
  double lat = 0, lon = 0;
  float  headingDeg = 0;                    // expected heading when crossing
  float  halfWidthM = LINE_HALF_WIDTH_M;
  bool   isSet = false;
};

struct Lap {
  uint32_t ms = 0;
  float    maxSpeedKmh = 0;
};

// Formats a lap time: "1:23.4" (centis=false) or "1:23.45" (centis=true).
void fmtLapTime(char* buf, size_t n, uint32_t ms, bool centis);

// Extra channels sampled along the lap traces (all optional).
struct LapChannels {
  float leanDeg = 0;
  int   rpm = 0;
  float thrPct = -1;  // -1 = unknown
};

// The timing core. Pure logic, no Arduino dependency: it also builds on the
// host for the unit tests (test/test_laptimer).
//
// Scopes:
//  - session data (laps list, session best, vmax) resets between sessions
//  - track data (all-time best, reference trace, best sectors) belongs to the
//    active track and is persisted by the caller
class LapTimer {
 public:
  // Distance -> time trace of a lap with telemetry channels, sampled at
  // every fix. Public: storage persists it, the web app exports it.
  struct Trace {
    uint16_t n = 0;
    float    dist[TRACE_MAX_SAMPLES];   // cumulative meters since the line
    uint32_t tMs[TRACE_MAX_SAMPLES];    // elapsed ms since the line
    uint8_t  spd[TRACE_MAX_SAMPLES];    // km/h
    int8_t   lean[TRACE_MAX_SAMPLES];   // deg, + = right
    uint16_t rpm[TRACE_MAX_SAMPLES];
    uint8_t  thr[TRACE_MAX_SAMPLES];    // %, 255 = unknown
  };

  // ---- Track setup ----
  void setLine(const StartLine& line);  // wipes track data + session (new gate)
  const StartLine& line() const { return line_; }
  void clearTrackData();                // erase all-time best / reference / sectors

  // Load persisted track data: fill the reference trace, then commit.
  Trace* refTraceMutable() { return &ref_; }
  void   commitReference(uint16_t n);
  void   setAllTimeBest(uint32_t ms) { allTimeBest_ = ms; }
  void   setBestSectors(const uint32_t* s);

  // Persistence/export: reference (all-time best) and last completed lap.
  const Trace* refTrace() const { return &ref_; }
  const Trace* lastTrace() const { return hasLast_ ? last_ : nullptr; }
  uint16_t refN() const { return ref_.n; }
  bool newAllTimeBest() const { return newAllTimeBest_; }
  bool sectorsImproved() const { return sectorsImproved_; }
  void ackPersist() { newAllTimeBest_ = false; sectorsImproved_ = false; }

  // ---- Live feed ----
  // Feed a new fix (+ optional telemetry channels); true when a lap completed.
  bool onFix(const GpsFix& fix, const LapChannels* ch = nullptr);
  void resetSession();

  // ---- Session state ----
  bool     timing() const { return timing_; }
  int      sessionIndex() const { return sessionIdx_; }
  int      lapCount() const { return lapCount_; }
  const Lap& lap(int i) const { return laps_[i]; }  // i < storedLaps()
  int      storedLaps() const { return lapCount_ < MAX_LAPS ? lapCount_ : MAX_LAPS; }
  uint32_t lastLapMs() const { return lastLapMs_; }
  float    lastLapMaxSpeed() const { return lastLapMaxSpeed_; }
  uint32_t bestLapMs() const { return bestMs_; }          // session best
  int      bestLapIndex() const { return bestIdx_; }
  uint32_t sessionAvgMs() const { return lapCount_ ? sessionTotalMs_ / (uint32_t)lapCount_ : 0; }
  float    sessionMaxSpeed() const { return sessMaxSpeed_; }
  uint32_t currentLapMs(uint32_t nowMs) const { return timing_ ? nowMs - lastCrossLocalMs_ : 0; }
  uint32_t lastCrossLocalMs() const { return lastCrossLocalMs_; }
  uint32_t lastCrossMsOfDay() const { return lastCrossMsOfDay_; }
  float    distToLineM() const { return distToLine_; }

  // ---- Track records ----
  uint32_t allTimeBestMs() const { return allTimeBest_; }
  bool     lastLapWasRecord() const { return lastLapWasRecord_; }
  int32_t  lastDeltaBestMs() const { return lastDeltaBest_; }  // last lap vs all-time best before it

  // ---- Predictive delta ----
  // Live gap vs the reference (all-time best) lap, compared at the same
  // distance along the lap. Available as soon as a reference exists.
  bool     hasPredDelta() const { return predValid_; }
  int32_t  predDeltaMs() const { return predDelta_; }

  // ---- Sectors ----
  // The reference lap is split into NUM_SECTORS equal-distance sectors.
  bool     hasSectors() const { return lastLapHasSectors_; }   // last completed lap
  int32_t  lastSectorDeltaMs(int k) const { return lastSectorDelta_[k]; }
  uint32_t lastSectorMs(int k) const { return lastLapSectorMs_[k]; }
  const uint32_t* bestSectors() const { return bestSectorMs_; }
  uint32_t theoreticalBestMs() const;

 private:
  bool crossed(const GpsFix& a, const GpsFix& b, float& tOut) const;
  void toLocal(double lat, double lon, float& x, float& y) const;
  void traceAppend(float d, uint32_t ms, float spdKmh, const LapChannels* ch);
  void traceRestart(float initialDist, uint32_t initialElapsed, float spdKmh,
                    const LapChannels* ch);
  void advancePoint(float d, uint32_t elapsed, float spdKmh, const LapChannels* ch);
  void adoptReference();
  void computeBoundaries();
  void finishLapSectors(uint32_t lapMs);
  void newSessionReset();
  uint32_t refTimeAtDist(float d);

  StartLine line_;
  float dirX_ = 0, dirY_ = 1;                       // unit vector of the crossing heading
  float mPerDegLat_ = 111194.9f, mPerDegLon_ = 111194.9f;

  GpsFix prev_;
  bool   hasPrev_ = false;

  // Session
  bool     timing_ = false;
  int      sessionIdx_ = 1;
  uint32_t lastCrossMsOfDay_ = 0;   // GPS time of the last crossing (interpolated)
  uint32_t lastCrossLocalMs_ = 0;   // millis() equivalent, for the display
  Lap      laps_[MAX_LAPS];
  int      lapCount_ = 0;
  uint32_t lastLapMs_ = 0;
  float    lastLapMaxSpeed_ = 0;
  uint32_t bestMs_ = 0;
  int      bestIdx_ = -1;
  uint32_t sessionTotalMs_ = 0;
  float    curMaxSpeed_ = 0, sessMaxSpeed_ = 0;
  float    distToLine_ = NAN;

  // Track records
  uint32_t allTimeBest_ = 0;
  int32_t  lastDeltaBest_ = 0;
  bool     lastLapWasRecord_ = false;
  bool     newAllTimeBest_ = false;

  // Traces / predictive delta. cur_ and last_ swap buffers at each lap
  // completion, so the previous lap stays available for comparison export.
  Trace    bufA_, bufB_, ref_;
  Trace*   cur_ = &bufA_;
  Trace*   last_ = &bufB_;
  bool     hasLast_ = false;
  float    curDist_ = 0;      // distance ridden in the current lap
  uint16_t refWalk_ = 0;      // walking index into ref_ (monotonic per lap)
  int32_t  predDelta_ = 0;
  bool     predValid_ = false;

  // Sectors
  float    secBoundary_[NUM_SECTORS > 1 ? NUM_SECTORS - 1 : 1] = {0};
  float    refTotalDist_ = 0;
  int      secIdx_ = 0;                    // next boundary to cross
  uint32_t lastSplitElapsed_ = 0;
  float    prevPtDist_ = 0;
  uint32_t prevPtElapsed_ = 0;
  uint32_t curSectorMs_[NUM_SECTORS] = {0};
  uint32_t lastLapSectorMs_[NUM_SECTORS] = {0};
  int32_t  lastSectorDelta_[NUM_SECTORS] = {0};
  uint32_t bestSectorMs_[NUM_SECTORS] = {0};
  bool     lastLapHasSectors_ = false;
  bool     sectorsImproved_ = false;
};

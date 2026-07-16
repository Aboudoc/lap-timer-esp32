#pragma once
#include <Arduino.h>
#include "config.h"
#include "gps.h"

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

class LapTimer {
 public:
  void setLine(const StartLine& line);  // also resets the session
  const StartLine& line() const { return line_; }

  // Feed a new fix; returns true when a lap was just completed.
  bool onFix(const GpsFix& fix);
  void resetSession();

  bool     timing() const { return timing_; }
  int      lapCount() const { return lapCount_; }
  const Lap& lap(int i) const { return laps_[i]; }  // i < min(lapCount, MAX_LAPS)
  int      storedLaps() const { return lapCount_ < MAX_LAPS ? lapCount_ : MAX_LAPS; }
  uint32_t lastLapMs() const { return lastLapMs_; }
  float    lastLapMaxSpeed() const { return lastLapMaxSpeed_; }
  uint32_t bestLapMs() const { return bestMs_; }
  int      bestLapIndex() const { return bestIdx_; }
  int32_t  lastDeltaBestMs() const { return lastDeltaBest_; }  // last lap vs previous best
  uint32_t currentLapMs(uint32_t nowMs) const { return timing_ ? nowMs - lastCrossLocalMs_ : 0; }
  float    sessionMaxSpeed() const { return sessMaxSpeed_; }
  uint32_t lastCrossLocalMs() const { return lastCrossLocalMs_; }
  uint32_t lastCrossMsOfDay() const { return lastCrossMsOfDay_; }
  float    distToLineM() const { return distToLine_; }

 private:
  bool crossed(const GpsFix& a, const GpsFix& b, float& tOut) const;
  void toLocal(double lat, double lon, float& x, float& y) const;

  StartLine line_;
  float dirX_ = 0, dirY_ = 1;                       // unit vector of the crossing heading
  float mPerDegLat_ = 111194.9f, mPerDegLon_ = 111194.9f;

  GpsFix prev_;
  bool   hasPrev_ = false;

  bool     timing_ = false;
  uint32_t lastCrossMsOfDay_ = 0;   // GPS time of the last crossing (interpolated)
  uint32_t lastCrossLocalMs_ = 0;   // millis() equivalent, for the display
  Lap      laps_[MAX_LAPS];
  int      lapCount_ = 0;
  uint32_t lastLapMs_ = 0;
  float    lastLapMaxSpeed_ = 0;
  uint32_t bestMs_ = 0;
  int      bestIdx_ = -1;
  int32_t  lastDeltaBest_ = 0;
  float    curMaxSpeed_ = 0, sessMaxSpeed_ = 0;
  float    distToLine_ = NAN;
};

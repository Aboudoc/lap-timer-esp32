#pragma once
#include <Arduino.h>
#include "config.h"
#include "gps.h"

// Ligne de depart/arrivee : un point + le cap de franchissement.
// La "porte" est un segment de 2 x halfWidthM, centre sur (lat, lon),
// perpendiculaire au cap. Un tour = franchir la porte dans le bon sens.
struct StartLine {
  double lat = 0, lon = 0;
  float  headingDeg = 0;                    // cap attendu au franchissement
  float  halfWidthM = LINE_HALF_WIDTH_M;
  bool   isSet = false;
};

struct Lap {
  uint32_t ms = 0;
  float    maxSpeedKmh = 0;
};

// Formate un temps au tour : "1:23.4" (centis=false) ou "1:23.45" (centis=true).
void fmtLapTime(char* buf, size_t n, uint32_t ms, bool centis);

class LapTimer {
 public:
  void setLine(const StartLine& line);  // remet aussi la session a zero
  const StartLine& line() const { return line_; }

  // Injecte un nouveau fix ; retourne true si un tour vient d'etre boucle.
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
  int32_t  lastDeltaBestMs() const { return lastDeltaBest_; }  // dernier tour vs meilleur d'avant
  uint32_t currentLapMs(uint32_t nowMs) const { return timing_ ? nowMs - lastCrossLocalMs_ : 0; }
  float    sessionMaxSpeed() const { return sessMaxSpeed_; }
  uint32_t lastCrossLocalMs() const { return lastCrossLocalMs_; }
  uint32_t lastCrossMsOfDay() const { return lastCrossMsOfDay_; }
  float    distToLineM() const { return distToLine_; }

 private:
  bool crossed(const GpsFix& a, const GpsFix& b, float& tOut) const;
  void toLocal(double lat, double lon, float& x, float& y) const;

  StartLine line_;
  float dirX_ = 0, dirY_ = 1;                       // vecteur unitaire du cap de franchissement
  float mPerDegLat_ = 111194.9f, mPerDegLon_ = 111194.9f;

  GpsFix prev_;
  bool   hasPrev_ = false;

  bool     timing_ = false;
  uint32_t lastCrossMsOfDay_ = 0;   // heure GPS du dernier franchissement (interpolee)
  uint32_t lastCrossLocalMs_ = 0;   // equivalent en millis() pour l'affichage
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

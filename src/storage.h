#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include <FS.h>
#include "laptimer.h"

// Everything the device knows about a stored track.
struct TrackMeta {
  uint8_t   id = 0;
  char      name[16] = {0};
  StartLine line;
  uint32_t  bestMs = 0;                       // all-time best on this track
  uint32_t  bestSectors[NUM_SECTORS] = {0};   // all-time best sectors
  uint16_t  traceN = 0;                       // reference-trace samples on file
};

// Persistence:
//  - LittleFS /tracks/T<id>.trk : one file per track (line, records, and the
//    reference-lap trace used by the predictive delta)
//  - LittleFS /laps.csv         : log of every completed lap
//  - NVS (Preferences)          : pit-mode flag
class Storage {
 public:
  void begin();

  // ---- Tracks ----
  int  listTracks(TrackMeta* out, int maxOut);
  bool loadTrackMeta(uint8_t id, TrackMeta& m);
  bool loadTrackTrace(uint8_t id, float* dist, uint32_t* tMs, uint16_t& n, uint16_t maxN);
  bool saveTrack(const TrackMeta& m, const float* dist, const uint32_t* tMs, uint16_t n);
  bool updateTrackMeta(const TrackMeta& m);   // rewrite the header, keep the trace
  int  createTrack(const StartLine& line, const char* name);  // nullptr name -> "Track <id>"
  bool deleteTrack(uint8_t id);
  int  nearestTrack(double lat, double lon, float maxKm);     // -1 if none in range

  // ---- Lap log ----
  void appendLap(const char* dateStr, uint32_t crossMsOfDay, const char* track,
                 int session, int lapIdx, uint32_t lapMs, float maxKmh,
                 float leanMaxDeg);
  File openCsvRead();
  void dumpCsv(Stream& out);
  void clearCsv();

  // ---- Pit mode flag (survives the reboot into/out of WiFi mode) ----
  bool pitFlag();
  void setPitFlag(bool on);

  // ---- IMU calibration (5 floats: gyro biases + accel level offsets) ----
  bool loadImuCal(float out[5]);
  void saveImuCal(const float in[5]);

 private:
  void trackPath(char* buf, size_t n, uint8_t id);
  Preferences prefs_;
};

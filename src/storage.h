#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "laptimer.h"

// Persistence:
//  - NVS (Preferences): start line + all-time best -> survive power-off
//  - LittleFS (/laps.csv): log of every completed lap
class Storage {
 public:
  void begin();
  bool loadLine(StartLine& line);
  void saveLine(const StartLine& line);
  uint32_t bestEverMs();
  void saveBestEver(uint32_t ms);
  void clearBestEver();
  void appendLap(const char* dateStr, uint32_t crossMsOfDay, int lapIdx,
                 uint32_t lapMs, float maxKmh);
  void dumpCsv(Stream& out);
  void clearCsv();

 private:
  Preferences prefs_;
};

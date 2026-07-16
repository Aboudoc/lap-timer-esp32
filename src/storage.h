#pragma once
#include <Arduino.h>
#include <Preferences.h>
#include "laptimer.h"

// Persistance :
//  - NVS (Preferences) : ligne de depart + record absolu -> survivent a l'extinction
//  - LittleFS (/laps.csv) : journal de tous les tours boucles
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

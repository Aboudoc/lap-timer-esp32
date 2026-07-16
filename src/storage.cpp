#include "storage.h"
#include <LittleFS.h>

static const char* kCsvPath = "/laps.csv";

void Storage::begin() {
  prefs_.begin("laptimer", false);
  if (!LittleFS.begin(true)) {  // true = formate au premier demarrage
    Serial.println("[FS] erreur d'initialisation LittleFS");
  }
}

bool Storage::loadLine(StartLine& line) {
  if (!prefs_.getBool("set", false)) return false;
  line.lat        = prefs_.getDouble("lat", 0);
  line.lon        = prefs_.getDouble("lon", 0);
  line.headingDeg = prefs_.getFloat("hdg", 0);
  line.halfWidthM = prefs_.getFloat("hw", LINE_HALF_WIDTH_M);
  line.isSet      = true;
  return true;
}

void Storage::saveLine(const StartLine& line) {
  prefs_.putDouble("lat", line.lat);
  prefs_.putDouble("lon", line.lon);
  prefs_.putFloat("hdg", line.headingDeg);
  prefs_.putFloat("hw", line.halfWidthM);
  prefs_.putBool("set", true);
}

uint32_t Storage::bestEverMs() { return prefs_.getUInt("best", 0); }
void Storage::saveBestEver(uint32_t ms) { prefs_.putUInt("best", ms); }
void Storage::clearBestEver() { prefs_.remove("best"); }

void Storage::appendLap(const char* dateStr, uint32_t crossMsOfDay, int lapIdx,
                        uint32_t lapMs, float maxKmh) {
  File f = LittleFS.open(kCsvPath, FILE_APPEND);
  if (!f) return;
  if (f.size() == 0) f.println("date,heure_utc,tour,temps_s,vmax_kmh");
  unsigned long sec = crossMsOfDay / 1000UL;
  f.printf("%s,%02lu:%02lu:%02lu.%03lu,%d,%lu.%03lu,%.1f\n",
           dateStr, sec / 3600UL, (sec / 60UL) % 60UL, sec % 60UL,
           (unsigned long)(crossMsOfDay % 1000UL),
           lapIdx,
           (unsigned long)(lapMs / 1000UL), (unsigned long)(lapMs % 1000UL),
           maxKmh);
  f.close();
}

void Storage::dumpCsv(Stream& out) {
  File f = LittleFS.open(kCsvPath, FILE_READ);
  if (!f) {
    out.println("(journal vide)");
    return;
  }
  uint8_t buf[128];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    out.write(buf, n);
  }
  f.close();
}

void Storage::clearCsv() {
  LittleFS.remove(kCsvPath);
}

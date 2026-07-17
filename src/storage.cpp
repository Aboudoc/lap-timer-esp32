#include "storage.h"
#include <LittleFS.h>
#include <math.h>

static const char* kCsvPath = "/laps.csv";
static const char* kCsvHeader =
    "date,time_utc,track,session,lap,time_s,vmax_kmh,lean_max_deg,tire_f_c,tire_r_c,"
    "s1_s,s2_s,s3_s,brake_g,thr_avg_pct,rpm_max";

// On-flash track file layout: header, then the trace arrays sequentially
// (dist, tMs, and since LTK2: spd, lean, rpm, thr), each traceN long.
struct TrackFileHeader {
  uint32_t magic;
  char     name[16];
  double   lat, lon;
  float    headingDeg, halfWidthM;
  uint32_t bestMs;
  uint32_t bestSectors[NUM_SECTORS];
  uint16_t traceN;
  uint16_t reserved;
} __attribute__((packed));

static const uint32_t kTrackMagic = 0x314B544CUL;    // "LTK1" (no channels)
static const uint32_t kTrackMagicV2 = 0x324B544CUL;  // "LTK2" (with channels)

void Storage::trackPath(char* buf, size_t n, uint8_t id) {
  snprintf(buf, n, "/tracks/T%u.trk", (unsigned)id);
}

static void metaToHeader(const TrackMeta& m, TrackFileHeader& h) {
  memset(&h, 0, sizeof(h));
  h.magic = kTrackMagicV2;
  strncpy(h.name, m.name, sizeof(h.name) - 1);
  h.lat = m.line.lat;
  h.lon = m.line.lon;
  h.headingDeg = m.line.headingDeg;
  h.halfWidthM = m.line.halfWidthM;
  h.bestMs = m.bestMs;
  memcpy(h.bestSectors, m.bestSectors, sizeof(h.bestSectors));
  h.traceN = m.traceN;
}

static void headerToMeta(const TrackFileHeader& h, uint8_t id, TrackMeta& m) {
  m.id = id;
  memcpy(m.name, h.name, sizeof(m.name));
  m.name[sizeof(m.name) - 1] = 0;
  m.line.lat = h.lat;
  m.line.lon = h.lon;
  m.line.headingDeg = h.headingDeg;
  m.line.halfWidthM = h.halfWidthM;
  m.line.isSet = true;
  m.bestMs = h.bestMs;
  memcpy(m.bestSectors, h.bestSectors, sizeof(m.bestSectors));
  m.traceN = h.traceN;
}

static bool readHeader(File& f, TrackFileHeader& h) {
  return f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) &&
         (h.magic == kTrackMagic || h.magic == kTrackMagicV2);
}

void Storage::begin() {
  prefs_.begin("laptimer", false);
  if (!LittleFS.begin(true)) {  // true = format on first boot
    Serial.println("[FS] LittleFS init error");
    return;
  }
  LittleFS.mkdir("/tracks");

  // Older CSV schema? Keep the file aside and start fresh.
  File f = LittleFS.open(kCsvPath, FILE_READ);
  if (f) {
    String first = f.readStringUntil('\n');
    f.close();
    first.trim();
    if (first.length() && first != kCsvHeader) {
      LittleFS.remove("/laps_old.csv");
      LittleFS.rename(kCsvPath, "/laps_old.csv");
      Serial.println("[FS] old lap log moved to /laps_old.csv");
    }
  }
}

int Storage::listTracks(TrackMeta* out, int maxOut) {
  File dir = LittleFS.open("/tracks");
  if (!dir || !dir.isDirectory()) return 0;
  int cnt = 0;
  File f;
  while ((f = dir.openNextFile()) && cnt < maxOut) {
    const char* base = strrchr(f.name(), '/');
    base = base ? base + 1 : f.name();
    unsigned id;
    TrackFileHeader h;
    if (sscanf(base, "T%u.trk", &id) == 1 && readHeader(f, h)) {
      headerToMeta(h, (uint8_t)id, out[cnt]);
      cnt++;
    }
    f.close();
  }
  dir.close();
  return cnt;
}

bool Storage::loadTrackMeta(uint8_t id, TrackMeta& m) {
  char path[24];
  trackPath(path, sizeof(path), id);
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;
  TrackFileHeader h;
  bool ok = readHeader(f, h);
  f.close();
  if (ok) headerToMeta(h, id, m);
  return ok;
}

bool Storage::loadTrackTrace(uint8_t id, LapTimer::Trace* t) {
  t->n = 0;
  char path[24];
  trackPath(path, sizeof(path), id);
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;
  TrackFileHeader h;
  if (!readHeader(f, h)) {
    f.close();
    return false;
  }
  uint16_t n = h.traceN < TRACE_MAX_SAMPLES ? h.traceN : TRACE_MAX_SAMPLES;
  bool ok = f.read((uint8_t*)t->dist, n * 4) == (size_t)n * 4 &&
            f.read((uint8_t*)t->tMs, n * 4) == (size_t)n * 4;
  if (ok && h.magic == kTrackMagicV2) {
    ok = f.read((uint8_t*)t->spd, n) == n &&
         f.read((uint8_t*)t->lean, n) == n &&
         f.read((uint8_t*)t->rpm, n * 2) == (size_t)n * 2 &&
         f.read((uint8_t*)t->thr, n) == n;
  } else if (ok) {
    memset(t->spd, 0, n);
    memset(t->lean, 0, n);
    memset(t->rpm, 0, n * 2);
    memset(t->thr, 255, n);
  }
  f.close();
  if (ok) t->n = n;
  return ok;
}

bool Storage::saveTrack(const TrackMeta& m, const LapTimer::Trace* t) {
  char path[24];
  trackPath(path, sizeof(path), m.id);
  File f = LittleFS.open(path, FILE_WRITE);  // truncates
  if (!f) return false;
  uint16_t n = t ? t->n : 0;
  TrackFileHeader h;
  metaToHeader(m, h);
  h.traceN = n;
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h);
  if (ok && n) {
    ok = f.write((const uint8_t*)t->dist, n * 4) == (size_t)n * 4 &&
         f.write((const uint8_t*)t->tMs, n * 4) == (size_t)n * 4 &&
         f.write((const uint8_t*)t->spd, n) == n &&
         f.write((const uint8_t*)t->lean, n) == n &&
         f.write((const uint8_t*)t->rpm, n * 2) == (size_t)n * 2 &&
         f.write((const uint8_t*)t->thr, n) == n;
  }
  f.close();
  return ok;
}

bool Storage::updateTrackMeta(const TrackMeta& m) {
  char path[24];
  trackPath(path, sizeof(path), m.id);
  File f = LittleFS.open(path, "r+");
  if (!f) return saveTrack(m, nullptr);
  TrackFileHeader old;
  if (!readHeader(f, old)) {
    f.close();
    return saveTrack(m, nullptr);
  }
  TrackFileHeader h;
  metaToHeader(m, h);
  h.magic = old.magic;    // keep the on-file trace layout as it is
  h.traceN = old.traceN;
  f.seek(0, SeekSet);
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h);
  f.close();
  return ok;
}

int Storage::createTrack(const StartLine& line, const char* name) {
  char path[24];
  int id = -1;
  for (int i = 1; i <= MAX_TRACKS; i++) {
    trackPath(path, sizeof(path), (uint8_t)i);
    if (!LittleFS.exists(path)) {
      id = i;
      break;
    }
  }
  if (id < 0) return -1;
  TrackMeta m;
  m.id = (uint8_t)id;
  if (name && name[0]) {
    strncpy(m.name, name, sizeof(m.name) - 1);
  } else {
    snprintf(m.name, sizeof(m.name), "Track %d", id);
  }
  m.line = line;
  return saveTrack(m, nullptr) ? id : -1;
}

bool Storage::deleteTrack(uint8_t id) {
  char path[24];
  trackPath(path, sizeof(path), id);
  return LittleFS.remove(path);
}

int Storage::nearestTrack(double lat, double lon, float maxKm) {
  TrackMeta list[MAX_TRACKS];
  int cnt = listTracks(list, MAX_TRACKS);
  int best = -1;
  float bestM = maxKm * 1000.0f;
  for (int i = 0; i < cnt; i++) {
    float mLon = 111194.9f * cosf((float)(lat * 0.017453292519943295));
    float dx = (float)((lon - list[i].line.lon) * mLon);
    float dy = (float)((lat - list[i].line.lat) * 111194.9);
    float d = sqrtf(dx * dx + dy * dy);
    if (d < bestM) {
      bestM = d;
      best = list[i].id;
    }
  }
  return best;
}

void Storage::appendLap(const char* dateStr, uint32_t crossMsOfDay, const char* track,
                        int session, int lapIdx, uint32_t lapMs, float maxKmh,
                        float leanMaxDeg, float tireFrontC, float tireRearC,
                        const float sectorsS[NUM_SECTORS],
                        float brakeG, float thrAvgPct, int rpmMax) {
  File f = LittleFS.open(kCsvPath, FILE_APPEND);
  if (!f) return;
  if (f.size() == 0) f.println(kCsvHeader);
  unsigned long sec = crossMsOfDay / 1000UL;
  f.printf("%s,%02lu:%02lu:%02lu.%03lu,%s,%d,%d,%lu.%03lu,%.1f,%.0f,%.0f,%.0f,"
           "%.2f,%.2f,%.2f,%.2f,%.0f,%d\n",
           dateStr, sec / 3600UL, (sec / 60UL) % 60UL, sec % 60UL,
           (unsigned long)(crossMsOfDay % 1000UL),
           track, session, lapIdx,
           (unsigned long)(lapMs / 1000UL), (unsigned long)(lapMs % 1000UL),
           maxKmh, leanMaxDeg, tireFrontC, tireRearC,
           sectorsS[0], sectorsS[1], sectorsS[2],
           brakeG, thrAvgPct, rpmMax);
  f.close();
}

File Storage::openCsvRead() {
  return LittleFS.open(kCsvPath, FILE_READ);
}

void Storage::dumpCsv(Stream& out) {
  File f = openCsvRead();
  if (!f) {
    out.println("(log empty)");
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

bool Storage::pitFlag() { return prefs_.getBool("pit", false); }
void Storage::setPitFlag(bool on) { prefs_.putBool("pit", on); }

bool Storage::loadImuCal(float out[5]) {
  return prefs_.getBytes("imucal", out, 5 * sizeof(float)) == 5 * sizeof(float);
}

void Storage::saveImuCal(const float in[5]) {
  prefs_.putBytes("imucal", in, 5 * sizeof(float));
}

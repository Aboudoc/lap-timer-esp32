#include "storage.h"
#include <LittleFS.h>
#include <math.h>

static const char* kCsvPath = "/laps.csv";
static const char* kCsvHeader =
    "date,time_utc,track,session,lap,time_s,vmax_kmh,lean_max_deg,tire_f_c,tire_r_c,"
    "s1_s,s2_s,s3_s";

// On-flash track file layout: header then dist[traceN] then tMs[traceN].
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

static const uint32_t kTrackMagic = 0x314B544CUL;  // "LTK1"

void Storage::trackPath(char* buf, size_t n, uint8_t id) {
  snprintf(buf, n, "/tracks/T%u.trk", (unsigned)id);
}

static void metaToHeader(const TrackMeta& m, TrackFileHeader& h) {
  memset(&h, 0, sizeof(h));
  h.magic = kTrackMagic;
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
  return f.read((uint8_t*)&h, sizeof(h)) == sizeof(h) && h.magic == kTrackMagic;
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

bool Storage::loadTrackTrace(uint8_t id, float* dist, uint32_t* tMs, uint16_t& n, uint16_t maxN) {
  n = 0;
  char path[24];
  trackPath(path, sizeof(path), id);
  File f = LittleFS.open(path, FILE_READ);
  if (!f) return false;
  TrackFileHeader h;
  if (!readHeader(f, h)) {
    f.close();
    return false;
  }
  uint16_t want = h.traceN < maxN ? h.traceN : maxN;
  bool ok = f.read((uint8_t*)dist, want * sizeof(float)) == want * sizeof(float);
  if (ok && h.traceN > want) f.seek((h.traceN - want) * sizeof(float), SeekCur);
  ok = ok && f.read((uint8_t*)tMs, want * sizeof(uint32_t)) == want * sizeof(uint32_t);
  f.close();
  if (ok) n = want;
  return ok;
}

bool Storage::saveTrack(const TrackMeta& m, const float* dist, const uint32_t* tMs, uint16_t n) {
  char path[24];
  trackPath(path, sizeof(path), m.id);
  File f = LittleFS.open(path, FILE_WRITE);  // truncates
  if (!f) return false;
  TrackFileHeader h;
  metaToHeader(m, h);
  h.traceN = n;
  bool ok = f.write((const uint8_t*)&h, sizeof(h)) == sizeof(h);
  if (ok && n) {
    ok = f.write((const uint8_t*)dist, n * sizeof(float)) == n * sizeof(float) &&
         f.write((const uint8_t*)tMs, n * sizeof(uint32_t)) == n * sizeof(uint32_t);
  }
  f.close();
  return ok;
}

bool Storage::updateTrackMeta(const TrackMeta& m) {
  char path[24];
  trackPath(path, sizeof(path), m.id);
  File f = LittleFS.open(path, "r+");
  if (!f) return saveTrack(m, nullptr, nullptr, 0);
  TrackFileHeader old;
  if (!readHeader(f, old)) {
    f.close();
    return saveTrack(m, nullptr, nullptr, 0);
  }
  TrackFileHeader h;
  metaToHeader(m, h);
  h.traceN = old.traceN;  // the trace on file stays as it is
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
  return saveTrack(m, nullptr, nullptr, 0) ? id : -1;
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
                        const float sectorsS[NUM_SECTORS]) {
  File f = LittleFS.open(kCsvPath, FILE_APPEND);
  if (!f) return;
  if (f.size() == 0) f.println(kCsvHeader);
  unsigned long sec = crossMsOfDay / 1000UL;
  f.printf("%s,%02lu:%02lu:%02lu.%03lu,%s,%d,%d,%lu.%03lu,%.1f,%.0f,%.0f,%.0f,"
           "%.2f,%.2f,%.2f\n",
           dateStr, sec / 3600UL, (sec / 60UL) % 60UL, sec % 60UL,
           (unsigned long)(crossMsOfDay % 1000UL),
           track, session, lapIdx,
           (unsigned long)(lapMs / 1000UL), (unsigned long)(lapMs % 1000UL),
           maxKmh, leanMaxDeg, tireFrontC, tireRearC,
           sectorsS[0], sectorsS[1], sectorsS[2]);
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

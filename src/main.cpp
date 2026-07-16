// GPS lap timer — ESP32 + NEO-6M + SSD1306 OLED
// Ninja 400 / MSP Bangkok track days
//
// Buttons (on-board BOOT or external button on GPIO25):
//   short press: next page (RACE -> SESSION -> GPS -> LINE)
//   long press : SESSION = reset the session
//                GPS     = toggle pit mode (WiFi hotspot + web app, reboots)
//                LINE    = set the start line here (while riding)
//
// Serial commands (115200 baud): type 'h' for help.

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "gps.h"
#include "laptimer.h"
#include "display.h"
#include "storage.h"
#include "button.h"
#include "ble_racechrono.h"
#include "bt_nmea.h"
#include "pit.h"

static const char* VERSION = "v1.3";

GpsModule gpsMod;
LapTimer  lapTimer;
Display   display;
Storage   storage;
Button    btnBoot, btnExt;
PitMode   pit;
#if BT_MODE == BT_MODE_RACECHRONO
BleRaceChrono btLink;  // RaceChrono app, over BLE
#elif BT_MODE == BT_MODE_NMEA
BtNmea btLink;         // generic Bluetooth NMEA GPS (TrackAddict & co, Android)
#endif

Page     page = Page::Race;
bool     pitActive = false;
int      activeTrackId = -1;
char     activeTrackName[16] = "--";
bool     trackAutoChecked = false;
uint32_t lastRenderMs = 0;
uint32_t lastButtonMs = 0;
uint32_t lastActivityMs = 0;  // buttons or movement, for screen dimming

void handleLapDone();
void handleButton(ButtonEvent ev);
void handleSerial();
void trySetLineHere();
void applyTrack(int id);
void persistActiveTrack(bool withTrace);
void togglePitMode();

// ==================== Simulation mode ====================
// Virtual circular track (~39-44 s per lap) to test the display, buttons
// and timing logic without a GPS. See the "esp32dev-sim" environment.
#ifdef SIMULATE_GPS
static GpsFix   simFix;
static uint32_t simLastMs = 0;
static float    simTheta = 0;
constexpr double SIM_LAT0 = 13.7563;
constexpr double SIM_LON0 = 100.5018;
constexpr float  SIM_R = 250.0f;  // radius in meters

void simSetup() {
  StartLine l;
  l.lat = SIM_LAT0 + SIM_R / 111194.9;  // north point of the circle
  l.lon = SIM_LON0;
  l.headingDeg = 90;                    // crossed heading due east
  l.isSet = true;
  lapTimer.setLine(l);
}

void simLoop() {
  uint32_t now = millis();
  if (now - simLastMs < GPS_MEAS_RATE_MS) return;
  simLastMs = now;

  float v = 36.0f + 6.0f * sinf(now / 9000.0f);              // m/s, varies lap to lap
  simTheta += (v / SIM_R) * (GPS_MEAS_RATE_MS / 1000.0f);

  simFix.lat = SIM_LAT0 + (SIM_R * cos(simTheta)) / 111194.9;
  simFix.lon = SIM_LON0 + (SIM_R * sin(simTheta)) / (111194.9 * cos(SIM_LAT0 * DEG_TO_RAD));
  simFix.speedKmh  = v * 3.6f;
  simFix.courseDeg = fmodf(90.0f + simTheta * (float)RAD_TO_DEG, 360.0f);
  simFix.altM      = 5.0f;
  simFix.altValid  = true;
  simFix.msOfDay   = now % MS_PER_DAY;
  simFix.localMs   = now;
  simFix.valid     = true;

  if (lapTimer.onFix(simFix)) handleLapDone();
#if BT_MODE != BT_MODE_OFF
  if (!pitActive) btLink.onFix(simFix, 12, 0.8f, true, 2026, 1, 1);
#endif
}
#endif
// ==========================================================

static const GpsFix& currentFix() {
#ifdef SIMULATE_GPS
  return simFix;
#else
  return gpsMod.fix();
#endif
}

static float approxDistM(double lat1, double lon1, double lat2, double lon2) {
  float mLon = 111194.9f * cosf((float)(lat1 * DEG_TO_RAD));
  float dx = (float)((lon2 - lon1) * mLon);
  float dy = (float)((lat2 - lat1) * 111194.9);
  return sqrtf(dx * dx + dy * dy);
}

#ifndef SIMULATE_GPS
static void onNewFix() {
  if (lapTimer.onFix(gpsMod.fix())) handleLapDone();
#if BT_MODE != BT_MODE_OFF
  if (!pitActive) {
    int y = 0, mo = 0, dd = 0;
    gpsMod.dateYmd(y, mo, dd);
    btLink.onFix(gpsMod.fix(), gpsMod.sats(), gpsMod.hdop(), gpsMod.hasFix(), y, mo, dd);
  }
#endif
}
#endif

static GpsView buildGpsView() {
  GpsView v;
#ifdef SIMULATE_GPS
  v.hasFix = simFix.valid;
  v.sats = 12;
  v.hdop = 0.8f;
  v.rateHz = 1000.0f / GPS_MEAS_RATE_MS;
  v.baud = 0;
  v.lat = simFix.lat;
  v.lon = simFix.lon;
  v.speedKmh = simFix.speedKmh;
  v.fixAgeMs = millis() - simFix.localMs;
  v.sim = true;
#else
  v.hasFix = gpsMod.hasFix();
  v.sats = gpsMod.sats();
  v.hdop = gpsMod.hdop();
  v.rateHz = gpsMod.measuredRateHz();
  v.baud = gpsMod.baud();
  v.lat = gpsMod.fix().lat;
  v.lon = gpsMod.fix().lon;
  v.speedKmh = gpsMod.lastSpeedKmh();
  v.fixAgeMs = gpsMod.fixAgeMs();
#endif
  v.wifi = pitActive;
#if BT_MODE != BT_MODE_OFF
  if (!pitActive) v.ble = btLink.connected();
#endif
  return v;
}

// ==================== Tracks ====================

void applyTrack(int id) {
  TrackMeta m;
  if (!storage.loadTrackMeta((uint8_t)id, m)) return;
  lapTimer.setLine(m.line);
  uint16_t n = 0;
  if (m.traceN &&
      storage.loadTrackTrace((uint8_t)id, lapTimer.refDistBuffer(),
                             lapTimer.refTimeBuffer(), n, TRACE_MAX_SAMPLES)) {
    lapTimer.commitReference(n);
  }
  lapTimer.setAllTimeBest(m.bestMs);
  lapTimer.setBestSectors(m.bestSectors);
  activeTrackId = id;
  strlcpy(activeTrackName, m.name, sizeof(activeTrackName));
  char b[24];
  snprintf(b, sizeof(b), "TRACK %s", m.name);
  display.notify(b);
  Serial.printf("[TRACK] active: %d \"%s\", best %lu ms, trace %u pts\n",
                id, m.name, (unsigned long)m.bestMs, n);
}

void persistActiveTrack(bool withTrace) {
  if (activeTrackId < 0) return;
  TrackMeta m;
  m.id = (uint8_t)activeTrackId;
  strlcpy(m.name, activeTrackName, sizeof(m.name));
  m.line = lapTimer.line();
  m.bestMs = lapTimer.allTimeBestMs();
  memcpy(m.bestSectors, lapTimer.bestSectors(), sizeof(m.bestSectors));
  m.traceN = lapTimer.refN();
  if (withTrace) {
    storage.saveTrack(m, lapTimer.refDist(), lapTimer.refTms(), lapTimer.refN());
  } else {
    storage.updateTrackMeta(m);
  }
}

// Pit-mode web callbacks.
static void pitSelectTrack(uint8_t id) { applyTrack(id); }
static void pitRenameTrack(uint8_t id, const char* name) {
  TrackMeta m;
  if (!storage.loadTrackMeta(id, m) || !name || !name[0]) return;
  strlcpy(m.name, name, sizeof(m.name));
  storage.updateTrackMeta(m);
  if ((int)id == activeTrackId) strlcpy(activeTrackName, m.name, sizeof(activeTrackName));
}
static void pitDeleteTrack(uint8_t id) {
  storage.deleteTrack(id);
  if ((int)id == activeTrackId) {
    activeTrackId = -1;
    strlcpy(activeTrackName, "--", sizeof(activeTrackName));
    lapTimer.setLine(StartLine{});  // isSet = false
  }
}
static const char* pitActiveName() { return activeTrackName; }
static uint32_t pitActiveBest() { return lapTimer.allTimeBestMs(); }

// ================================================

void setup() {
  storage.begin();
  pitActive = storage.pitFlag();
#if BT_MODE == BT_MODE_NMEA
  setCpuFrequencyMhz(160);  // Bluetooth Classic needs more headroom than BLE
#else
  setCpuFrequencyMhz(pitActive ? 160 : 80);
#endif
  Serial.begin(SERIAL_BAUD);

  display.begin();
  display.splash(VERSION);

  btnBoot.begin(PIN_BTN_BOOT);
  btnExt.begin(PIN_BTN_EXT);

#ifdef SIMULATE_GPS
  simSetup();
  Serial.println("[SIM] simulation mode active");
#elif defined(REPLAY_GPS)
  Serial.println("[REPLAY] paste/stream NMEA lines on this port");
#else
  gpsMod.begin(Serial2);
#endif

  if (pitActive) {
    PitActions actions{pitSelectTrack, pitRenameTrack, pitDeleteTrack,
                       pitActiveName, pitActiveBest};
    pit.begin(&storage, actions, VERSION);
    display.notify("WIFI 192.168.4.1", 5000);
  } else {
    WiFi.mode(WIFI_OFF);
#if BT_MODE == BT_MODE_OFF
    btStop();
#else
    btLink.begin(BT_DEVICE_NAME);
#endif
  }

  lastActivityMs = millis();
  Serial.println("Lap timer ready. Type 'h' for help.");
}

void loop() {
  uint32_t now = millis();

  if (pitActive) pit.loop();

#ifdef SIMULATE_GPS
  simLoop();
#elif defined(REPLAY_GPS)
  // fixes arrive through handleSerial()
#else
  if (gpsMod.update()) onNewFix();
#endif

  ButtonEvent ev = btnBoot.poll();
  if (ev == ButtonEvent::None) ev = btnExt.poll();
  if (ev != ButtonEvent::None) {
    lastButtonMs = now;
    lastActivityMs = now;
    handleButton(ev);
  }

  // While timing: automatically fall back to the RACE page after a while.
  if (page != Page::Race && lapTimer.timing() && now - lastButtonMs > PAGE_TIMEOUT_MS) {
    page = Page::Race;
  }

  handleSerial();

  // First valid fix after boot: activate the nearest stored track.
  if (!trackAutoChecked && activeTrackId < 0) {
    const GpsFix& f = currentFix();
    if (f.valid && now - f.localMs < 2000) {
      trackAutoChecked = true;
      int id = storage.nearestTrack(f.lat, f.lon, TRACK_MATCH_KM);
      if (id >= 0) applyTrack(id);
    }
  }

  // Screen dimming when parked.
  if (currentFix().valid && currentFix().speedKmh > DIM_SPEED_KMH) lastActivityMs = now;
  display.setDim(now - lastActivityMs > DIM_AFTER_MS);

  if (now - lastRenderMs >= DISPLAY_PERIOD_MS) {
    lastRenderMs = now;
    GpsView gv = buildGpsView();
    display.render(page, lapTimer, gv, now, activeTrackName);
  }
}

void handleLapDone() {
  uint32_t lapMs = lapTimer.lastLapMs();
  int n = lapTimer.lapCount();

  char date[12];
#ifdef SIMULATE_GPS
  strncpy(date, "sim", sizeof(date));
#else
  gpsMod.dateStr(date, sizeof(date));
#endif
  storage.appendLap(date, lapTimer.lastCrossMsOfDay(), activeTrackName,
                    lapTimer.sessionIndex(), n, lapMs, lapTimer.lastLapMaxSpeed());

  // Persist the track records: full write (with the new reference trace) on
  // an all-time best, header-only when just a sector improved.
  if (lapTimer.newAllTimeBest()) {
    persistActiveTrack(true);
  } else if (lapTimer.sectorsImproved()) {
    persistActiveTrack(false);
  }
  lapTimer.ackPersist();

  char tm[12];
  fmtLapTime(tm, sizeof(tm), lapMs, true);
  Serial.printf("[LAP %d] %s  vmax %.1f km/h%s\n", n, tm, lapTimer.lastLapMaxSpeed(),
                lapTimer.lastLapWasRecord() ? "  RECORD" : "");
}

void togglePitMode() {
  storage.setPitFlag(!pitActive);
  display.notify(pitActive ? "WIFI OFF, REBOOT" : "WIFI ON, REBOOT");
  GpsView gv = buildGpsView();
  display.render(page, lapTimer, gv, millis(), activeTrackName);
  delay(900);
  ESP.restart();
}

void handleButton(ButtonEvent ev) {
  if (ev == ButtonEvent::Short) {
    page = (Page)(((int)page + 1) % (int)Page::COUNT);
    return;
  }
  // Long press: action depends on the page. Nothing on RACE to avoid
  // accidental actions while riding.
  switch (page) {
    case Page::Session:
      lapTimer.resetSession();
      display.notify("SESSION RESET");
      Serial.println("[SESSION] reset");
      break;
    case Page::Gps:
      togglePitMode();
      break;
    case Page::Line:
      trySetLineHere();
      break;
    default:
      break;
  }
}

void trySetLineHere() {
  const GpsFix& f = currentFix();
  if (!f.valid || millis() - f.localMs > 2000) {
    display.notify("NO GPS FIX");
    return;
  }
  if (f.speedKmh < MIN_SETLINE_SPEED_KMH) {
    display.notify("RIDE AT >10 KM/H");
    return;
  }
  StartLine l;
  l.lat = f.lat;
  l.lon = f.lon;
  l.headingDeg = f.courseDeg;
  l.halfWidthM = LINE_HALF_WIDTH_M;
  l.isSet = true;

  // Same physical line as the active track? Move its gate (records reset,
  // the distance base changed). Otherwise create a new track here.
  if (activeTrackId >= 0 && lapTimer.line().isSet &&
      approxDistM(lapTimer.line().lat, lapTimer.line().lon, l.lat, l.lon) < 1000.0f) {
    lapTimer.setLine(l);
    persistActiveTrack(true);
    display.notify("LINE UPDATED");
    Serial.printf("[LINE] updated: %.6f %.6f hdg %.0f\n", l.lat, l.lon, l.headingDeg);
    return;
  }
  int id = storage.createTrack(l, nullptr);
  if (id < 0) {
    display.notify("TRACK LIST FULL");
    return;
  }
  lapTimer.setLine(l);
  TrackMeta m;
  storage.loadTrackMeta((uint8_t)id, m);
  activeTrackId = id;
  strlcpy(activeTrackName, m.name, sizeof(activeTrackName));
  display.notify("LINE SET!");
  Serial.printf("[LINE] new track %d: %.6f %.6f hdg %.0f\n", id, l.lat, l.lon, l.headingDeg);
}

void handleSerial() {
  static char buf[96];
  static size_t len = 0;
#ifdef REPLAY_GPS
  static bool nmeaLine = false;
#endif
  while (Serial.available()) {
    char c = (char)Serial.read();
#ifdef REPLAY_GPS
    // Lines starting with '$' are NMEA and feed the GPS parser directly.
    if (len == 0 && !nmeaLine && c == '$') nmeaLine = true;
    if (nmeaLine) {
      if (gpsMod.processChar(c)) onNewFix();
      if (c == '\n') nmeaLine = false;
      continue;
    }
#endif
    if (c == '\r') continue;
    if (c != '\n') {
      if (len < sizeof(buf) - 1) buf[len++] = c;
      continue;
    }
    buf[len] = 0;
    len = 0;
    if (buf[0] == 0) continue;

    switch (buf[0]) {
      case 'h':
        Serial.println("Commands:");
        Serial.println("  h  help");
        Serial.println("  i  info (track, session, GPS)");
        Serial.println("  T  list tracks / T <id> select a track");
        Serial.println("  N <name>  rename the active track");
        Serial.println("  d  dump the CSV lap log");
        Serial.println("  x  erase the CSV lap log");
        Serial.println("  r  reset the session");
        Serial.println("  z  erase the active track records (best/sectors/reference)");
        Serial.println("  w  toggle pit mode (WiFi hotspot, reboots)");
        Serial.println("  L <lat> <lon> <hdg> [half-width]  set the line manually");
        break;
      case 'i': {
        const StartLine& l = lapTimer.line();
        Serial.printf("Track: %s (id %d)\n", activeTrackName, activeTrackId);
        if (l.isSet) {
          Serial.printf("Line: %.6f %.6f hdg %.0f half-width %.0fm\n",
                        l.lat, l.lon, l.headingDeg, l.halfWidthM);
        } else {
          Serial.println("Line: not set");
        }
        char tm[12];
        fmtLapTime(tm, sizeof(tm), lapTimer.bestLapMs(), true);
        Serial.printf("Session %d: %d laps, best %s, vmax %.1f km/h\n",
                      lapTimer.sessionIndex(), lapTimer.lapCount(),
                      lapTimer.lapCount() ? tm : "-", lapTimer.sessionMaxSpeed());
        fmtLapTime(tm, sizeof(tm), lapTimer.allTimeBestMs(), true);
        Serial.printf("All-time best: %s\n", lapTimer.allTimeBestMs() ? tm : "-");
        fmtLapTime(tm, sizeof(tm), lapTimer.theoreticalBestMs(), true);
        Serial.printf("Theoretical best: %s\n", lapTimer.theoreticalBestMs() ? tm : "-");
        GpsView g = buildGpsView();
        Serial.printf("GPS: fix=%d sats=%d hdop=%.1f %.1fHz %lu baud\n",
                      g.hasFix, g.sats, g.hdop, g.rateHz, (unsigned long)g.baud);
        break;
      }
      case 'T': {
        int id = atoi(buf + 1);
        if (id > 0) {
          applyTrack(id);
        } else {
          TrackMeta list[MAX_TRACKS];
          int cnt = storage.listTracks(list, MAX_TRACKS);
          Serial.printf("%d track(s):\n", cnt);
          char tm[12];
          for (int i = 0; i < cnt; i++) {
            fmtLapTime(tm, sizeof(tm), list[i].bestMs, true);
            Serial.printf(" %c%2u  %-15s best %s\n",
                          (int)list[i].id == activeTrackId ? '*' : ' ',
                          list[i].id, list[i].name, list[i].bestMs ? tm : "-");
          }
        }
        break;
      }
      case 'N': {
        char* p = buf + 1;
        while (*p == ' ') p++;
        if (activeTrackId < 0 || !*p) {
          Serial.println("Usage: N <name> (a track must be active)");
          break;
        }
        pitRenameTrack((uint8_t)activeTrackId, p);
        Serial.printf("Track renamed: %s\n", activeTrackName);
        break;
      }
      case 'd':
        storage.dumpCsv(Serial);
        break;
      case 'x':
        storage.clearCsv();
        Serial.println("CSV lap log erased.");
        break;
      case 'r':
        lapTimer.resetSession();
        Serial.println("Session reset.");
        break;
      case 'z':
        lapTimer.clearTrackData();
        persistActiveTrack(true);
        Serial.println("Active track records erased.");
        break;
      case 'w':
        togglePitMode();
        break;
      case 'L': {
        char* p = buf + 1;
        char* end = nullptr;
        double la = strtod(p, &end); p = end;
        double lo = strtod(p, &end); p = end;
        double hd = strtod(p, &end); p = end;
        double hw = strtod(p, &end);
        if (la == 0 || lo == 0) {
          Serial.println("Usage: L <lat> <lon> <hdg> [half-width]");
          break;
        }
        StartLine l;
        l.lat = la;
        l.lon = lo;
        l.headingDeg = (float)hd;
        l.halfWidthM = hw > 0 ? (float)hw : LINE_HALF_WIDTH_M;
        l.isSet = true;
        int id = storage.createTrack(l, nullptr);
        if (id < 0) {
          Serial.println("Track list full.");
          break;
        }
        lapTimer.setLine(l);
        TrackMeta m;
        storage.loadTrackMeta((uint8_t)id, m);
        activeTrackId = id;
        strlcpy(activeTrackName, m.name, sizeof(activeTrackName));
        Serial.printf("Track %d created: %.6f %.6f hdg %.0f half-width %.0fm\n",
                      id, l.lat, l.lon, l.headingDeg, l.halfWidthM);
        break;
      }
      default:
        Serial.println("Unknown command, 'h' for help.");
        break;
    }
  }
}

// GPS lap timer — ESP32 + NEO-6M + SSD1306 OLED
// Ninja 400 / MSP Bangkok track days
//
// Buttons (on-board BOOT or external button on GPIO25):
//   short press: next page (RACE -> SESSION -> GPS -> LINE)
//   long press : SESSION = reset the session
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

static const char* VERSION = "v1.1";

GpsModule gpsMod;
LapTimer  lapTimer;
Display   display;
Storage   storage;
Button    btnBoot, btnExt;
#if ENABLE_BLE_RACECHRONO
BleRaceChrono ble;
#endif

Page     page = Page::Race;
uint32_t bestEver = 0;
uint32_t lastRenderMs = 0;
uint32_t lastButtonMs = 0;

void handleLapDone();
void handleButton(ButtonEvent ev);
void handleSerial();
void trySetLineHere();

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
#if ENABLE_BLE_RACECHRONO
  ble.onFix(simFix, 12, 0.8f, true, 2026, 1, 1);
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
  return v;
}

void setup() {
  setCpuFrequencyMhz(80);  // no need for more -> saves battery
  Serial.begin(SERIAL_BAUD);
  WiFi.mode(WIFI_OFF);
#if !ENABLE_BLE_RACECHRONO
  btStop();
#endif

  display.begin();
  display.splash(VERSION);

  storage.begin();
  StartLine line;
  if (storage.loadLine(line)) {
    lapTimer.setLine(line);
    Serial.printf("[LINE] loaded: %.6f %.6f hdg %.0f\n", line.lat, line.lon, line.headingDeg);
  }
  bestEver = storage.bestEverMs();

  btnBoot.begin(PIN_BTN_BOOT);
  btnExt.begin(PIN_BTN_EXT);

#ifdef SIMULATE_GPS
  simSetup();
  Serial.println("[SIM] simulation mode active");
#else
  gpsMod.begin(Serial2);
#endif

#if ENABLE_BLE_RACECHRONO
  ble.begin(BLE_DEVICE_NAME);
#endif

  Serial.println("Lap timer ready. Type 'h' for help.");
}

void loop() {
  uint32_t now = millis();

#ifdef SIMULATE_GPS
  simLoop();
#else
  if (gpsMod.update()) {
    if (lapTimer.onFix(gpsMod.fix())) handleLapDone();
#if ENABLE_BLE_RACECHRONO
    int y = 0, mo = 0, dd = 0;
    gpsMod.dateYmd(y, mo, dd);
    ble.onFix(gpsMod.fix(), gpsMod.sats(), gpsMod.hdop(), gpsMod.hasFix(), y, mo, dd);
#endif
  }
#endif

  ButtonEvent ev = btnBoot.poll();
  if (ev == ButtonEvent::None) ev = btnExt.poll();
  if (ev != ButtonEvent::None) {
    lastButtonMs = now;
    handleButton(ev);
  }

  // While timing: automatically fall back to the RACE page after a while.
  if (page != Page::Race && lapTimer.timing() && now - lastButtonMs > PAGE_TIMEOUT_MS) {
    page = Page::Race;
  }

  handleSerial();

  if (now - lastRenderMs >= DISPLAY_PERIOD_MS) {
    lastRenderMs = now;
    GpsView gv = buildGpsView();
#if ENABLE_BLE_RACECHRONO
    gv.ble = ble.connected();
#endif
    display.render(page, lapTimer, gv, now, bestEver);
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
  storage.appendLap(date, lapTimer.lastCrossMsOfDay(), n, lapMs, lapTimer.lastLapMaxSpeed());

  if (bestEver == 0 || lapMs < bestEver) {
    bestEver = lapMs;
    storage.saveBestEver(lapMs);
  }

  char tm[12];
  fmtLapTime(tm, sizeof(tm), lapMs, true);
  Serial.printf("[LAP %d] %s  vmax %.1f km/h\n", n, tm, lapTimer.lastLapMaxSpeed());
}

void handleButton(ButtonEvent ev) {
  if (ev == ButtonEvent::Short) {
    page = (Page)(((int)page + 1) % (int)Page::COUNT);
    return;
  }
  // Long press: action depends on the page. Nothing on RACE/GPS to avoid
  // accidental actions while riding.
  switch (page) {
    case Page::Session:
      lapTimer.resetSession();
      display.notify("SESSION RESET");
      Serial.println("[SESSION] reset");
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
  lapTimer.setLine(l);
  storage.saveLine(l);
  display.notify("LINE SET!");
  Serial.printf("[LINE] set: %.6f %.6f hdg %.0f\n", l.lat, l.lon, l.headingDeg);
}

void handleSerial() {
  static char buf[96];
  static size_t len = 0;
  while (Serial.available()) {
    char c = (char)Serial.read();
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
        Serial.println("  i  info (line, session, GPS)");
        Serial.println("  d  dump the CSV lap log");
        Serial.println("  x  erase the CSV lap log");
        Serial.println("  r  reset the session");
        Serial.println("  z  erase the all-time best");
        Serial.println("  L <lat> <lon> <hdg> [half-width]  set the line manually");
        break;
      case 'i': {
        const StartLine& l = lapTimer.line();
        if (l.isSet) {
          Serial.printf("Line: %.6f %.6f hdg %.0f half-width %.0fm\n",
                        l.lat, l.lon, l.headingDeg, l.halfWidthM);
        } else {
          Serial.println("Line: not set");
        }
        char tm[12];
        fmtLapTime(tm, sizeof(tm), lapTimer.bestLapMs(), true);
        Serial.printf("Session: %d laps, best %s, vmax %.1f km/h\n",
                      lapTimer.lapCount(), lapTimer.lapCount() ? tm : "-", lapTimer.sessionMaxSpeed());
        fmtLapTime(tm, sizeof(tm), bestEver, true);
        Serial.printf("All-time best: %s\n", bestEver ? tm : "-");
        GpsView g = buildGpsView();
        Serial.printf("GPS: fix=%d sats=%d hdop=%.1f %.1fHz %lu baud\n",
                      g.hasFix, g.sats, g.hdop, g.rateHz, (unsigned long)g.baud);
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
        bestEver = 0;
        storage.clearBestEver();
        Serial.println("All-time best erased.");
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
        lapTimer.setLine(l);
        storage.saveLine(l);
        Serial.printf("Line set: %.6f %.6f hdg %.0f half-width %.0fm\n",
                      l.lat, l.lon, l.headingDeg, l.halfWidthM);
        break;
      }
      default:
        Serial.println("Unknown command, 'h' for help.");
        break;
    }
  }
}

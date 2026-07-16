#include "display.h"
#include <stdlib.h>

// "+1.23" / "-0.45" (seconds.hundredths)
static void fmtDelta(char* buf, size_t n, int32_t d) {
  uint32_t a = (uint32_t)labs((long)d);
  snprintf(buf, n, "%c%lu.%02lu", d < 0 ? '-' : '+',
           (unsigned long)(a / 1000UL), (unsigned long)((a / 10UL) % 100UL));
}

// "+1.2" / "-0.4" (seconds.tenths, clamped to 9.9) — for the sector row.
static void fmtDeltaShort(char* buf, size_t n, int32_t d) {
  long a = labs((long)d);
  if (a > 9900) a = 9900;
  snprintf(buf, n, "%c%ld.%ld", d < 0 ? '-' : '+', a / 1000, (a / 100) % 10);
}

void Display::begin() {
  u8g2_.begin();
  u8g2_.setBusClock(400000);
  u8g2_.setContrast(255);  // full contrast for direct sunlight
}

void Display::setDim(bool dim) {
  if (dim == dimmed_) return;
  dimmed_ = dim;
  u8g2_.setContrast(dim ? 30 : 255);
}

void Display::drawCentered(const char* s, int y) {
  int w = u8g2_.getStrWidth(s);
  u8g2_.drawStr((128 - w) / 2, y, s);
}

void Display::splash(const char* version) {
  u8g2_.clearBuffer();
  u8g2_.setFont(u8g2_font_logisoso16_tr);
  drawCentered("LAP TIMER", 24);
  u8g2_.setFont(u8g2_font_6x10_tf);
  drawCentered("Ninja 400 - MSP BKK", 42);
  drawCentered(version, 56);
  u8g2_.sendBuffer();
}

void Display::notify(const char* msg, uint32_t durationMs) {
  strncpy(notif_, msg, sizeof(notif_) - 1);
  notif_[sizeof(notif_) - 1] = 0;
  notifUntil_ = millis() + durationMs;
}

void Display::render(Page page, const LapTimer& t, const GpsView& g, const EcuData& e,
                     const ImuData& m, const TireData& ti, uint32_t now,
                     const char* trackName) {
  u8g2_.clearBuffer();
  bool flashActive = t.lapCount() > 0 && t.timing() &&
                     now - t.lastCrossLocalMs() < LAP_FLASH_MS;
  if (notifUntil_ && now < notifUntil_) {
    drawNotify();
  } else if (page == Page::Race && flashActive) {
    drawLapFlash(t);
  } else {
    switch (page) {
      case Page::Race:    drawRace(t, g, now); break;
      case Page::Session: drawSession(t); break;
      case Page::Ecu:     drawEcu(e); break;
      case Page::Lean:    drawLean(m); break;
      case Page::Tires:   drawTires(ti); break;
      case Page::Gps:     drawGps(g); break;
      case Page::Line:    drawLinePage(t, g, trackName); break;
      default: break;
    }
  }
  u8g2_.sendBuffer();
}

void Display::drawEcu(const EcuData& e) {
  char b[32];
  u8g2_.setFont(u8g2_font_6x12_tf);
  u8g2_.drawStr(0, 10, e.link ? "ECU LINK" : "ECU ---");
  if (e.coolantC > -100) {
    snprintf(b, sizeof(b), "%.0fC", e.coolantC);
    u8g2_.drawStr(128 - u8g2_.getStrWidth(b), 10, b);
  }
  if (!e.link) {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(0, 30, "Waiting for the bike:");
    u8g2_.drawStr(0, 40, "ignition ON, K-line");
    u8g2_.drawStr(0, 50, "wired (L9637D)");
    return;
  }

  // Gear, huge (N when neutral).
  char g[4];
  if (e.gear == 0) {
    strncpy(g, "N", sizeof(g));
  } else if (e.gear > 0) {
    snprintf(g, sizeof(g), "%u", (unsigned)e.gear % 10u);
  } else {
    strncpy(g, "-", sizeof(g));
  }
  u8g2_.setFont(u8g2_font_logisoso28_tr);
  u8g2_.drawStr(8, 48, g);

  // RPM, right-aligned.
  u8g2_.setFont(u8g2_font_logisoso16_tr);
  snprintf(b, sizeof(b), "%d", e.rpm);
  u8g2_.drawStr(122 - 22 - u8g2_.getStrWidth(b), 42, b);
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.drawStr(104, 42, "rpm");

  // Throttle bar.
  u8g2_.drawFrame(0, 54, 128, 10);
  if (e.throttlePct >= 0) {
    int w = (int)(124.0f * e.throttlePct / 100.0f);
    if (w > 0) u8g2_.drawBox(2, 56, w, 6);
  }
}

void Display::drawNotify() {
  u8g2_.drawBox(0, 18, 128, 28);
  u8g2_.setDrawColor(0);
  u8g2_.setFont(u8g2_font_6x12_tf);
  drawCentered(notif_, 36);
  u8g2_.setDrawColor(1);
}

// Lap completed: inverted, highly visible screen for LAP_FLASH_MS.
void Display::drawLapFlash(const LapTimer& t) {
  char b[40];
  u8g2_.drawBox(0, 0, 128, 64);
  u8g2_.setDrawColor(0);

  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "LAP %d", t.lapCount());
  u8g2_.drawStr(2, 11, b);
  if (t.lastLapWasRecord()) {
    u8g2_.drawStr(128 - u8g2_.getStrWidth("RECORD!") - 2, 11, "RECORD!");
  } else if (t.lastDeltaBestMs() != 0) {
    char d[12];
    fmtDelta(d, sizeof(d), t.lastDeltaBestMs());
    u8g2_.drawStr(128 - u8g2_.getStrWidth(d) - 2, 11, d);
  }

  u8g2_.setFont(u8g2_font_logisoso28_tn);
  fmtLapTime(b, sizeof(b), t.lastLapMs(), true);
  drawCentered(b, 47);

  u8g2_.setFont(u8g2_font_6x12_tf);
  if (t.hasSectors()) {
    char s1[8], s2[8], s3[8];
    fmtDeltaShort(s1, sizeof(s1), t.lastSectorDeltaMs(0));
    fmtDeltaShort(s2, sizeof(s2), t.lastSectorDeltaMs(1));
    fmtDeltaShort(s3, sizeof(s3), t.lastSectorDeltaMs(2));
    snprintf(b, sizeof(b), "%s %s %s", s1, s2, s3);
    drawCentered(b, 63);
  } else if (t.lapCount() < 2) {
    drawCentered("first lap", 63);
  }
  u8g2_.setDrawColor(1);
}

void Display::drawRace(const LapTimer& t, const GpsView& g, uint32_t now) {
  char b[32];

  // Top bar: laps, speed, link + GPS state.
  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "L%d", t.lapCount());
  u8g2_.drawStr(0, 10, b);
  snprintf(b, sizeof(b), "%3.0f km/h", g.speedKmh);
  drawCentered(b, 10);
  if (g.hasFix) {
    snprintf(b, sizeof(b), "%sS%d", g.wifi ? "W " : (g.ble ? "B " : ""), g.sats);
    u8g2_.drawStr(128 - u8g2_.getStrWidth(b), 10, b);
  } else if ((now / 400) % 2) {
    u8g2_.drawStr(128 - u8g2_.getStrWidth("GPS!"), 10, "GPS!");
  }

  // Current lap time, extra large.
  u8g2_.setFont(u8g2_font_logisoso24_tn);
  if (t.timing()) {
    fmtLapTime(b, sizeof(b), t.currentLapMs(now), false);
  } else {
    strncpy(b, "-:--.-", sizeof(b));
  }
  drawCentered(b, 44);

  // Bottom line: live predictive delta once a reference lap exists,
  // otherwise last / best, otherwise a contextual hint.
  u8g2_.setFont(u8g2_font_6x12_tf);
  if (t.hasPredDelta()) {
    char d[12], m[12];
    fmtDelta(d, sizeof(d), t.predDeltaMs());
    fmtLapTime(m, sizeof(m), t.allTimeBestMs(), true);
    if (t.predDeltaMs() < 0) {
      // Gaining on the best lap: inverted patch, readable at a glance.
      u8g2_.drawBox(0, 52, u8g2_.getStrWidth(d) + 5, 12);
      u8g2_.setDrawColor(0);
      u8g2_.drawStr(2, 62, d);
      u8g2_.setDrawColor(1);
    } else {
      u8g2_.drawStr(2, 62, d);
    }
    snprintf(b, sizeof(b), "B%s", m);
    u8g2_.drawStr(128 - u8g2_.getStrWidth(b), 63, b);
  } else if (t.lapCount() > 0) {
    char l[12], m[12];
    fmtLapTime(l, sizeof(l), t.lastLapMs(), true);
    fmtLapTime(m, sizeof(m), t.bestLapMs(), true);
    snprintf(b, sizeof(b), "L%s  B%s", l, m);
    drawCentered(b, 63);
  } else if (!t.line().isSet) {
    drawCentered("Line not set", 63);
  } else if (!t.timing()) {
    drawCentered("Ready: cross the line", 63);
  } else {
    drawCentered("Lap 1 running...", 63);
  }
}

void Display::drawSession(const LapTimer& t) {
  char b[36], tm[12];
  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "S%d L%d  Vmax %3.0f", t.sessionIndex(), t.lapCount(),
           t.sessionMaxSpeed());
  u8g2_.drawStr(0, 10, b);

  // Theoretical best (sum of the best sectors) and session average.
  char tb[12], avg[12];
  fmtLapTime(tb, sizeof(tb), t.theoreticalBestMs(), false);
  fmtLapTime(avg, sizeof(avg), t.sessionAvgMs(), false);
  snprintf(b, sizeof(b), "TB %s  Avg %s",
           t.theoreticalBestMs() ? tb : "-", t.lapCount() ? avg : "-");
  u8g2_.drawStr(0, 21, b);

  int stored = t.storedLaps();
  int row = 0;
  for (int i = stored - 1; i >= 0 && row < 3; i--, row++) {
    fmtLapTime(tm, sizeof(tm), t.lap(i).ms, true);
    snprintf(b, sizeof(b), "%2d  %s %s", i + 1, tm, i == t.bestLapIndex() ? "*" : "");
    u8g2_.drawStr(4, 32 + row * 10, b);
  }
  if (stored == 0) u8g2_.drawStr(4, 40, "(no laps yet)");

  u8g2_.setFont(u8g2_font_6x10_tf);
  drawCentered("long press = reset", 63);
}

void Display::drawLean(const ImuData& m) {
  char b[32];
  u8g2_.setFont(u8g2_font_6x12_tf);
  u8g2_.drawStr(0, 10, "LEAN");
  if (m.present) {
    snprintf(b, sizeof(b), "G %+.1f", m.gLong);
    u8g2_.drawStr(128 - u8g2_.getStrWidth(b), 10, b);
  } else {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(0, 30, "IMU: no sensor");
    u8g2_.drawStr(0, 40, "MPU6050 on the I2C bus");
    u8g2_.drawStr(0, 50, "(SDA 21 / SCL 22)");
    return;
  }

  // Current lean angle, huge, with the side letter.
  float lean = m.leanDeg;
  char side = lean < -2 ? 'L' : (lean > 2 ? 'R' : ' ');
  float a = fabsf(lean);
  u8g2_.setFont(u8g2_font_logisoso28_tr);
  snprintf(b, sizeof(b), "%2.0f", (double)(a < 90 ? a : 90));
  int w = u8g2_.getStrWidth(b);
  u8g2_.drawStr((128 - w) / 2 - 8, 48, b);
  if (side != ' ') {
    u8g2_.setFont(u8g2_font_logisoso16_tr);
    char s[2] = {side, 0};
    u8g2_.drawStr((128 + w) / 2 - 2, 48, s);
  }

  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "max L%2.0f R%2.0f  B%.1fG",
           m.maxLeanL, m.maxLeanR, m.maxBrakeG > 0 ? m.maxBrakeG : 0);
  drawCentered(b, 63);
}

void Display::drawTires(const TireData& ti) {
  char b[24];
  u8g2_.setFont(u8g2_font_6x12_tf);
  u8g2_.drawStr(0, 10, "TIRES");
  if (!ti.frontOk && !ti.rearOk) {
    u8g2_.setFont(u8g2_font_6x10_tf);
    u8g2_.drawStr(0, 30, "No IR sensor");
    u8g2_.drawStr(0, 40, "MLX90614 on bus 2");
    u8g2_.drawStr(0, 50, "(SDA 32 / SCL 33)");
    return;
  }
  // Front on the left, rear on the right, big.
  u8g2_.setFont(u8g2_font_6x12_tf);
  u8g2_.drawStr(10, 24, "FRONT");
  u8g2_.drawStr(78, 24, "REAR");
  u8g2_.setFont(u8g2_font_logisoso24_tn);
  if (ti.frontOk) {
    snprintf(b, sizeof(b), "%2.0f", (double)ti.frontC);
  } else {
    strncpy(b, "--", sizeof(b));
  }
  u8g2_.drawStr(8, 54, b);
  if (ti.rearOk) {
    snprintf(b, sizeof(b), "%2.0f", (double)ti.rearC);
  } else {
    strncpy(b, "--", sizeof(b));
  }
  u8g2_.drawStr(74, 54, b);
  u8g2_.drawVLine(64, 14, 46);
  u8g2_.setFont(u8g2_font_6x10_tf);
  drawCentered("deg C, tread surface", 63);
}

void Display::drawGps(const GpsView& g) {
  char b[36];
  u8g2_.setFont(u8g2_font_6x10_tf);
  snprintf(b, sizeof(b), "GPS %s  %.1fHz%s", g.hasFix ? "FIX" : "---",
           g.rateHz, g.sim ? " SIM" : "");
  u8g2_.drawStr(0, 9, b);
  snprintf(b, sizeof(b), "Sats %d  HDOP %.1f", g.sats, g.hdop);
  u8g2_.drawStr(0, 19, b);
  snprintf(b, sizeof(b), "Lat %11.5f", g.lat);
  u8g2_.drawStr(0, 29, b);
  snprintf(b, sizeof(b), "Lon %11.5f", g.lon);
  u8g2_.drawStr(0, 39, b);
  snprintf(b, sizeof(b), "Spd %.1f km/h", g.speedKmh);
  u8g2_.drawStr(0, 49, b);
  if (g.wifi) {
    u8g2_.drawStr(0, 59, "WiFi ON 192.168.4.1");
  } else {
    snprintf(b, sizeof(b), "Age %lums  %lubd  [W]", (unsigned long)g.fixAgeMs,
             (unsigned long)g.baud);
    u8g2_.drawStr(0, 59, b);
  }
}

void Display::drawLinePage(const LapTimer& t, const GpsView& g, const char* trackName) {
  char b[32];
  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "TRACK: %s", trackName);
  u8g2_.drawStr(0, 10, b);
  if (t.line().isSet && !isnan(t.distToLineM())) {
    snprintf(b, sizeof(b), "Line %4.0fm  Hdg %3.0f", t.distToLineM(), t.line().headingDeg);
    u8g2_.drawStr(0, 22, b);
  } else {
    u8g2_.drawStr(0, 22, t.line().isSet ? "Line set" : "Line not set");
  }
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.drawStr(0, 36, "Long press while");
  u8g2_.drawStr(0, 46, "crossing the line");
  u8g2_.drawStr(0, 56, "at >10km/h = set here");
}

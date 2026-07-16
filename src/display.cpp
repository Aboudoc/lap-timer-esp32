#include "display.h"
#include <stdlib.h>

// "+1.23" / "-0.45" (secondes.centiemes)
static void fmtDelta(char* buf, size_t n, int32_t d) {
  uint32_t a = (uint32_t)labs((long)d);
  snprintf(buf, n, "%c%lu.%02lu", d < 0 ? '-' : '+',
           (unsigned long)(a / 1000UL), (unsigned long)((a / 10UL) % 100UL));
}

void Display::begin() {
  u8g2_.begin();
  u8g2_.setBusClock(400000);
  u8g2_.setContrast(255);  // plein contraste pour le soleil
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

void Display::render(Page page, const LapTimer& t, const GpsView& g,
                     uint32_t now, uint32_t bestEverMs) {
  u8g2_.clearBuffer();
  bool flashActive = t.lapCount() > 0 && t.timing() &&
                     now - t.lastCrossLocalMs() < LAP_FLASH_MS;
  if (notifUntil_ && now < notifUntil_) {
    drawNotify();
  } else if (page == Page::Race && flashActive) {
    drawLapFlash(t, bestEverMs);
  } else {
    switch (page) {
      case Page::Race:    drawRace(t, g, now); break;
      case Page::Session: drawSession(t); break;
      case Page::Gps:     drawGps(g); break;
      case Page::Line:    drawLinePage(t, g); break;
      default: break;
    }
  }
  u8g2_.sendBuffer();
}

void Display::drawNotify() {
  u8g2_.drawBox(0, 18, 128, 28);
  u8g2_.setDrawColor(0);
  u8g2_.setFont(u8g2_font_6x12_tf);
  drawCentered(notif_, 36);
  u8g2_.setDrawColor(1);
}

// Fin de tour : ecran inverse tres visible pendant LAP_FLASH_MS.
void Display::drawLapFlash(const LapTimer& t, uint32_t bestEverMs) {
  char b[32];
  u8g2_.drawBox(0, 0, 128, 64);
  u8g2_.setDrawColor(0);

  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "TOUR %d", t.lapCount());
  u8g2_.drawStr(2, 11, b);
  if (bestEverMs && t.lastLapMs() == bestEverMs) {
    u8g2_.drawStr(128 - u8g2_.getStrWidth("RECORD!") - 2, 11, "RECORD!");
  }

  u8g2_.setFont(u8g2_font_logisoso28_tn);
  fmtLapTime(b, sizeof(b), t.lastLapMs(), true);
  drawCentered(b, 47);

  u8g2_.setFont(u8g2_font_6x12_tf);
  if (t.lapCount() >= 2) {
    char d[16];
    fmtDelta(d, sizeof(d), t.lastDeltaBestMs());
    snprintf(b, sizeof(b), "%s vs meilleur", d);
    drawCentered(b, 63);
  } else {
    drawCentered("premier tour", 63);
  }
  u8g2_.setDrawColor(1);
}

void Display::drawRace(const LapTimer& t, const GpsView& g, uint32_t now) {
  char b[32];

  // Bandeau superieur : tours, vitesse, etat GPS.
  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "T%d", t.lapCount());
  u8g2_.drawStr(0, 10, b);
  snprintf(b, sizeof(b), "%3.0f km/h", g.speedKmh);
  drawCentered(b, 10);
  if (g.hasFix) {
    snprintf(b, sizeof(b), "S%d", g.sats);
    u8g2_.drawStr(128 - u8g2_.getStrWidth(b), 10, b);
  } else if ((now / 400) % 2) {
    u8g2_.drawStr(128 - u8g2_.getStrWidth("GPS!"), 10, "GPS!");
  }

  // Temps du tour en cours, en tres gros.
  u8g2_.setFont(u8g2_font_logisoso24_tn);
  if (t.timing()) {
    fmtLapTime(b, sizeof(b), t.currentLapMs(now), false);
  } else {
    strncpy(b, "-:--.-", sizeof(b));
  }
  drawCentered(b, 44);

  // Ligne du bas : dernier / meilleur, ou aide contextuelle.
  u8g2_.setFont(u8g2_font_6x12_tf);
  if (t.lapCount() > 0) {
    char l[12], m[12];
    fmtLapTime(l, sizeof(l), t.lastLapMs(), true);
    fmtLapTime(m, sizeof(m), t.bestLapMs(), true);
    snprintf(b, sizeof(b), "D%s  M%s", l, m);
    drawCentered(b, 63);
  } else if (!t.line().isSet) {
    drawCentered("Ligne non definie", 63);
  } else if (!t.timing()) {
    drawCentered("Pret: franchir ligne", 63);
  } else {
    drawCentered("Tour 1 en cours...", 63);
  }
}

void Display::drawSession(const LapTimer& t) {
  char b[32], tm[12];
  u8g2_.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "Tours:%d  Vmax %3.0f", t.lapCount(), t.sessionMaxSpeed());
  u8g2_.drawStr(0, 10, b);

  int stored = t.storedLaps();
  int row = 0;
  for (int i = stored - 1; i >= 0 && row < 4; i--, row++) {
    fmtLapTime(tm, sizeof(tm), t.lap(i).ms, true);
    snprintf(b, sizeof(b), "%2d  %s %s", i + 1, tm, i == t.bestLapIndex() ? "*" : "");
    u8g2_.drawStr(4, 22 + row * 10, b);
  }
  if (stored == 0) u8g2_.drawStr(4, 32, "(aucun tour)");

  u8g2_.setFont(u8g2_font_6x10_tf);
  drawCentered("appui long = reset", 63);
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
  snprintf(b, sizeof(b), "Vit %.1f km/h", g.speedKmh);
  u8g2_.drawStr(0, 49, b);
  snprintf(b, sizeof(b), "Age %lums  %lubd", (unsigned long)g.fixAgeMs, (unsigned long)g.baud);
  u8g2_.drawStr(0, 59, b);
}

void Display::drawLinePage(const LapTimer& t, const GpsView& g) {
  char b[32];
  u8g2_.setFont(u8g2_font_6x12_tf);
  u8g2_.drawStr(0, 10, t.line().isSet ? "LIGNE: definie" : "LIGNE: non definie");
  if (t.line().isSet && !isnan(t.distToLineM())) {
    snprintf(b, sizeof(b), "Dist %4.0fm  Cap %3.0f", t.distToLineM(), t.line().headingDeg);
    u8g2_.drawStr(0, 22, b);
  }
  u8g2_.setFont(u8g2_font_6x10_tf);
  u8g2_.drawStr(0, 36, "Appui long en passant");
  u8g2_.drawStr(0, 46, "sur la ligne (>10kmh)");
  u8g2_.drawStr(0, 56, "= definir la ligne ici");
}

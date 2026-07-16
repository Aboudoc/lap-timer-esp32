// PIT BOX — the coach-side receiver (second ESP32 + OLED + LoRa).
// Flash with: pio run -e pitbox -t upload
//
// Shows the rider's live data broadcast by the bike: predictive delta (big),
// current/last/best lap, session, speed, lean, tire and engine temperatures.
// No buttons: power it, it listens.

#include <Arduino.h>
#include <U8g2lib.h>
#include <LoRa.h>
#include "config.h"
#include "pitlink_proto.h"
#include "laptimer.h"  // fmtLapTime

static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE,
                                                PIN_I2C_SCL, PIN_I2C_SDA);

static PitPacket pkt;
static bool      everReceived = false;
static uint32_t  lastRxMs = 0;
static int       lastRssi = 0;
static bool      radioOk = false;

static void fmtDelta(char* buf, size_t n, int32_t d) {
  uint32_t a = (uint32_t)labs((long)d);
  snprintf(buf, n, "%c%lu.%02lu", d < 0 ? '-' : '+',
           (unsigned long)(a / 1000UL), (unsigned long)((a / 10UL) % 100UL));
}

static void drawCentered(const char* s, int y) {
  u8g2.drawStr((128 - u8g2.getStrWidth(s)) / 2, y, s);
}

void setup() {
  Serial.begin(SERIAL_BAUD);
  setCpuFrequencyMhz(80);

  u8g2.begin();
  u8g2.setBusClock(400000);
  u8g2.setContrast(255);

  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
  radioOk = LoRa.begin(LORA_FREQ_HZ);
  if (radioOk) {
    LoRa.setSyncWord(LORA_SYNC_WORD);
    LoRa.enableCrc();
    Serial.printf("[PITBOX] listening on %.1f MHz\n", LORA_FREQ_HZ / 1e6);
  } else {
    Serial.println("[PITBOX] LoRa radio not detected!");
  }
}

static void receive() {
  int n = LoRa.parsePacket();
  if (n <= 0) return;
  uint8_t buf[64];
  int i = 0;
  while (LoRa.available() && i < (int)sizeof(buf)) buf[i++] = (uint8_t)LoRa.read();
  PitPacket p;
  if (pitDecode(buf, (size_t)i, p)) {
    pkt = p;
    everReceived = true;
    lastRxMs = millis();
    lastRssi = LoRa.packetRssi();
  }
}

static void render(uint32_t now) {
  char b[32], tm[12];
  u8g2.clearBuffer();

  if (!radioOk) {
    u8g2.setFont(u8g2_font_6x12_tf);
    drawCentered("PIT BOX", 20);
    drawCentered("LoRa radio missing!", 40);
    u8g2.sendBuffer();
    return;
  }
  if (!everReceived) {
    u8g2.setFont(u8g2_font_logisoso16_tr);
    drawCentered("PIT BOX", 24);
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered("waiting for the bike...", 42);
    snprintf(b, sizeof(b), "%.1f MHz", LORA_FREQ_HZ / 1e6);
    drawCentered(b, 56);
    u8g2.sendBuffer();
    return;
  }

  bool stale = now - lastRxMs > PIT_STALE_MS;

  // Top bar: session/laps + track, link freshness or RSSI.
  u8g2.setFont(u8g2_font_6x12_tf);
  snprintf(b, sizeof(b), "S%u L%u %s", pkt.session, pkt.lapCount, pkt.track);
  u8g2.drawStr(0, 10, b);
  if (stale && (now / 400) % 2) {
    u8g2.drawStr(128 - u8g2.getStrWidth("OLD!"), 10, "OLD!");
  } else if (!stale) {
    snprintf(b, sizeof(b), "%ddB", lastRssi);
    u8g2.drawStr(128 - u8g2.getStrWidth(b), 10, b);
  }

  // Big center: predictive delta when valid, else the current lap time.
  bool hasPred = pkt.flags & PIT_F_HASPRED;
  bool timing = pkt.flags & PIT_F_TIMING;
  uint32_t curMs = pkt.currentLapMs;
  if (timing && !stale) curMs += now - lastRxMs;  // extrapolate between packets

  if (hasPred) {
    char d[12];
    fmtDelta(d, sizeof(d), pkt.predDeltaMs);
    u8g2.setFont(u8g2_font_logisoso28_tn);
    if (pkt.predDeltaMs < 0) {
      int w = u8g2.getStrWidth(d);
      u8g2.drawBox((128 - w) / 2 - 3, 16, w + 6, 32);
      u8g2.setDrawColor(0);
      drawCentered(d, 45);
      u8g2.setDrawColor(1);
    } else {
      drawCentered(d, 45);
    }
  } else if (timing) {
    fmtLapTime(b, sizeof(b), curMs, false);
    u8g2.setFont(u8g2_font_logisoso28_tn);
    drawCentered(b, 45);
  } else {
    u8g2.setFont(u8g2_font_logisoso16_tr);
    drawCentered("- no lap -", 40);
  }

  // Bottom: last/best, then live numbers.
  u8g2.setFont(u8g2_font_6x10_tf);
  char l[12], m[12];
  fmtLapTime(l, sizeof(l), pkt.lastLapMs, true);
  fmtLapTime(m, sizeof(m), pkt.bestLapMs, true);
  snprintf(b, sizeof(b), "L%s B%s", pkt.lastLapMs ? l : "-:--", pkt.bestLapMs ? m : "-:--");
  drawCentered(b, 55);
  char tf[6] = "--", tr[6] = "--";
  if (pkt.tireF != 255) snprintf(tf, sizeof(tf), "%d", (int)pkt.tireF - 40);
  if (pkt.tireR != 255) snprintf(tr, sizeof(tr), "%d", (int)pkt.tireR - 40);
  snprintf(b, sizeof(b), "%uk %dd F%s R%s", pkt.speedKmh, (int)pkt.leanDeg, tf, tr);
  drawCentered(b, 64);
  (void)tm;

  u8g2.sendBuffer();
}

void loop() {
  uint32_t now = millis();
  receive();
  static uint32_t lastRender = 0;
  if (now - lastRender >= 100) {
    lastRender = now;
    render(now);
  }
}

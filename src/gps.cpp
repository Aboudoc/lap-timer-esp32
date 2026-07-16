#include "gps.h"

void GpsModule::begin(HardwareSerial& serial) {
  serial_ = &serial;
  configureModule();
}

// Waits until the start of an NMEA sentence ("$G") shows up on the serial port.
bool GpsModule::waitForNmea(uint32_t timeoutMs) {
  uint32_t start = millis();
  int state = 0;
  while (millis() - start < timeoutMs) {
    while (serial_->available()) {
      char c = (char)serial_->read();
      if (state == 0) {
        if (c == '$') state = 1;
      } else {
        if (c == 'G') return true;
        state = (c == '$') ? 1 : 0;
      }
    }
    delay(5);
  }
  return false;
}

// Tries several baud rates until valid NMEA is received.
// The module cold-boots at 9600, but keeps the configured baud as long as
// its backup battery holds a charge (warm start).
uint32_t GpsModule::detectBaud() {
  const uint32_t candidates[] = {GPS_TARGET_BAUD, 9600, 115200, 38400, 19200};
  for (uint32_t b : candidates) {
    serial_->updateBaudRate(b);
    delay(50);
    while (serial_->available()) serial_->read();
    if (waitForNmea(700)) return b;
  }
  return 0;
}

// Sends a UBX frame (u-blox binary protocol) with its checksum.
void GpsModule::sendUbx(uint8_t cls, uint8_t id, const uint8_t* payload, uint16_t len) {
  uint8_t hdr[6] = {0xB5, 0x62, cls, id, (uint8_t)(len & 0xFF), (uint8_t)(len >> 8)};
  uint8_t ckA = 0, ckB = 0;
  for (int i = 2; i < 6; i++) { ckA += hdr[i]; ckB += ckA; }
  for (uint16_t i = 0; i < len; i++) { ckA += payload[i]; ckB += ckA; }
  serial_->write(hdr, 6);
  if (len) serial_->write(payload, len);
  serial_->write(ckA);
  serial_->write(ckB);
  serial_->flush();
}

// Emission rate of a standard NMEA sentence (class 0xF0) on the current port.
void GpsModule::setNmeaRate(uint8_t msgId, uint8_t rate) {
  uint8_t p[3] = {0xF0, msgId, rate};
  sendUbx(0x06, 0x01, p, 3);  // CFG-MSG
  delay(30);
}

void GpsModule::configureModule() {
  serial_->setRxBufferSize(1024);  // ride out short main-loop stalls without losing NMEA
  serial_->begin(9600, SERIAL_8N1, PIN_GPS_RX, PIN_GPS_TX);
  delay(100);

  uint32_t detected = detectBaud();
  baud_ = detected ? detected : 9600;
  if (!detected) {
    serial_->updateBaudRate(9600);
    Serial.println("[GPS] no NMEA detected (module unplugged?), config sent at 9600");
  }

  // Switch the module's UART to the target baud if needed (CFG-PRT).
  if (baud_ != GPS_TARGET_BAUD) {
    uint8_t prt[20] = {0};
    prt[0]  = 1;                                   // portID 1 = UART
    prt[8]  = 0xD0; prt[9] = 0x08;                 // mode 0x000008D0: 8N1
    prt[12] = (uint8_t)(GPS_TARGET_BAUD & 0xFF);   // baud, little-endian
    prt[13] = (uint8_t)((GPS_TARGET_BAUD >> 8) & 0xFF);
    prt[14] = (uint8_t)((GPS_TARGET_BAUD >> 16) & 0xFF);
    prt[15] = (uint8_t)((GPS_TARGET_BAUD >> 24) & 0xFF);
    prt[16] = 0x07;                                // in: UBX+NMEA+RTCM
    prt[18] = 0x03;                                // out: UBX+NMEA
    sendUbx(0x06, 0x00, prt, 20);
    delay(100);
    serial_->updateBaudRate(GPS_TARGET_BAUD);
    delay(50);
    while (serial_->available()) serial_->read();
    if (waitForNmea(1000)) {
      baud_ = GPS_TARGET_BAUD;
    } else {
      serial_->updateBaudRate(baud_);  // failed: stay at the detected baud
    }
  }

  // Keep only RMC (position/speed/course/time) and GGA (sats/hdop).
  setNmeaRate(0x00, 1);  // GGA
  setNmeaRate(0x01, 0);  // GLL
  setNmeaRate(0x02, 0);  // GSA
  setNmeaRate(0x03, 0);  // GSV
  setNmeaRate(0x04, 1);  // RMC
  setNmeaRate(0x05, 0);  // VTG

  // Measurement rate (CFG-RATE): 200 ms -> 5 Hz.
  uint8_t rate[6] = {(uint8_t)(GPS_MEAS_RATE_MS & 0xFF), (uint8_t)(GPS_MEAS_RATE_MS >> 8),
                     0x01, 0x00, 0x01, 0x00};
  sendUbx(0x06, 0x08, rate, 6);
  delay(30);

  // Save to the module's battery-backed RAM (CFG-CFG): the next warm start
  // boots already configured.
  uint8_t cfg[13] = {0, 0, 0, 0, 0xFF, 0xFF, 0, 0, 0, 0, 0, 0, 0x17};
  sendUbx(0x06, 0x09, cfg, 13);

  Serial.printf("[GPS] configured: %lu baud, %u ms/measurement\n",
                (unsigned long)baud_, GPS_MEAS_RATE_MS);
}

bool GpsModule::update() {
  bool newFix = false;
  while (serial_ && serial_->available()) {
    if (processChar((char)serial_->read())) newFix = true;
  }
  return newFix;
}

bool GpsModule::processChar(char c) {
  if (!gps_.encode(c)) return false;
  // A complete sentence was just parsed. Emit a fix when position AND
  // speed are fresh (= RMC sentence), once per epoch.
  if (!gps_.location.isUpdated() || !gps_.speed.isUpdated() || !gps_.time.isValid()) return false;

  uint32_t mod = ((uint32_t)gps_.time.hour() * 3600UL + gps_.time.minute() * 60UL +
                  gps_.time.second()) * 1000UL + gps_.time.centisecond() * 10UL;
  if (mod == lastEmitMsOfDay_) {
    // Same epoch (redundant sentence): just consume the values to clear
    // the "updated" flags.
    gps_.location.lat(); gps_.location.lng(); gps_.speed.kmph(); gps_.course.deg();
    return false;
  }
  lastEmitMsOfDay_ = mod;

  uint32_t now = millis();
  fix_.lat       = gps_.location.lat();
  fix_.lon       = gps_.location.lng();
  fix_.speedKmh  = gps_.speed.isValid() ? (float)gps_.speed.kmph() : 0.0f;
  fix_.courseDeg = gps_.course.isValid() ? (float)gps_.course.deg() : 0.0f;
  fix_.altValid  = gps_.altitude.isValid();
  fix_.altM      = fix_.altValid ? (float)gps_.altitude.meters() : 0.0f;
  fix_.msOfDay   = mod;
  fix_.localMs   = now;
  fix_.valid     = gps_.location.isValid();

  if (lastFixLocalMs_ != 0) {
    float dt = (now - lastFixLocalMs_) / 1000.0f;
    if (dt > 0.01f && dt < 3.0f) {
      float inst = 1.0f / dt;
      rateHz_ = (rateHz_ == 0) ? inst : rateHz_ * 0.9f + inst * 0.1f;
    }
  }
  lastFixLocalMs_ = now;
  return fix_.valid;
}

bool GpsModule::hasFix() {
  return gps_.location.isValid() && gps_.location.age() < 2000;
}

int GpsModule::sats() {
  return gps_.satellites.isValid() ? (int)gps_.satellites.value() : 0;
}

float GpsModule::hdop() {
  return gps_.hdop.isValid() ? (float)gps_.hdop.hdop() : 99.9f;
}

float GpsModule::lastSpeedKmh() {
  return (float)gps_.speed.kmph();
}

uint32_t GpsModule::fixAgeMs() {
  return gps_.location.age();
}

bool GpsModule::dateYmd(int& y, int& m, int& d) {
  if (!gps_.date.isValid()) return false;
  y = gps_.date.year();
  m = gps_.date.month();
  d = gps_.date.day();
  return true;
}

void GpsModule::dateStr(char* out, size_t n) {
  if (gps_.date.isValid()) {
    snprintf(out, n, "%04d-%02d-%02d", gps_.date.year(), gps_.date.month(), gps_.date.day());
  } else {
    snprintf(out, n, "0000-00-00");
  }
}

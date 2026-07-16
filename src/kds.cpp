#include "kds.h"

#if ENABLE_KDS

// Poll order: RPM and throttle refresh three times as often as the rest.
static const uint8_t kPoll[] = {kds::REG_RPM, kds::REG_THROTTLE, kds::REG_GEAR,
                                kds::REG_RPM, kds::REG_THROTTLE, kds::REG_COOLANT,
                                kds::REG_RPM, kds::REG_THROTTLE, kds::REG_SPEED};

void KdsBus::begin(HardwareSerial& serial) {
  serial_ = &serial;
  st_ = St::Rest;
  stMs_ = millis() - KDS_RETRY_MS;  // first attempt right away
}

// ISO 14230 fast init: idle high 300 ms, low 25 ms, high 25 ms, then the
// StartCommunication request at 10400 baud.
void KdsBus::startInit() {
  data_.link = false;
  serial_->end();
  pinMode(PIN_KDS_TX, OUTPUT);
  digitalWrite(PIN_KDS_TX, HIGH);
  st_ = St::InitIdle;
  stMs_ = millis();
}

void KdsBus::startRequest(const uint8_t* payload, uint8_t len) {
  txLen_ = (uint8_t)kds::buildFrame(payload, len, txBuf_, sizeof(txBuf_));
  txSent_ = 0;
  lastByteMs_ = 0;
  st_ = St::Send;
  stMs_ = millis();
}

void KdsBus::fail() {
  failCount_++;
  if (data_.link && failCount_ < 3) {  // tolerate isolated lost frames
    st_ = St::Gap;
    stMs_ = millis();
    return;
  }
  if (data_.link) Serial.println("[KDS] ECU link lost");
  data_.link = false;
  st_ = St::Rest;
  stMs_ = millis();
}

void KdsBus::onFrame() {
  const uint8_t* pl = kds::payload(parser_);
  uint8_t n = kds::payloadLen(parser_);
  uint32_t now = millis();

  if (awaitingStart_) {
    if (n >= 1 && pl[0] == 0xC1) {  // StartCommunication accepted
      data_.link = true;
      failCount_ = 0;
      pollIdx_ = 0;
      Serial.println("[KDS] ECU link established");
      st_ = St::Gap;
      stMs_ = now;
    } else {
      fail();
    }
    return;
  }

  if (n >= 2 && pl[0] == 0x61 && pl[1] == curReg_) {
    switch (curReg_) {
      case kds::REG_RPM:
        if (n >= 4) { data_.rpm = kds::rpmFrom(pl[2], pl[3]); data_.updatedMs = now; }
        break;
      case kds::REG_THROTTLE:
        if (n >= 4) data_.throttlePct = kds::throttleFrom(pl[2], pl[3]);
        break;
      case kds::REG_GEAR:
        if (n >= 3) data_.gear = kds::gearFrom(pl[2]);
        break;
      case kds::REG_COOLANT:
        if (n >= 3) data_.coolantC = kds::coolantFrom(pl[2]);
        break;
      case kds::REG_SPEED:
        if (n >= 4) data_.speedKmh = kds::speedFrom(pl[2], pl[3]);
        break;
    }
    failCount_ = 0;
  }
  // Negative responses (0x7F: register not supported) just move on.
  st_ = St::Gap;
  stMs_ = now;
}

void KdsBus::loop() {
  if (!serial_) return;
  uint32_t now = millis();

  switch (st_) {
    case St::Rest:
      if (now - stMs_ >= KDS_RETRY_MS) startInit();
      break;

    case St::InitIdle:
      if (now - stMs_ >= 300) {
        digitalWrite(PIN_KDS_TX, LOW);
        st_ = St::InitLow;
        stMs_ = now;
      }
      break;

    case St::InitLow:
      if (now - stMs_ >= 25) {
        digitalWrite(PIN_KDS_TX, HIGH);
        st_ = St::InitHigh;
        stMs_ = now;
      }
      break;

    case St::InitHigh:
      if (now - stMs_ >= 25) {
        serial_->begin(KDS_BAUD, SERIAL_8N1, PIN_KDS_RX, PIN_KDS_TX);
        while (serial_->available()) serial_->read();
        uint8_t p[1] = {kds::SID_START_COMM};
        awaitingStart_ = true;
        startRequest(p, 1);
      }
      break;

    case St::Send:
      // The K-line echoes every byte we send: swallow while transmitting.
      while (serial_->available()) serial_->read();
      if (txSent_ >= txLen_) {
        if (now - lastByteMs_ >= KDS_INTERBYTE_MS) {
          kds::reset(parser_);
          st_ = St::WaitResp;
          stMs_ = now;
        }
      } else if (lastByteMs_ == 0 || now - lastByteMs_ >= KDS_INTERBYTE_MS) {
        serial_->write(txBuf_[txSent_++]);
        lastByteMs_ = now;
      }
      break;

    case St::WaitResp: {
      bool done = false;
      while (serial_->available() && !done) {
        int r = kds::feed(parser_, (uint8_t)serial_->read());
        if (r == 1) {
          onFrame();
          done = true;
        } else if (r == -1) {
          kds::reset(parser_);  // resync on garbage
        }
      }
      if (!done && now - stMs_ >= (awaitingStart_ ? KDS_START_TIMEOUT_MS : KDS_TIMEOUT_MS)) {
        fail();
      }
      break;
    }

    case St::Gap:
      if (now - stMs_ >= KDS_POLL_GAP_MS) {
        awaitingStart_ = false;
        curReg_ = kPoll[pollIdx_];
        pollIdx_ = (uint8_t)((pollIdx_ + 1) % sizeof(kPoll));
        uint8_t p[2] = {kds::SID_READ_REG, curReg_};
        startRequest(p, 2);
      }
      break;
  }
}

#endif  // ENABLE_KDS

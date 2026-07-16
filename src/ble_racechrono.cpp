#include "ble_racechrono.h"

#if BT_MODE == BT_MODE_RACECHRONO

#include <NimBLEDevice.h>
#include <math.h>

namespace {

class ServerCallbacks : public NimBLEServerCallbacks {
 public:
  explicit ServerCallbacks(BleRaceChrono* owner) : owner_(owner) {}
  void onConnect(NimBLEServer*) override { owner_->setConnected(true); }
  void onDisconnect(NimBLEServer*) override {
    owner_->setConnected(false);
    NimBLEDevice::startAdvertising();  // stay discoverable
  }

 private:
  BleRaceChrono* owner_;
};

class FilterCallbacks : public NimBLECharacteristicCallbacks {
 public:
  explicit FilterCallbacks(BleRaceChrono* owner) : owner_(owner) {}
  void onWrite(NimBLECharacteristic* c) override {
    std::string v = c->getValue();
    owner_->onFilterWrite((const uint8_t*)v.data(), v.size());
  }

 private:
  BleRaceChrono* owner_;
};

// The RaceChrono DIY protocol is big-endian.
void putU16(uint8_t* p, uint16_t v) {
  p[0] = (uint8_t)(v >> 8);
  p[1] = (uint8_t)(v & 0xFF);
}

void putI32(uint8_t* p, int32_t v) {
  uint32_t u = (uint32_t)v;
  p[0] = (uint8_t)(u >> 24);
  p[1] = (uint8_t)((u >> 16) & 0xFF);
  p[2] = (uint8_t)((u >> 8) & 0xFF);
  p[3] = (uint8_t)(u & 0xFF);
}

void putU24(uint8_t* p, uint32_t v) {
  p[0] = (uint8_t)((v >> 16) & 0xFF);
  p[1] = (uint8_t)((v >> 8) & 0xFF);
  p[2] = (uint8_t)(v & 0xFF);
}

}  // namespace

void BleRaceChrono::begin(const char* deviceName) {
  NimBLEDevice::init(deviceName);
  NimBLEServer* server = NimBLEDevice::createServer();
  server->setCallbacks(new ServerCallbacks(this));

  NimBLEService* service = server->createService(NimBLEUUID((uint16_t)0x1FF8));
  canChar_ = service->createCharacteristic(NimBLEUUID((uint16_t)0x0001),
                                           NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  NimBLECharacteristic* filterChar =
      service->createCharacteristic(NimBLEUUID((uint16_t)0x0002), NIMBLE_PROPERTY::WRITE);
  filterChar->setCallbacks(new FilterCallbacks(this));
  mainChar_ = service->createCharacteristic(NimBLEUUID((uint16_t)0x0003),
                                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  timeChar_ = service->createCharacteristic(NimBLEUUID((uint16_t)0x0004),
                                            NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);
  service->start();

  NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
  adv->addServiceUUID(service->getUUID());
  adv->setScanResponse(true);
  adv->start();

  Serial.printf("[BLE] advertising as \"%s\" (RaceChrono DIY)\n", deviceName);
}

void BleRaceChrono::onFix(const GpsFix& fix, int sats, float hdop, bool hasFix,
                          int year, int month, int day) {
  if (!mainChar_) return;

  // Time characteristic: hours since 2000-01-01, calendar-encoded. The 3-bit
  // sync counter is bumped on every change so the app can pair both packets.
  if (year >= 2000) {
    uint32_t hour = (fix.msOfDay / 3600000UL) % 24UL;
    uint32_t hourDate = (uint32_t)(year - 2000) * 8928UL +
                        (uint32_t)(month - 1) * 744UL +
                        (uint32_t)(day - 1) * 24UL + hour;
    if (hourDate != lastHourDate_) {
      lastHourDate_ = hourDate;
      syncBits_ = (uint8_t)((syncBits_ + 1) & 0x07);
      uint8_t tp[3];
      putU24(tp, ((uint32_t)syncBits_ << 21) | (hourDate & 0x1FFFFF));
      timeChar_->setValue(tp, sizeof(tp));
      if (connected_) timeChar_->notify();
    }
  }

  uint8_t p[20];
  buildMainPacket(p, fix, sats, hdop, hasFix);
  mainChar_->setValue(p, sizeof(p));
  if (connected_) mainChar_->notify();
}

// CAN filter commands from the app: 0 = deny all, 1 = allow all,
// 2 = add one PID (payload: cmd u8, notify interval u16, pid u32).
void BleRaceChrono::onFilterWrite(const uint8_t* data, size_t len) {
  if (len < 1) return;
  switch (data[0]) {
    case 0:
      canAllowAll_ = false;
      canAllowedCount_ = 0;
      break;
    case 1:
      canAllowAll_ = true;
      break;
    case 2:
      if (len >= 7 && canAllowedCount_ < 8) {
        // Byte order of the PID is not clearly specified: store both
        // interpretations, matching either costs nothing.
        uint32_t be = ((uint32_t)data[3] << 24) | ((uint32_t)data[4] << 16) |
                      ((uint32_t)data[5] << 8) | data[6];
        uint32_t le = ((uint32_t)data[6] << 24) | ((uint32_t)data[5] << 16) |
                      ((uint32_t)data[4] << 8) | data[3];
        canAllowed_[canAllowedCount_++] = be;
        if (canAllowedCount_ < 8 && le != be) canAllowed_[canAllowedCount_++] = le;
      }
      break;
    default:
      break;
  }
}

bool BleRaceChrono::canAllowed(uint32_t id) const {
  if (canAllowAll_) return true;
  for (uint8_t i = 0; i < canAllowedCount_; i++) {
    if (canAllowed_[i] == id) return true;
  }
  return false;
}

void BleRaceChrono::sendCan(uint32_t id, const uint8_t* data, uint8_t len) {
  if (!canChar_ || !connected_ || len > 16) return;
  if (!canAllowed(id)) return;
  uint8_t p[20];
  p[0] = (uint8_t)(id & 0xFF);  // packet id, little-endian
  p[1] = (uint8_t)((id >> 8) & 0xFF);
  p[2] = (uint8_t)((id >> 16) & 0xFF);
  p[3] = (uint8_t)((id >> 24) & 0xFF);
  memcpy(p + 4, data, len);
  canChar_->setValue(p, (size_t)(4 + len));
  canChar_->notify();
}

// GPS main data characteristic (0x0003), 20 bytes.
void BleRaceChrono::buildMainPacket(uint8_t out[20], const GpsFix& fix, int sats,
                                    float hdop, bool hasFix) const {
  // Bytes 0-2: 3 sync bits + time from hour start in 2 ms units.
  uint32_t msInHour = fix.msOfDay % 3600000UL;
  uint32_t tfh = (msInHour / 60000UL) * 30000UL +
                 ((msInHour / 1000UL) % 60UL) * 500UL +
                 (msInHour % 1000UL) / 2UL;
  putU24(out, ((uint32_t)syncBits_ << 21) | (tfh & 0x1FFFFF));

  // Byte 3: fix quality (2 bits) + locked satellites (6 bits, 0x3F = unknown).
  uint8_t quality = hasFix ? 1 : 0;
  uint8_t s6 = (sats > 0) ? (uint8_t)(sats > 62 ? 62 : sats) : 0x3F;
  out[3] = (uint8_t)((quality << 6) | s6);

  // Bytes 4-11: latitude/longitude, degrees x 1e7 (0x7FFFFFFF = invalid).
  if (hasFix) {
    putI32(out + 4, (int32_t)llround(fix.lat * 1e7));
    putI32(out + 8, (int32_t)llround(fix.lon * 1e7));
  } else {
    putI32(out + 4, 0x7FFFFFFF);
    putI32(out + 8, 0x7FFFFFFF);
  }

  // Bytes 12-13: altitude, 0.1 m variant: (meters + 500) * 10, high bit clear.
  if (hasFix && fix.altValid) {
    long a = lroundf((fix.altM + 500.0f) * 10.0f);
    if (a < 0) a = 0;
    putU16(out + 12, (uint16_t)(a & 0x7FFF));
  } else {
    putU16(out + 12, 0xFFFF);
  }

  if (hasFix) {
    // Bytes 14-15: speed, 0.01 km/h variant, high bit clear.
    long v = lroundf(fix.speedKmh * 100.0f);
    if (v < 0) v = 0;
    if (v > 0x7FFF) v = 0x7FFF;
    putU16(out + 14, (uint16_t)v);
    // Bytes 16-17: bearing, degrees x 100.
    long b = lroundf(fix.courseDeg * 100.0f) % 36000L;
    if (b < 0) b += 36000L;
    putU16(out + 16, (uint16_t)b);
    // Byte 18: HDOP x 10.
    long h = lroundf(hdop * 10.0f);
    if (h < 0) h = 0;
    if (h > 254) h = 254;
    out[18] = (uint8_t)h;
  } else {
    putU16(out + 14, 0xFFFF);
    putU16(out + 16, 0xFFFF);
    out[18] = 0xFF;
  }
  out[19] = 0xFF;  // VDOP not available (GSA sentences are disabled)
}

#endif  // BT_MODE == BT_MODE_RACECHRONO

#pragma once
#include <Arduino.h>
#include "config.h"
#include "kds_proto.h"

#if ENABLE_KDS

// Drives the K-line (through an L9637D transceiver) with the Kawasaki KDS
// protocol: ISO 14230 fast init, then cyclic register polling. Fully
// non-blocking; retries quietly every KDS_RETRY_MS when the bike is off or
// nothing is wired.
class KdsBus {
 public:
  void begin(HardwareSerial& serial);
  void loop();
  const EcuData& data() const { return data_; }

 private:
  enum class St : uint8_t { Rest, InitIdle, InitLow, InitHigh, Send, WaitResp, Gap };

  void startInit();
  void startRequest(const uint8_t* payload, uint8_t len);
  void onFrame();
  void fail();

  HardwareSerial* serial_ = nullptr;
  EcuData     data_;
  kds::Parser parser_;
  St          st_ = St::Rest;
  uint32_t    stMs_ = 0;
  uint8_t     txBuf_[12];
  uint8_t     txLen_ = 0, txSent_ = 0;
  uint32_t    lastByteMs_ = 0;
  bool        awaitingStart_ = false;
  uint8_t     curReg_ = 0;
  uint8_t     pollIdx_ = 0;
  uint8_t     failCount_ = 0;
};

#endif  // ENABLE_KDS

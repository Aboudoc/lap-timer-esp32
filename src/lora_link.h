#pragma once
#include <Arduino.h>
#include "config.h"

#if ENABLE_LORA

// Bike-side LoRa transmitter. Probes the SX127x quietly: the firmware works
// with or without the radio plugged in.
class LoraLink {
 public:
  void begin();
  bool ok() const { return ok_; }
  // Non-blocking async send; retries the radio probe when absent.
  bool send(const uint8_t* data, size_t len);

 private:
  bool tryBegin();
  bool     ok_ = false;
  uint32_t lastTryMs_ = 0;
};

#endif  // ENABLE_LORA

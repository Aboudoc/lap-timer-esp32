#include "lora_link.h"

#if ENABLE_LORA

#include <LoRa.h>

bool LoraLink::tryBegin() {
  LoRa.setPins(PIN_LORA_CS, PIN_LORA_RST, PIN_LORA_DIO0);
  if (!LoRa.begin(LORA_FREQ_HZ)) return false;
  LoRa.setSyncWord(LORA_SYNC_WORD);
  LoRa.setTxPower(LORA_TX_DBM);
  LoRa.enableCrc();
  Serial.printf("[LORA] radio online, %.1f MHz\n", LORA_FREQ_HZ / 1e6);
  return true;
}

void LoraLink::begin() {
  ok_ = tryBegin();
  lastTryMs_ = millis();
  if (!ok_) Serial.println("[LORA] no radio detected (will keep probing)");
}

bool LoraLink::send(const uint8_t* data, size_t len) {
  if (!ok_) {
    uint32_t now = millis();
    if (now - lastTryMs_ < LORA_RETRY_MS) return false;
    lastTryMs_ = now;
    ok_ = tryBegin();
    if (!ok_) return false;
  }
  if (LoRa.beginPacket() == 0) return false;  // still transmitting the previous one
  LoRa.write(data, len);
  LoRa.endPacket(true);  // async: never stalls the GPS/timing loop
  return true;
}

#endif  // ENABLE_LORA

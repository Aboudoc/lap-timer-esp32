#pragma once
#include <Arduino.h>
#include "config.h"

enum class ButtonEvent { None, Short, Long };

// Bouton vers GND avec pull-up interne. Anti-rebond logiciel.
// L'appui long est signale des que le seuil est atteint (sans attendre
// le relachement) : plus agreable avec des gants.
class Button {
 public:
  void begin(int pin);
  ButtonEvent poll();

 private:
  int      pin_ = -1;
  bool     pressed_ = false;
  bool     longFired_ = false;
  uint32_t pressStart_ = 0;
  uint32_t lastChange_ = 0;
};

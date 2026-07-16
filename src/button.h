#pragma once
#include <Arduino.h>
#include "config.h"

enum class ButtonEvent { None, Short, Long };

// Button to GND with internal pull-up. Software debounce.
// A long press fires as soon as the threshold is reached (without waiting
// for release): nicer with gloves on.
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

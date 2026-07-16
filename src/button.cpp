#include "button.h"

void Button::begin(int pin) {
  pin_ = pin;
  if (pin_ >= 0) pinMode(pin_, INPUT_PULLUP);
}

ButtonEvent Button::poll() {
  if (pin_ < 0) return ButtonEvent::None;
  bool raw = digitalRead(pin_) == LOW;
  uint32_t now = millis();

  if (raw != pressed_) {
    if (now - lastChange_ < DEBOUNCE_MS) return ButtonEvent::None;
    lastChange_ = now;
    pressed_ = raw;
    if (pressed_) {
      pressStart_ = now;
      longFired_ = false;
    } else if (!longFired_ && now - pressStart_ < LONG_PRESS_MS) {
      return ButtonEvent::Short;
    }
  } else if (pressed_ && !longFired_ && now - pressStart_ >= LONG_PRESS_MS) {
    longFired_ = true;
    return ButtonEvent::Long;
  }
  return ButtonEvent::None;
}

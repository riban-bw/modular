#include "switch.h"

Switch::Switch(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
    pinMode(gpi, INPUT_PULLUP);
};

bool Switch::Switch::read(uint32_t now) {
  if (pending)
    return false;
  bool val = !digitalRead(pin);
  if (val != value && now > last_change + DEBOUNCE_TIME) {
    value = val;
    last_change = now;
    pending = true;
  }
  return pending;
};

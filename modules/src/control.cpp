#include "control.h"

Control::Control(uint16_t idx, uint8_t gpi) :
  index(idx),
  pin(gpi) {};

bool Control::read(uint32_t now) {
  return false;
};

void Control::write(uint8_t val) {
};

void Control::reset() {
  value = 0;
  last_change = 0;
  pending = false;
};

int32_t Control::get_value() {
  pending = false;
  return value;
};

void Control::set_value(int16_t) {
};
    
uint8_t Control::get_pin() {
  return pin;
};

uint16_t Control::get_index() {
  return index;
};

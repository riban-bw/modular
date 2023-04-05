#include "adc.h"

ADC::ADC(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
  value = analogRead(gpi);
};

bool ADC::read(uint32_t now) {
  if (pending)
    return false;
  sum += analogRead(pin);
  if (++count > ADC_AVERAGE) {
    int32_t val = sum / ADC_AVERAGE;
    if (abs(value - val) > ADC_HYST) {
        value = val;
        pending = true;
    }
    count = 0;
    sum = 0;
  }
  return pending;
};

void ADC::reset(){
  Control::reset();
  sum = 0;
  count = 0;
};

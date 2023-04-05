#include "led.h"

LED::LED(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
    pinMode(gpi, OUTPUT);
    pending = false;
};

void LED::write(uint32_t val) {
    analogWrite(pin, val);
};

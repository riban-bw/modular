#ifndef DETECT_H
#define DETECT_H

#include <Arduino.h>

void init_detect();
bool detect();
uint8_t getI2cAddress();

#endif // DETECT_H
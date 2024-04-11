#ifndef DETECT_H
#define DETECT_H

#include <Arduino.h>

void initDetect();
bool detect();
uint8_t getI2cAddress();
void getUid(char*);

#endif // DETECT_H
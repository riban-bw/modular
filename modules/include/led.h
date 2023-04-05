#ifndef LED_H
#define LED_H

#include "control.h"

class LED : public Control {
  public:

    LED(uint16_t idx, uint8_t gpi);

    void write(uint32_t val);
};
#endif
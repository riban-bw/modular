#ifndef ADC_H
#define ADC_H
#include "control.h"

// class representing a ADC
class ADC : public Control {
  public:

    ADC(uint16_t idx, uint8_t gpi);

    bool read(uint32_t now);

    void reset();

  private:
    uint32_t sum = 0;
    uint32_t count = 0;
};

#endif
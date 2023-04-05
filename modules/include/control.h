#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include "Arduino.h"

#define DEBOUNCE_TIME 20 // Duration to ignore change of switch state (ms)
#define ADC_HYST 2 // Minimum ADC value change to trigger new value
#define ADC_AVERAGE 8 // Quantity of ADC measurements to average over

class Control {
  public:
    Control(uint16_t idx, uint8_t gpi);
    virtual bool read(uint32_t now);

    void write(uint8_t val);

    void reset();

    int32_t  get_value();

    virtual void set_value(int16_t);
    
    uint8_t get_pin();

    uint16_t get_index();

  protected:
    int32_t value = 0;
    uint32_t last_change = 0;
    uint16_t index;
    uint8_t pin;
    bool pending = false;
};

#endif
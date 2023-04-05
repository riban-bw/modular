#ifndef TOUCH_H
#define TOUCH_H

#include "control.h"

class Touch : public Control {
  public:
    Touch(uint16_t idx, uint8_t gpi);

    bool read(uint32_t now);

    void set_value(int16_t t);

  private:
    uint32_t sched_start = 0; // Time of next charge/discharge start
    uint32_t start_time = 0;
    uint32_t timeout = 3; //!@todo tune sensitivity
    uint32_t threshold = 30;
    uint32_t sum = 0; // Cumulative time of samples
    uint16_t count = 0; // Count of samples collected
    uint16_t repeats = 5; // Quantity of samples to collect
    bool charging = true; // True whilst pin charging
};

#endif
#include "touch.h"

Touch::Touch(uint16_t idx, uint8_t gpi) : Control(idx, gpi) {
}

bool Touch::read(uint32_t now) {
    /*  Charge pin for <timeout>ms
        Discharge until trigger
        Measure discharge time
        Repeat <x> times, adding to cumulative total
        Check if threshold exceded
    */
    if (pending)
    return false;
    if (charging) {
    if(now > sched_start) {
        charging = false;
        sched_start = now;
        pinMode(pin, INPUT);
    }
    return false;
    }
    
    if(digitalRead(pin))
    return false; // Not discharged
    
    // Pin has discharged
    pinMode(pin, INPUT_PULLUP);
    charging = true;
    sched_start = now + timeout;

    sum += now - sched_start;
    if (++count < repeats)
    return false;

    uint16_t val = (sum > threshold);
    if (val != value) {
    value = val;
    last_change = now;
    pending = true;
    sched_start = now + DEBOUNCE_TIME;
    }
    sum = 0;
    return pending;
}

void Touch::set_value(int16_t t) {
    threshold = t;
}


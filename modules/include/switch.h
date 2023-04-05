#ifndef SWITCH_H
#define SWITCH_H

#include <stdint.h>
#include <control.h>

// Class representing a switch
class Switch : public Control {

public:
    Switch(uint16_t idx, uint8_t gpi);

    bool read(uint32_t now);
};

#endif
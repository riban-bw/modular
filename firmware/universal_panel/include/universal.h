#ifndef UNIVERSAL_H
#include <Arduino.h>

#define DEBOUNCE_TIME 20
#define ACCEL_THRES 10
#define ACCEL_FACTOR 10
#define LONG_PRESS_TIME 1200

static const uint8_t ENC_VALID[] = {0,1,1,0,1,0,0,1,1,0,0,1,0,1,1,0}; // Table of valid encoder states
static const char* MIDI_SRC_NAMES[] = {"PITCH", "GATE", "VELOCITY", "A.TOUCH", "P.BEND", "MOD", "CLOCK", "VOLUME", "PAN", "START", "STOP", "CONTINUE"};
static const char* MIDI_DST_NAMES[] = {"PITCH", "GATE", "VELOCITY", "A.TOUCH", "P.BEND", "MOD", "RETRIG", "CLOCK", "CLOCK DIV", "START", "STOP", "CONTINUE"};
static const char* VCA_SRC_NAMES[] = {"OUT1", "OUT2"};
static const char* VCA_DST_NAMES[] = {"CV1", "IN1", "CV2", "IN2"};
static const char* VCA_PARAM_NAMES[] = {"LEVEL1", "LEVEL2", "LINEAR"};
static const char* VCO_SRC_NAMES[] = {"SQUARE", "SAWTOOTH", "TRIANGLE", "SINE"};
static const char* VCO_DST_NAMES[] = {"PITCH", "SYNC", "PW", "FM"};
static const char* VCO_PARAM_NAMES[] = {"TUNE", "FINE", "SLOW", "PM", "FM", "FM TYPE", "LINEAR"};


enum {
    MENU_HOME,
    MENU_MODULES,
    MENU_MODULE,
    MENU_SRCS,
    MENU_DESTS,
    MENU_PARAMS,
    MENU_REMOVE,
    MENU_COUNT
};

enum {
    SWITCH_IDLE,
    SWITCH_PRESS,
    SWITCH_SHORT,
    SWITCH_LONG
};

struct MODULE {
    uint32_t type;
    char name[13];
    uint8_t nSrcs = 0;
    uint8_t nDests = 0;
    uint8_t nParams = 0;
    const char** srcNames;
    const char** dstNames;
    const char** paramNames;
    MODULE(uint32_t t, const char* n) {
        type = t;
        sprintf(name, "%s", n);
        switch(type) {
            case 2:
                // MIDI
                srcNames = MIDI_SRC_NAMES;
                nSrcs = sizeof(MIDI_SRC_NAMES) / sizeof(char*);
                dstNames = MIDI_DST_NAMES;
                nDests = sizeof(MIDI_DST_NAMES) / sizeof(char*);
                break;
            case  11:
                // VCO
                srcNames = VCO_SRC_NAMES;
                nSrcs = sizeof(VCO_SRC_NAMES) / sizeof(char*);
                dstNames = VCO_DST_NAMES;
                nDests = sizeof(VCO_DST_NAMES) / sizeof(char*);
                paramNames = VCO_PARAM_NAMES;
                nParams = sizeof(VCO_PARAM_NAMES) / sizeof(char*);
                break;
            case  15:
                // VCA
                srcNames = VCA_SRC_NAMES;
                nSrcs = sizeof(VCA_SRC_NAMES) / sizeof(char*);
                dstNames = VCA_DST_NAMES;
                nDests = sizeof(VCA_DST_NAMES) / sizeof(char*);
                paramNames = VCA_PARAM_NAMES;
                nParams = sizeof(VCA_PARAM_NAMES) / sizeof(char*);
                break;
        }
    }
};

struct SWITCH {
    uint8_t pin;
    bool state = false;
    uint32_t time = 0;
    SWITCH(uint8_t p) {
        pin = p;
        pinMode(p, INPUT_PULLUP);
    }
};

struct ENCODER {
    uint8_t pinClk;
    uint8_t pinData;
    int32_t value = 0;
    uint8_t code = 0;
    uint8_t count = 0;
    uint32_t time = 0;
    ENCODER(uint8_t clk, uint8_t data) {
        pinClk = clk;
        pinData = data;
        pinMode(clk, INPUT_PULLUP);
        pinMode(data, INPUT_PULLUP);
    }
};


// Declare functions
void drawMenu();
uint8_t processSwitch(SWITCH& sw, uint32_t now);
void startI2c();
void onI2Creceive(int count);
void onI2Crequest();

#endif //UNIVERSAL_H
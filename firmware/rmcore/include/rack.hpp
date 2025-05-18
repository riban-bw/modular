// Define structures required by rack plugins

#pragma once
using namespace std;

#include "global.h"
#include <jack/jack.h> // Provides jack_client_t, jack_port_t, jack_nframes_t
#include <string> // Provides std::string
#include <algorithm> // Provides std::clamp

struct Param {
    float value = 0.0f;

    float getValue() {
        return value;
    }

    void setValue(float value) {
        this->value = value;
    }
};

struct Port {
    jack_port_t* m_port[MAX_POLY]; // Jack input ports (use m_port[0] for non-polyphonic input)
    float m_value[MAX_POLY]; // Current output values
    bool poly = false; // True if polyphonic port
    bool connected = false; // True if connected

    Port(jack_client_t* jackClient, std::string name, uint8_t polyphony, bool input) {
        poly = polyphony != 0;
        if (!poly)
            --polyphony;
        char nameBuffer[128];
        for(uint8_t channel = 0; channel < MAX_POLY + 1; ++channel) {
            if (poly)
                sprintf(nameBuffer, "%s[%u]", name.c_str(), channel + 1);
            else
                sprintf(nameBuffer, "%s", name.c_str());
            if ((channel == 0) || poly && channel < polyphony) {
                m_port[channel] = jack_port_register(jackClient, nameBuffer, JACK_DEFAULT_AUDIO_TYPE, input ? JackPortIsInput : JackPortIsOutput, 0);
            }
            else
                m_port[channel] = nullptr;
        }
    }

    void updateConnected() {
        connected = jack_port_connected(m_port[0]);
    }

    bool isConnected() {
        return connected;
    }

    float getPolyVoltage(uint8_t channel = 0) {
        return getVoltage(channel);
    }

    float getVoltage(uint8_t channel = 0) {
        if (channel < MAX_POLY)
            return m_value[channel];
        return 0.0f;
    }

    void setVoltage(float voltage, uint8_t channel = 0) {
        if (channel < MAX_POLY)
            m_value[channel] = voltage;
    }
};

struct Input : Port {
    Input(jack_client_t* jackClient, std::string name, uint8_t polyphony) :
        Port(jackClient, name, polyphony, true) {}
};

struct Output : Port {
    Output(jack_client_t* jackClient, std::string name, uint8_t polyphony) :
        Port(jackClient, name, polyphony, false) {}
};


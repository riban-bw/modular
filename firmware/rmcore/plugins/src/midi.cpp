/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    MIDI input class implementation.
*/

#include "midi.h"
#include "global.h"
#include <stdio.h>
#include <jack/midiport.h> // provides JACK MIDI interface
#include <cmath>

DEFINE_PLUGIN(MIDI)

MIDI::MIDI() {
    m_info.description = "MIDI to CV";
    m_info.outputs = {
            "cc1",
            "cc2",
            "cc3",
            "cc4",
            "cc5",
            "cc6",
            "cc7",
            "cc8"
    };
    m_info.polyOutputs = {
            "v/oct",
            "gate",
            "velocity"
    };
    m_info.params = {
            "range cc1",
            "range cc2",
            "range cc3",
            "range cc4",
            "range cc5",
            "range cc6",
            "range cc7",
            "range cc8",
            "portamento",
            "legato",
            "channel",
            "range"
    };
    m_info.midiInputs = {
        "input"
    };
}

void MIDI::init() {
    for (uint8_t i = 0; i < MAX_POLY; ++i)
        m_outputValue[i].output = i;
    setParam(MIDI_PARAM_PORTAMENTO, 0.0);
    setParam(MIDI_PARAM_LEGATO, 0.0);
    setParam(MIDI_PARAM_CHANNEL, 0.0);
}

bool MIDI::setParam(uint32_t param, float val) {
    Module::setParam(param, val);
    if (param <= MIDI_PARAM_RANGE_CC8) {
        m_ccRange[param] = val;
        return true;
    }
    switch (param) {
        case MIDI_PARAM_PORTAMENTO:
            m_portamento = 1.0 - val; //!@todo Fix portamento time
            break;
    }
    return true;
}

int MIDI::process(jack_nframes_t frames) {
    // Process MIDI input
    void* midiBuffer = jack_port_get_buffer(m_midiInput[0], frames);
    jack_midi_event_t midiEvent;
    uint8_t chan, cmd, note, val, nextPoly;
    bool found;
    jack_nframes_t count = jack_midi_get_event_count(midiBuffer);
    for (jack_nframes_t event = 0; event < count; event++) {
        jack_midi_event_get(&midiEvent, midiBuffer, event);
        chan = midiEvent.buffer[0] & 0x0F;
        if (chan != m_param[MIDI_PARAM_CHANNEL].value)
            continue;
        cmd = midiEvent.buffer[0] & 0xF0;
        switch (cmd) {
            case 0x90:
                // Note on
                val = midiEvent.buffer[2];
                if (val) {
                    note = midiEvent.buffer[1];

                    // Check if note already pressed and find oldest poly in case we need to steal
                    found = false;
                    uint8_t oldestPoly = 0xff; // Index of oldest used poly output in vector
                    for (uint8_t i = 0; i < m_heldNotes.size(); ++i) {
                        if (m_heldNotes[i].note == note)
                            found = true;
                        if (oldestPoly == 0xff && m_heldNotes[i].output != 0xff)
                            oldestPoly = i;
                    }
                    if (found)
                        continue; // Ignore duplicate note-on

                    // Get available output
                    for (uint8_t poly = 0; poly < m_poly; ++poly) {
                        if (m_outputValue[poly].gate == 0.0) {
                            m_outputValue[poly].note = note;
                            m_outputValue[poly].cv = (note - 60) / 12.0f;
                            m_outputValue[poly].targetCv = (note - 60) / 12.0f;
                            m_outputValue[poly].velocity = val / 127.0f;
                            m_outputValue[poly].gate = 1.0;
                            m_heldNotes.push_back(m_outputValue[poly]);
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        continue; // New note has been assigned to a poly output

                    // No available outputs so steal oldest
                    if (oldestPoly == 0xff)
                        continue; // Shouldn't be here but safety...
                    uint8_t poly = m_heldNotes[oldestPoly].output;
                    m_heldNotes[oldestPoly].output = 0xff;
                    m_outputValue[poly].note = note;
                    m_outputValue[poly].cv = (note - 60) / 12.0f;
                    m_outputValue[poly].targetCv = (note - 60) / 12.0f;
                    m_outputValue[poly].velocity = val / 127.0f;
                    m_outputValue[poly].gate = 1.0;
                    m_heldNotes.push_back(m_outputValue[poly]);
                    break;
                }
                // fall through if val==0 for note off 
            case 0x80:
                // Note off
                note = midiEvent.buffer[1];

                //Check to see if note already exists and if not, ignore it.
                // If note exists, store its output then remove note from list.
                nextPoly = 0xff;
                for (auto it = m_heldNotes.begin(); it != m_heldNotes.end() ; ++it) {
                    if ((*it).note == note) {
                        nextPoly = (*it).output; // Store this poly output for reassignement to previously stolen note
                        m_heldNotes.erase(it); // Remove note
                        break;
                    }
                }
                if (nextPoly == 0xff)
                    continue; // Ignore notes that are not currently asserted

                // If list is longer than MAX_POLY, assign output to entry in list at end - MAX_POLY.
                found = false;
                for (int i = m_heldNotes.size() - 1; i >= 0; --i) {
                    // Search for stolen notes from most recent, backwards
                    if (m_heldNotes[i].output == 0xff) {
                        // Found a stolen note so assin to recently freed output and update corresponding output in array.
                        m_heldNotes[i].output = nextPoly;
                        m_outputValue[nextPoly].cv = m_heldNotes[i].cv;
                        m_outputValue[nextPoly].note = m_heldNotes[i].note;
                        m_outputValue[nextPoly].targetCv = m_heldNotes[i].targetCv;
                        m_outputValue[nextPoly].velocity = m_heldNotes[i].velocity;
                        m_outputValue[nextPoly].gate = m_heldNotes[i].gate;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    // No stolen notes found so clear gate
                    //!@todo May want different behaviour based on legato
                    m_outputValue[nextPoly].gate = 0.0;
                }
                break;
            case 0xb0:
                // CC
                if (midiEvent.buffer[1] < m_ccBase || midiEvent.buffer[1] > m_ccBase + NUM_MIDI_CC)
                    continue;
                m_cc[midiEvent.buffer[1] - m_ccBase] = (float)midiEvent.buffer[2] / 127;
                break;
            case 0xe0:
                // Pitch bend
                m_pitchbend = m_pitchbendRange / 12 * double(midiEvent.buffer[1] + (midiEvent.buffer[2] << 7) - 0x2000) / 0x2000;
                break;
        }
    }
    for (uint8_t poly = 0; poly < m_poly; ++poly) {
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[MIDI_OUTPUT_CV].m_port[poly], frames);
        jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[MIDI_OUTPUT_GATE].m_port[poly], frames);
        jack_default_audio_sample_t * velBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[MIDI_OUTPUT_VEL].m_port[poly], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_outputValue[poly].cv += m_portamento * (m_outputValue[poly].targetCv - m_outputValue[poly].cv);
            cvBuffer[frame] = m_outputValue[poly].cv + m_pitchbend;
            gateBuffer[frame] = m_outputValue[poly].gate;
            velBuffer[frame] = m_outputValue[poly].velocity;
        }
    }
    for (uint8_t cc = 0; cc < NUM_MIDI_CC; ++cc) {
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[cc].m_port[0], frames);
        float ccVal;
        switch (m_ccRange[MIDI_PARAM_RANGE_CC1 + cc]) {
            case MIDI_CC_RANGE_5:
                ccVal = m_cc[cc] * 10 - 5;
                break;
            case MIDI_CC_RANGE_10:
                ccVal = m_cc[cc] * 10;
                break;
            default:
                ccVal = m_cc[cc];
        }
        for (jack_nframes_t frame = 0; frame < frames; ++frame)
            cvBuffer[frame] = ccVal;
    }
    return 0;
}

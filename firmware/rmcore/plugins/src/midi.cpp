/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    MIDI input class implementation.
*/

#include "midi.h"
#include "global.h"
#include "moduleManager.h"
#include <stdio.h>
#include <jack/midiport.h> // provides JACK MIDI interface
#include <cmath>

extern uint8_t g_poly;

void Midi::init() {
    for (uint8_t i = 0; i < MAX_POLY; ++i) {
        m_note[i] = 0xff;
        m_cv[i] = 0.0;
        m_velocity[i] = 0.0;
        m_gate[i] = false;
    }
    setParam(MIDI_PARAM_PORTAMENTO, 0.0);
    setParam(MIDI_PARAM_LEGATO, 0.0);
    setParam(MIDI_PARAM_CHANNEL, 0.0);
}

bool Midi::setParam(uint32_t param, float val) {
    Node::setParam(param, val);
    switch (param) {
        case MIDI_PARAM_PORTAMENTO:
            m_portamento = 1.0 - val; //!@todo Fix portamento time
            break;
        case MIDI_PARAM_POLYPHONY:
            if ((uint8_t)val < MAX_POLY)
                g_poly = (uint8_t)val;
            //!@todo Reconfigure all polyphonic modules
            break;
    }
    fprintf(stderr, "Midi: Set parameter %u to %f\n", param, val);
    return true;
}

int Midi::process(jack_nframes_t frames) {
    // Process MIDI input
    void* midiBuffer = jack_port_get_buffer(m_midiIn, frames);
    jack_midi_event_t midiEvent;
    jack_nframes_t count = jack_midi_get_event_count(midiBuffer);
    uint8_t chan, cmd, note, val, freeNote=0xff;
    uint64_t noteMask;
    for (jack_nframes_t frame = 0; frame < count; frame++) {
        jack_midi_event_get(&midiEvent, midiBuffer, frame);
        chan = midiEvent.buffer[0] & 0x0F;
        cmd = midiEvent.buffer[0] & 0xF0;
        switch (cmd) {
            case 0x90:
                // Note on
                val = midiEvent.buffer[2];
                if (val) {
                    note = midiEvent.buffer[1];
                    if (note < 64) {
                        noteMask = 1U << note;
                        if (noteMask & m_heldNotesLow)
                            continue; // Note already held
                        m_heldNotesLow |= noteMask;
                    } else {
                        noteMask = 1U << (note - 64);
                        if (noteMask & m_heldNotesHigh)
                            continue; // Note already held
                        m_heldNotesHigh |= noteMask;
                    }
                    for (uint8_t poly = 0; poly < g_poly; ++poly) {
                        if (m_note[poly] == note) {
                            freeNote = 0xff;
                            break;
                        }
                        else if (m_note[poly] == 0xff) {
                            freeNote = poly;
                            break;
                        }
                    }
                    if (freeNote == 0xff)
                        freeNote = 0; //!@todo Improve note steal
                    m_note[freeNote] = note;
                    m_velocity[freeNote] = val / 127.0f;
                    m_gate[freeNote] = true;
                    m_targetCv[freeNote] = (note - 60) / 12.0f;
                    break;
                }
                // fall through if val==0 for note off 
            case 0x80:
                // Note off
                note = midiEvent.buffer[1];
                if (note < 64) {
                    noteMask = 1U << note;
                    if (!(noteMask & m_heldNotesLow))
                        continue; // Note note held
                    m_heldNotesLow &= ~noteMask;
                } else {
                    noteMask = 1U << (note - 64);
                    if (!(noteMask & m_heldNotesHigh))
                        continue; // Note note held
                    m_heldNotesHigh &= ~noteMask;
                }
                for (uint8_t poly = 0; poly < g_poly; ++poly) {
                    if (note == m_note[poly]) {
                        m_note[poly] = -1;
                        m_gate[poly] = false;
                        break;
                    }
                }
                break;
        }
    }
    for (uint8_t poly = 0; poly < g_poly; ++poly) {
        jack_default_audio_sample_t * cvBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][MIDI_PORT_CV], frames);
        jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][MIDI_PORT_GATE], frames);
        jack_default_audio_sample_t * velBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_polyOutput[poly][MIDI_PORT_VEL], frames);
        for (jack_nframes_t frame = 0; frame < frames; ++frame) {
            m_cv[poly] += m_portamento * (m_targetCv[poly] - m_cv[poly]);
            cvBuffer[frame] = m_cv[poly];
            gateBuffer[frame] = m_gate[poly] ? 1.0f : 0.0f;
            velBuffer[frame] = m_velocity[poly];
        }
    }
    return 0;
}

ModuleInfo createMidiModuleInfo() {
    ModuleInfo info;
    info.type = "midi";
    info.name = "MIDI Input";
    info.polyOutputs.push_back("cv");
    info.polyOutputs.push_back("gate");
    info.polyOutputs.push_back("vel");
    info.params = {"portamento", "legato", "channel"};
    info.midi = true;
    return info;
}
// Register this module as an available plugin
static RegisterModule<Midi> reg_midi(createMidiModuleInfo());

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

void Midi::init() {
    for (uint8_t i = 0; i < MAX_POLY; ++i) {
        m_note[i] = 0xff;
        m_cv[i] = 0.0;
        m_velocity[i] = 0.0;
        m_gate[i] = false;
    }
}

int Midi::process(jack_nframes_t frames) {
    // Process MIDI input
    void* midiBuffer = jack_port_get_buffer(m_midiIn, frames);
    jack_midi_event_t midiEvent;
    jack_nframes_t count = jack_midi_get_event_count(midiBuffer);
    uint8_t chan, cmd, note, val, freeNote=0xff;
    for (jack_nframes_t i = 0; i < count; i++) {
        jack_midi_event_get(&midiEvent, midiBuffer, i);
        chan = midiEvent.buffer[0] & 0x0F;
        cmd = midiEvent.buffer[0] & 0xF0;
        switch (cmd) {
            case 0x90:
                // Note on
                val = midiEvent.buffer[2];
                if (val) {
                    note = midiEvent.buffer[1];
                    for (uint8_t n = 0; n < MAX_POLY; ++n) {
                        if (m_note[n] == note) {
                            freeNote = 0xff;
                            break;
                        }
                        else if (m_note[n] == 0xff) {
                            freeNote = n;
                            break;
                        }
                    }
                    if (freeNote == 0xff)
                        break; //!@todo Implement note stealing, portamento, etc.
                    m_note[freeNote] = note;
                    m_velocity[freeNote] = val / 127.0f;
                    m_gate[freeNote] = true;
                    m_cv[freeNote] = note / 12.0f;
                    break;
                }
                // fall through if val==0 for note off 
            case 0x80:
                // Note off
                note = midiEvent.buffer[1];
                for (uint8_t n = 0; n < MAX_POLY; ++n) {
                    if (note == m_note[n]) {
                        m_note[n] = -1;
                        m_gate[n] = false;
                        break;
                    }
                }
                break;
        }
    }
    for (uint8_t i = 0; i < MAX_POLY; ++i) {
        jack_default_audio_sample_t * cvBuffer= (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[i * 3], frames);
        jack_default_audio_sample_t * gateBuffer = (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[i * 3 + 1], frames);
        jack_default_audio_sample_t * velBuffer= (jack_default_audio_sample_t*)jack_port_get_buffer(m_output[i * 3 + 2], frames);
        float gate = m_gate[i] ? 1.0 : 0.0;
        for (jack_nframes_t j = 0; j < frames; ++j) {
            cvBuffer[j] = m_cv[i];
            gateBuffer[j] = gate;
            velBuffer[j] = m_velocity[i];
        }
    }
    return 0;
}

ModuleInfo createMidiModuleInfo() {
    ModuleInfo info;
    info.type = "midi";
    info.name = "MIDI Input";
    for (int i = 1; i <= MAX_POLY; ++i) {
        info.outputs.push_back("cv" + std::to_string(i));
        info.outputs.push_back("gate" + std::to_string(i));
        info.outputs.push_back("vel" + std::to_string(i));
    }
    info.midi = true;
    return info;
}
// Register this module as an available plugin
static RegisterModule<Midi> reg_midi(createMidiModuleInfo());

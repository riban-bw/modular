/*  riban modular
    Copyright 2023-2025 riban ltd <info@riban.co.uk>

    This file is part of riban modular.
    riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
    riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
    You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

    Core host application.
*/

#include "global.h"
#include "usart.h"
#include "oscillator.h"
#include "version.h"

#include <cstdio> // Provides printf
#include <getopt.h> // Provides getopt_long for command line parsing
#include <unistd.h> // Provides usleep
#include <alsa/asoundlib.h> // Provides ALSA interface
#include <thread> // Provides threading
#include <cmath> // Provides sin, etc.

#define PCM_DEVICE "default"
#define SAMPLERATE 48000
#define FRAMES 256

enum VERBOSE {
    VERBOSE_SILENT  = 0,
    VERBOSE_ERROR   = 1,
    VERBOSE_INFO    = 2,
    VERBOSE_DEBUG   = 3
};

uint8_t g_verbose = VERBOSE_INFO;
const char* swState[] = {"Release", "Press", "Bold", "Long", "", "Long"};
snd_pcm_t *g_pcmPlayback = nullptr; // Pointer to PCM playback handle
float* g_aoutBuffer; // Sample buffer for audio input
snd_pcm_uframes_t g_frames; // Quantity of frames per period
bool g_running = true; // False to stop processing
uint32_t g_now = 0; // Monotonic counter of elapsed frames
uint8_t g_waveform = -1;
Oscillator g_oscillator(SAMPLERATE);

/*  TODO
    Initialise display
    Initialise audio: check for audio interfaces (may be USB) and mute outputs 
    Obtain list of panels
    Initialise each panel: Get info, check firmware version, show status via panel LEDs
    Instantiate modules based on connected hardware panels
    Reload last state including internal modules, binary (button) states, routing, etc.
    Start audio processing thread
    Unmute outputs
    Show info on display, e.g. firmware updates available, current patch name, etc.
    Start program loop:
        Check CAN message queue
        Add / remove panels / modules
        Adjust signal routing
        Adjust parameter values
        Periodic / event driven persistent state save
*/

void debug(const char *format, ...) {
    if (g_verbose >= VERBOSE_DEBUG) {
      va_list args;
      va_start(args, format);
      vfprintf(stderr, format, args);
      va_end(args);
    }
  }
  
  void info(const char *format, ...) {
    if (g_verbose >= VERBOSE_INFO) {
      va_list args;
      va_start(args, format);
      vfprintf(stdout, format, args);
      va_end(args);
    }
  }
  
  void error(const char *format, ...) {
    if (g_verbose >= VERBOSE_ERROR) {
      va_list args;
      va_start(args, format);
      fprintf(stderr, "ERROR: ");
      vfprintf(stderr, format, args);
      va_end(args);
    }
  }
void print_version() {
    info("%s %s (%s) Copyright riban ltd 2023-%s\n", PROJECT_NAME, PROJECT_VERSION, BUILD_DATE, BUILD_YEAR);
}

void print_help() {
    print_version();
    info("Usage: rmcore <options>\n");
    info("\t-v --version\tShow version\n");
    info("\t-V --verbose\tSet verbose level (0:silent, 1:error, 2:info, 3:debug\n");
    info("\t-h --help\tShow this help\n");
    info("\t-w --waveform\tPlay a waveform (0:sin 1:tri 2:saw 3:square 4:noise)\n");
}

bool parseCmdline(int argc, char** argv) {
    static struct option long_options[] = {
        {"verbose", no_argument, 0, 'V'},
        {"version", no_argument, 0, 'v'},
        {"waveform", no_argument, 0, 'w'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    int opt, option_index;
    while ((opt = getopt_long (argc, argv, "hvV:w:?", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'V': 
                if (optarg)
                    g_verbose = atoi(optarg);
                break;
            case 'w':
                if (optarg)
                    g_waveform = atoi(optarg);
                break;
            case '?':
            case 'h': print_help(); return true;
            case 'v': print_version(); return true;
        }
    }
    return false;
}

void processAudio() {
    //!@todo Process each plugin

    static uint32_t freq = 100;
    static float buffer1[FRAMES];
    static float buffer2[FRAMES];
    static double pos = 0;
    static double pos1 = 0;
    static double pos2 = 0;
    static double pwm = 0.5;

    switch (g_waveform) {
        case 0:
        case 1:
        case 2:
        case 3:
        case 4:
            pos = g_oscillator.populateBuffer(g_aoutBuffer, g_frames, g_waveform, pos, freq, 0.5);
            break;
        case 5:
            // Sharktooth: 20% sawtooth, 80% triangle
            pos = g_oscillator.populateBuffer(g_aoutBuffer, g_frames, 1, pos, freq, 0.6);
            pos1 = g_oscillator.populateBuffer(buffer1, g_frames, 2, pos1, freq, 0.2);
            for (uint32_t i = 0; i < g_frames; ++i)
                g_aoutBuffer[i] += buffer1[i];
            break;
        case 6:
            // Square with PWM
            pos = g_oscillator.square(g_aoutBuffer, g_frames, pos, freq, pwm, 0.5);
            if (pwm < 0.9)
                pwm += 0.01;
            else
                pwm = 0.1;
            break;
        case 7:
            // Blade: 2 sawtooth, 1 octave separation
            pos = g_oscillator.populateBuffer(g_aoutBuffer, g_frames, 2, pos, freq, 0.4);
            pos1 = g_oscillator.populateBuffer(buffer1, g_frames, 2, pos1, freq*2, 0.4);
            for (uint32_t i = 0; i < g_frames; ++i)
                g_aoutBuffer[i] += buffer1[i];
            break;
    }

    if (freq < 10000)
        freq += 1;
    else
        freq = 100;
    if (freq % 500 == 0)
        fprintf(stderr, "%u Hz\n", freq);

    //std::this_thread::sleep_for(std::chrono::milliseconds(1));
}

void audioThread() {
    // Thread to handle PCM audio output

    info("ALSA library version: %s\n",SND_LIB_VERSION_STR);
    snd_pcm_t *handle;
    snd_pcm_hw_params_t *params;
    snd_pcm_sframes_t available;
    int rc = snd_pcm_open(&g_pcmPlayback, PCM_DEVICE, SND_PCM_STREAM_PLAYBACK, 0);
    if (rc < 0) {
        error("Failed to open audio device: %s\n", snd_strerror(rc));
        g_running = false;
    } else {
        // Allocate hardware parameter objects
        snd_pcm_hw_params_alloca(&params);
        // Set playback hardware parameters
        snd_pcm_hw_params_any(g_pcmPlayback, params);
        snd_pcm_hw_params_set_access(g_pcmPlayback, params, SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(g_pcmPlayback, params, SND_PCM_FORMAT_FLOAT_LE);
        snd_pcm_hw_params_set_channels(g_pcmPlayback, params, 1);
        snd_pcm_hw_params_set_rate(g_pcmPlayback, params, SAMPLERATE, 0);
        snd_pcm_hw_params_set_period_size(g_pcmPlayback, params, FRAMES, 0);
        snd_pcm_hw_params(g_pcmPlayback, params);
        // Allocate buffer
        g_frames = FRAMES;
        snd_pcm_hw_params_get_period_size(params, &g_frames, 0);
        g_aoutBuffer = (float *) malloc(g_frames * sizeof(float));

        while(g_running) {
            if (snd_pcm_wait(g_pcmPlayback, 1000) > 0) {
                processAudio();
                int pcm = snd_pcm_writei(g_pcmPlayback, g_aoutBuffer, g_frames);
                if (pcm == -EPIPE) {
                    error("Underrun occurred\n");
                    snd_pcm_prepare(g_pcmPlayback);
                } else if (pcm < 0) {
                    error("writing to PCM device: %s\n", snd_strerror(pcm));
                }
            }
        }

        free(g_aoutBuffer);
        snd_pcm_drain(g_pcmPlayback);
        snd_pcm_close(g_pcmPlayback);
    }
}

int main(int argc, char** argv) {
    if(parseCmdline(argc, argv)) {
        // Error parsing command line
        return -1;
    }
    info("Starting riban modular core...\n");

    // Start audio thread
    std::thread aThread(audioThread);

    /*@todo
        Start background panel detection
        Connect audio interface
        Start module manager (add/remove modules, update module state)
        Start connection manager 
        Start config manager (persistent configuration storage)
        Start audio processing
    */

    uint8_t txData[8];
    double value;
    int rxLen;
    USART usart("/dev/ttyS0", B1152000);
    if (!usart.isOpen()) {
//        return -1;
    }
    usart.txCmd(HOST_CMD_PNL_INFO);

    // Main program loop
    while(g_running) {
        rxLen = usart.rx();
        if (rxLen > 0) {
            switch (usart.getRxId() & 0x0f) {
                case CAN_MSG_ADC:
                    value = (usart.rxData[2] | (usart.rxData[3] << 8)) / 1019.0;
                    //printf("Panel %u ADC %u: %0.03f - %u\n", usart.rxData[0], usart.rxData[1] + 1, value, int(value * 255.0));
                    txData[0] = usart.rxData[1];
                    txData[1] = LED_MODE_ON;
                    txData[2] = int(255.0 * value);
                    txData[3] = int(255.0 * value);
                    txData[4] = int(255.0 * value);
                    usart.txCAN(usart.rxData[0], CAN_MSG_LED, txData, 5);
                    break;
                case CAN_MSG_SWITCH:
                    if (usart.rxData[2] < 6)
                        info("Panel %u Switch %u: %s\n", usart.rxData[0], usart.rxData[1] + 1, swState[usart.rxData[2]]);
                    txData[0] = usart.rxData[1];
                    switch(usart.rxData[2]) {
                        case 0:
                            //release
                            txData[1] = 0;
                            break;
                        case 1:
                            //press
                            txData[1] = 1;
                            break;
                        case 2:
                            //bold
                            txData[1] = 6;
                            break;
                        case 3:
                        case 5:
                            //long
                            txData[1] = 2;
                            break;
                    }
                    usart.txCAN(usart.rxData[0], CAN_MSG_LED, txData, 2);
                    break;
                case CAN_MSG_QUADENC:
                    info("Panel %u Encoder %u: %d\n", usart.rxData[0], usart.rxData[1] + 1, usart.rxData[2]);
                    break;
            }
        } else {
            usleep(1000); // 1ms sleep to avoid tight loop
        }
    }

    // Wait for playback to complete
    aThread.join();
    return 0;
}

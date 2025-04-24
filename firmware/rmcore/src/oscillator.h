#include <cstdint> // Provides fixed sized integer types

#define SAMPLERATE 48000
#define PI 3.141592653589793238462643383279502884L
#define WAVETABLE_FREQ 20
#define WAVETABLE_SIZE SAMPLERATE / WAVETABLE_FREQ

enum WAVEFORM {
    WAVEFORM_SIN    = 0,
    WAVEFORM_TRI    = 1,
    WAVEFORM_SAW    = 2,
    WAVEFORM_SQU    = 3,
    WAVEFORM_NOISE  = 4,
};

class Oscillator {

    public:
        Oscillator();
        double populateBuffer(float* buffer, uint32_t frames, uint32_t waveform, double pos, double freq, double amp=1.0);

    private:
        void buildWavetables();

        float m_wavetable[5][WAVETABLE_SIZE];
};

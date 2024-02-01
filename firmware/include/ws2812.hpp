#include "FastLED.h" // Provides WS2812 LED interface
#ifndef WSLEDS
#define WSLEDS 0
#endif //WSLEDS
#ifndef SWITCHES
#define SWITCHES 0
#endif //SWITCHES
#ifndef ADCS
#define ADCS 0
#endif //ADCS
#define LED_DATA_PIN PB15
#define REFRESH_RATE 60
#define PULSE_RATE REFRESH_RATE / 8
#define PULSE_RATE_FAST PULSE_RATE * 4

enum LED_STATES
{
    LED_STATE_OFF,
    LED_STATE_DIM,
    LED_STATE_ON,
    LED_STATE_ON2,
    LED_STATE_FLASHING,
    LED_STATE_FAST_FLASHING,
    LED_STATE_PULSING,
    LED_STATE_FAST_PULSING,
    LED_STATE_QUANTITY
};

struct LED
{
    uint8_t state = LED_STATE_OFF;
    CRGB rgb1 = CRGB::White;
    CRGB rgb2 = CRGB::Black;
    CRGB dim = CRGB::Grey;
    CHSV hsv1 = rgb2hsv_approximate(rgb1);
    CHSV hsv2 = rgb2hsv_approximate(rgb2);
};

// Arrays to hold led config
LED leds[WSLEDS];        // LED config indexed by LED
CRGB ledColours[WSLEDS]; // LED colour indexed by LED

/*  @brief  Initiate WS2812 LEDs
 */
void initLeds()
{
    // Initialise and clear LEDs
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(&ledColours[0], WSLEDS);
    FastLED.showColor(CRGB::Black);
}

/*  @brief  Calculate pulse pulse colour
    @param  hsv1 Colour 1
    @param  hsv2 Colour 2
    @param  phase Percentage of fade (0=colour2, 100=colour1)
    @return Calculated colour
*/
static inline CRGB getFade(CHSV hsv1, CHSV hsv2, uint8_t phase) {
    CHSV hsv = hsv2;
    int16_t tmp;

    tmp = phase * (hsv1.h - hsv2.h) / 100;
    hsv.h += int8_t(tmp);

    tmp = phase * (hsv1.s - hsv2.s) / 100;
    if (tmp > 0)
        hsv.s += int8_t(tmp);
    else
        hsv.s -= int8_t(tmp);

    tmp = phase * (hsv1.v - hsv2.v) / 100;
    if (tmp > 0)
        hsv.v += int8_t(tmp);
    else
        hsv.v -= int8_t(tmp);

    CRGB rgb;
    hsv2rgb_spectrum(hsv, rgb);
    return rgb;
}

/*  @brief Set LED colour 1
    @param r Red
    @param g Green
    @param b Blue
*/
void setLedColour1(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    if (led >= WSLEDS)
        return;
    leds[led].rgb1 = CRGB(r, g, b);
    leds[led].hsv1 = rgb2hsv_approximate(leds[led].rgb1);
    leds[led].dim = leds[led].rgb1;
    nscale8(&leds[led].dim, 1, 20);
}

/*  @brief Set LED colour 2
    @param led LED index
    @param r Red
    @param g Green
    @param b Blue
*/
void setLedColour2(uint8_t led, uint8_t r, uint8_t g, uint8_t b)
{
    if (led >= WSLEDS)
        return;
    leds[led].rgb2 = CRGB(r, g, b);
    leds[led].hsv2 = rgb2hsv_approximate(leds[led].rgb2);
}

/*  @brief Set LED state
    @param led LED index
    @param state LED state (@see LED_STATES)
*/
void setLedState(uint8_t led, uint8_t state)
{
    if (led >= WSLEDS || state >= LED_STATE_QUANTITY)
        return;
    leds[led].state = state;
}

/*  @brief  Process LED animations
    @note   Call periodically every PULSE_RATE ms
*/
void processLeds()
{
    static int8_t phase = 100;
    static int8_t dPhase = -PULSE_RATE;
    static int8_t phaseFast = 100;
    static int8_t dPhaseFast = -PULSE_RATE_FAST;

    // Update slow flash / pulse
    phase += dPhase;
    if (phase <= 0)
    {
        phase = 0;
        dPhase = PULSE_RATE;
    }
    else if (phase >= 100)
    {
        phase = 100;
        dPhase = -PULSE_RATE;
    }

    // Update fast flash / pulse
    phaseFast += dPhaseFast;
    if (phaseFast <= 0)
    {
        phaseFast = 0;
        dPhaseFast = PULSE_RATE_FAST;
    }
    else if (phaseFast >= 100)
    {
        phaseFast = 100;
        dPhaseFast = -PULSE_RATE_FAST;
    }

    bool phaseOff = phase < 100 / 2;
    bool phaseFastOff = phaseFast < 100 / 2;

    CRGB colour;
    for (uint8_t i = 0; i < WSLEDS; ++i)
    {
        LED *led = &leds[i]; // LED object
        CRGB &ledColour = ledColours[i]; // Current WS2812 colour 
        switch (led->state) {
            case LED_STATE_OFF:
                ledColour = CRGB::Black;
                break;
            case LED_STATE_DIM:
                ledColour = led->dim;
                break;
            case LED_STATE_ON:
                ledColour = led->rgb1;
                break;
            case LED_STATE_ON2:
                ledColour = led->rgb2;
                break;
            case LED_STATE_FLASHING:
                ledColour = phaseOff?led->rgb2:led->rgb1;
                break;
            case LED_STATE_FAST_FLASHING:
                ledColour = phaseFastOff?led->rgb2:led->rgb1;
                break;
            case LED_STATE_PULSING:
                ledColour = getFade(led->hsv1, led->hsv2, phase);
                break;
            case LED_STATE_FAST_PULSING:
                ledColour = getFade(led->hsv1, led->hsv2, phaseFast);
                break;
        }
    }
    FastLED.show();
}

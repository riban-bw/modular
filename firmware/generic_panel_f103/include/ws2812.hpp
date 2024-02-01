#include "FastLED.h" // Provides WS2812 LED interface
#define NUM_LEDS 8
#define LED_DATA_PIN PB15
#define PULSE_DEPTH_MAX 200
#define PULSE_DEPTH_MIN 20
#define REFRESH_RATE 60
#define PULSE_RATE REFRESH_RATE / 4
#define PULSE_RATE_FAST REFRESH_RATE

enum LED_STATES
{
    LED_STATE_OFF,
    LED_STATE_DIM,
    LED_STATE_ON,
    LED_STATE_FLASHING,
    LED_STATE_FAST_FLASHING,
    LED_STATE_PULSING,
    LED_STATE_FAST_PULSING,
    LED_STATE_QUANTITY
};

enum LED_TYPES
{
    LED_TYPE_SOURCE,
    LED_TYPE_DESTINATION,
    LED_TYPE_TOGGLE,
    LED_TYPE_INFO,
    LED_TYPE_QUANTITY
};

enum LED_TYPE_COLOURS
{
    LED_TYPE_COLOUR_DESTINATION = CRGB::Purple,
    LED_TYPE_COLOUR_SOURCE = CRGB::Green,
    LED_TYPE_COLOUR_TOGGLE = CRGB::Red,
    LED_TYPE_COLOUR_INFO = CRGB::Blue
};

struct LED
{
    uint8_t state = LED_STATE_OFF;
    uint8_t type = LED_TYPE_TOGGLE;
};

// Define LED colours
CRGB LED_COLOUR_OFF = CRGB::Black;
CRGB ledTypeColours[] = {
    LED_TYPE_COLOUR_DESTINATION,
    LED_TYPE_COLOUR_SOURCE,
    LED_TYPE_COLOUR_TOGGLE,
    LED_TYPE_COLOUR_INFO};                     // Base LED colour, indexed by type
CRGB ledColoursDim[LED_TYPE_QUANTITY];         // Low intensity LED colour, indexed by type
CRGB ledPulsingColours[LED_TYPE_QUANTITY];     // LED colour (intensity) when pulsing, indexed by type
CRGB ledFastPulsingColours[LED_TYPE_QUANTITY]; // LED colour (intensity) when fast pulsing, indexed by type

// Arrays to hold led config
LED leds[NUM_LEDS];        // LED config indexed by LED
CRGB ledColours[NUM_LEDS]; // LED colour indexed by LED

/*  @brief  Initiate WS2812 LEDs
 */
void initLeds()
{
    for (uint8_t type = 0; type < LED_TYPE_QUANTITY; ++type)
    {
        CRGB colour = ledTypeColours[type];
        ledColoursDim[type] = colour.nscale8(20);
    }
    // Initialise and clear LEDs
    FastLED.addLeds<WS2812B, LED_DATA_PIN, GRB>(&ledColours[0], NUM_LEDS);
    FastLED.showColor(CRGB::Black);
}

/*  @brief Set LED type
    @param led LED index
    @param type LED type (@see LED_TYPES)
*/
void setLedType(uint8_t led, uint8_t type)
{
    if (led >= NUM_LEDS || type >= LED_TYPE_QUANTITY)
        return;
    leds[led].type = type;
}

/*  @brief Set LED state
    @param led LED index
    @param state LED state (@see LED_STATES)
*/
void setLedState(uint8_t led, uint8_t state)
{
    if (led >= NUM_LEDS || state >= LED_STATE_QUANTITY)
        return;
    leds[led].state = state;
}

/*  @brief  Process LED animations
    @note   Call periodically every PULSE_RATE ms
*/
void processLeds()
{
    static int16_t phase = PULSE_DEPTH_MAX;
    static int8_t dPhase = -PULSE_RATE;
    static int16_t phaseFast = PULSE_DEPTH_MAX;
    static int8_t dPhaseFast = -PULSE_RATE_FAST;

    // Update slow flash / pulse
    phase += dPhase;
    if (phase <= PULSE_DEPTH_MIN)
    {
        phase = PULSE_DEPTH_MIN;
        dPhase = PULSE_RATE;
    }
    else if (phase >= PULSE_DEPTH_MAX)
    {
        phase = PULSE_DEPTH_MAX;
        dPhase = -PULSE_RATE;
    }

    // Update fast flash / pulse
    phaseFast += dPhaseFast;
    if (phaseFast <= PULSE_DEPTH_MIN)
    {
        phaseFast = PULSE_DEPTH_MIN;
        dPhaseFast = PULSE_RATE_FAST;
    }
    else if (phaseFast >= PULSE_DEPTH_MAX)
    {
        phaseFast = PULSE_DEPTH_MAX;
        dPhaseFast = -PULSE_RATE_FAST;
    }

    bool phaseOff = phase < PULSE_DEPTH_MAX / 2;
    bool phaseFastOff = phaseFast < PULSE_DEPTH_MAX / 2;

    // Calculate colours for current phase
    for (uint8_t type = 0; type < LED_TYPE_QUANTITY; ++type)
    {
        CRGB colour = ledTypeColours[type];
        ledPulsingColours[type] = colour.nscale8(phase);
        colour = ledTypeColours[type];
        ledFastPulsingColours[type] = colour.nscale8(phaseFast);
    }

    // Assign individual LED colour pointers to relevant colour objects
    for (uint8_t i = 0; i < NUM_LEDS; ++i)
    {
        LED *led = &leds[i];
        CRGB &ledColour = ledColours[i];
        switch (led->state)
        {
        case LED_STATE_OFF:
            ledColour = LED_COLOUR_OFF;
            break;
        case LED_STATE_DIM:
            ledColour = ledColoursDim[led->type];
            break;
        case LED_STATE_ON:
            ledColour = ledTypeColours[led->type];
            break;
        case LED_STATE_FLASHING:
            if (phaseOff)
                ledColour = ledColoursDim[led->type];
            else
                ledColour = ledTypeColours[led->type];
            break;
        case LED_STATE_FAST_FLASHING:
            if (phaseFastOff)
                ledColour = ledColoursDim[led->type];
            else
                ledColour = ledTypeColours[led->type];
            break;
        case LED_STATE_PULSING:
            ledColour = ledPulsingColours[led->type];
            break;
        case LED_STATE_FAST_PULSING:
            ledColour = ledFastPulsingColours[led->type];
            break;
        }
    }
    FastLED.show();
}

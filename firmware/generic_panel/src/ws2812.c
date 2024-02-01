/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Impelentation of WS2812 device driver using libopencm3 library
*/

#include "ws2812.h"
#include "panel_types.h"
#include <string.h> // provides bzero
#include <stdlib.h> // provides malloc, free
#include <libopencm3/stm32/rcc.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/timer.h>
#include <libopencm3/stm32/dma.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/stm32/f0/nvic.h>

// #defines
#define WS2812_PWM_0 20 // PWM duty for logic 0
#define WS2812_PWM_1 40 // PWM duty for logic 1
#define DATA_SIZE num_leds * 24 + 50

enum WS2812_STATE_FLAGS {
    WS2812_STATE_INIT       = 1, // Initialised
    WS2812_STATE_PENDING    = 2, // Change of LED mode
    WS2812_STATE_BUSY       = 4  // Transmitting data
};

// Global variables
uint8_t* ws2812_pwm_data = NULL; // Pointer to PWM data
struct WS2812_LED* ws2812_leds = NULL; // Pointer to LED config
volatile uint8_t ws2812_state = 0; // Current state of the lib (see WS2812_STATE_FLAGS)
uint16_t num_leds = 0; // Quantity of LEDs being controlled
uint32_t animation_rate = 1000; // Steps to complete one animation, e.g. pulse

void ws2812_init(uint16_t leds)
{
    // Configure TIM2
    rcc_periph_clock_enable(RCC_TIM2);
    timer_disable_counter(TIM2);
    timer_disable_preload(TIM2);
    timer_set_period(TIM2, WS2812_PWM_0 + WS2812_PWM_1 - 1);
    timer_set_prescaler(TIM2, 0);
    timer_set_repetition_counter(TIM2, 0);
    timer_disable_oc_output(TIM2, TIM_OC1);
    timer_reset_output_idle_state(TIM2, TIM_OC1);
    timer_reset_output_idle_state(TIM2, TIM_OC1N);
    timer_set_oc_mode(TIM2, TIM_OC1, TIM_OCM_PWM1);
    timer_set_oc_value(TIM2, TIM_OC1, 0); // Set PWM duty to zero ???
    timer_set_oc_polarity_high(TIM2, TIM_OC1);
    timer_set_oc_polarity_high(TIM2, TIM_OC1N);
    timer_enable_oc_preload(TIM2, TIM_OC1);
    timer_enable_irq(TIM2, TIM_DIER_CC1DE); // Enable DMA on output compare channel 1
    timer_enable_oc_output(TIM2, TIM_OC1N); // Enable output compare
    timer_enable_break_main_output(TIM2); // Enable main output
    timer_generate_event(TIM2, TIM_EGR_UG); // Load config into device

    // Configure DMA1
    rcc_periph_clock_enable(RCC_DMA1);
    dma_channel_reset(DMA1, DMA_CHANNEL5); // Reset DMA config
    dma_disable_channel(DMA1, DMA_CHANNEL5);
    dma_set_read_from_memory(DMA1, DMA_CHANNEL5); // Memory to peripheral
    dma_disable_peripheral_increment_mode(DMA1, DMA_CHANNEL5);
    dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL5); // Transfer from sequential addresses
    dma_set_peripheral_size(DMA1, DMA_CHANNEL5, DMA_CCR_PSIZE_16BIT); // Pulse width (CCR) register is 16-bit
    dma_set_memory_size(DMA1, DMA_CHANNEL5, DMA_CCR_MSIZE_8BIT); // Data is stored as byte array
    dma_set_priority(DMA1, DMA_CHANNEL5, DMA_CCR_PL_LOW); // Low priority
    dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL5);
    dma_enable_transfer_error_interrupt(DMA1, DMA_CHANNEL5);
    nvic_set_priority(NVIC_DMA1_CHANNEL4_7_DMA2_CHANNEL3_5_IRQ, 0);
    nvic_enable_irq(NVIC_DMA1_CHANNEL4_7_DMA2_CHANNEL3_5_IRQ);

    // Configure GPIO B15
    rcc_periph_clock_enable(RCC_GPIOB);
    gpio_set_mode(GPIOB, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO15);

    free(ws2812_pwm_data);
    free(ws2812_leds);
    num_leds = leds;
    ws2812_pwm_data = malloc(DATA_SIZE);
    if (ws2812_pwm_data == NULL) {
        //!@todo Handle failure to assign memory
    }
    ws2812_leds = malloc(num_leds * sizeof(struct WS2812_LED));
    if (ws2812_leds == NULL) {
        //!@todo Handle failure to assign memory
    }
    bzero(ws2812_leds, num_leds * sizeof(struct WS2812_LED));

    ws2812_state = WS2812_STATE_INIT;
    // Populate PWM data and extinguish all LEDs
    bzero(ws2812_pwm_data, DATA_SIZE);
    ws2812_set_all(0, 0, 0);
    ws2812_send();
}

void ws2812_set(uint16_t led, uint8_t red, uint8_t green, uint8_t blue) {
        if (led >= num_leds || !ws2812_state)
                return;
        ++led;
        uint32_t colour = (green << 16) | (red << 8) | blue;
    if (colour == ws2812_leds[led].value)
        return;
    for (int i = 23; i >=0; --i) {
                if (colour & (1 << i))
                        ws2812_pwm_data[led * 24 - i] = WS2812_PWM_1;
                else
                        ws2812_pwm_data[led * 24 - i] = WS2812_PWM_0;
        }
    ws2812_leds[led].value = colour;
    ws2812_state |= WS2812_STATE_PENDING;
}

void ws2812_set_all(uint8_t red, uint8_t green, uint8_t blue) {
    for (uint16_t i = 0; i < num_leds; ++i)
        ws2812_set(i, red, green, blue);
}

void ws2812_set_colour_1(uint16_t led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led >= num_leds)
        return;
    ws2812_leds[led].red1 = red;
    ws2812_leds[led].green1 = green;
    ws2812_leds[led].blue1 = blue;
    ws2812_leds[led].dRed = ws2812_leds[led].red2 - ws2812_leds[led].red1;
    ws2812_leds[led].dGreen = ws2812_leds[led].green2 - ws2812_leds[led].green1;
    ws2812_leds[led].dBlue = ws2812_leds[led].blue2 - ws2812_leds[led].blue1;
}

void ws2812_set_colour_2(uint16_t led, uint8_t red, uint8_t green, uint8_t blue) {
    if (led >= num_leds)
        return;
    ws2812_leds[led].red2 = red;
    ws2812_leds[led].green2 = green;
    ws2812_leds[led].blue2 = blue;
    ws2812_leds[led].dRed = ws2812_leds[led].red2 - ws2812_leds[led].red1;
    ws2812_leds[led].dGreen = ws2812_leds[led].green2 - ws2812_leds[led].green1;
    ws2812_leds[led].dBlue = ws2812_leds[led].blue2 - ws2812_leds[led].blue1;
}

void ws2812_set_mode(uint16_t led, uint8_t mode) {
    if (led >= num_leds)
        return;
    ws2812_leds[led].mode = mode;
    switch (mode) {
        case WS2812_MODE_OFF:
            ws2812_set(led, 0, 0, 0);
            break;
        case WS2812_MODE_ON1:
            ws2812_set(led, ws2812_leds[led].red1, ws2812_leds[led].green1, ws2812_leds[led].blue1);
            break;
        case WS2812_MODE_ON2:
            ws2812_set(led, ws2812_leds[led].red2, ws2812_leds[led].green2, ws2812_leds[led].blue2);
            break;
    }
}

void ws2812_refresh(uint32_t now) {
    uint8_t slow = (now % 2000) > 1000;
    uint8_t fast = (now % 200) > 100;
    float dt_slow = ((float)(now % 1000)) / 999;
    float dt_fast = ((float)(now % 200)) / 199;
    if (slow)
        dt_slow = 1 - dt_slow;
    if (fast)
        dt_fast = 1 - dt_fast;
    for (uint16_t led = 0; led < num_leds; ++led) {
        switch (ws2812_leds[led].mode) {
            case WS2812_MODE_SLOW_FLASH:
                if (slow)
                    ws2812_set(led, ws2812_leds[led].red1, ws2812_leds[led].green1, ws2812_leds[led].blue1);
                else
                    ws2812_set(led, ws2812_leds[led].red2, ws2812_leds[led].green2, ws2812_leds[led].blue2);
                break;
            case WS2812_MODE_FAST_FLASH:
                if (fast)
                    ws2812_set(led, ws2812_leds[led].red1, ws2812_leds[led].green1, ws2812_leds[led].blue1);
                else
                    ws2812_set(led, ws2812_leds[led].red2, ws2812_leds[led].green2, ws2812_leds[led].blue2);
                break;
            case WS2812_MODE_SLOW_PULSE:
                ws2812_set(led, ws2812_leds[led].red1 + ws2812_leds[led].dRed * dt_slow,
                    ws2812_leds[led].green1 + ws2812_leds[led].dGreen * dt_slow,
                    ws2812_leds[led].blue1 + ws2812_leds[led].dBlue * dt_slow);
                    break;
            case WS2812_MODE_FAST_PULSE:
                ws2812_set(led, ws2812_leds[led].red1 + ws2812_leds[led].dRed * dt_fast,
                    ws2812_leds[led].green1 + ws2812_leds[led].dGreen * dt_fast,
                    ws2812_leds[led].blue1 + ws2812_leds[led].dBlue * dt_fast);
                break;
        }
    }
    ws2812_send();
}

void ws2812_send(void) {
    if (ws2812_state | WS2812_STATE_INIT | WS2812_STATE_PENDING) {
        while (ws2812_state & WS2812_STATE_BUSY)
            ; // Wait for previous transmission to complete
        dma_disable_channel(DMA1, DMA_CHANNEL5); // Can't change config whilst DMA enabled
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL5, DMA_GIF); // Clear all interrupt flags
        dma_set_number_of_data(DMA1, DMA_CHANNEL5, DATA_SIZE); // Quantity of DMA transfers
        dma_set_peripheral_address(DMA1, DMA_CHANNEL5, (uint32_t)&TIM2_CCR1); // Destination address
        dma_set_memory_address(DMA1, DMA_CHANNEL5, (uint32_t)ws2812_pwm_data); // Source address
        dma_enable_channel(DMA1, DMA_CHANNEL5);
        timer_enable_counter(TIM2); // Start transfer

        ws2812_state = WS2812_STATE_INIT | WS2812_STATE_BUSY;
    }
}

// Interrupt handlers
void dma1_channel4_7_dma2_channel3_5_isr(void) {
    // ISR handler for DMA1 channels 4..7
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL5, DMA_GIF)) {
        ws2812_state &= ~WS2812_STATE_BUSY;
        dma_clear_interrupt_flags(DMA1, DMA_CHANNEL5, DMA_GIF);
    }
}

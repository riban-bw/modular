/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Impelentation of ADC device driver using libopencm3 library
*/

#include "adc.h"
#include "can.h"
#include "panel_types.h"
#include <libopencm3/stm32/rcc.h> // Provides reset & clock
#include <libopencm3/stm32/gpio.h> // Provides GPIO constants
#include <libopencm3/cm3/nvic.h> // Provides ISR
#include <libopencm3/stm32/adc.h> // Provides ADC
#include <libopencm3/stm32/dma.h> // Provides DMA
#include <stdlib.h> // Provides free, malloc, size_t

/* STM32F107 ADC Pins
     0 PA0
     1 PA1
     2 PA2
     3 PA3
     4 PA4
     5 PA5
     6 PA6
     7 PA7
     8 PB0
     9 PB1
    10 PC0
    11 PC1
    12 PC2
    13 PC3
    14 PC4
    15 PC5
*/

#ifndef ADCS
#define ADCS {0, 1, 2, 3, 4, 5, 6, 7}
#endif // ADCS
#define EMA_F 0.6 // Exponential moving average cutoff frequencey factor

uint8_t num_adcs = 0; // Quantity of ADC channels (max 16)
uint16_t adc_raw_values[16]; // Raw ADC values
uint16_t adc_changed_flags = 0; // Bitwise flags indicating change of value of each ADC
struct ADC_T* adcs = NULL; // Pointers to ADC config
extern struct PANEL_ID_T panel_info;

void init_adcs(void) {
    uint32_t ADC_PORT_MAP[] = {GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA, GPIOA,
            GPIOB, GPIOB, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC, GPIOC};
    uint8_t ADC_GPI_MAP[] = {GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5, GPIO6, GPIO7,
            GPIO0, GPIO1, GPIO0, GPIO1, GPIO2, GPIO3, GPIO4, GPIO5};
    uint8_t adcChannels[] = ADCS;
    free(adcs);

    num_adcs = sizeof(adcChannels); // Quantity of ADCs defined for this panel
    adcs = malloc(num_adcs * sizeof(struct ADC_T));
    if (adcs == NULL) {
        //!@todo Handle failure to assign memory
    }
    for (size_t i = 0; i < num_adcs; ++i) {
        uint8_t channel = adcChannels[i];
        adcs[i].port = ADC_PORT_MAP[channel * 2];
        adcs[i].gpi = ADC_GPI_MAP[channel * 2];
        adcs[i].value = 0;
        rcc_periph_clock_enable(adcs[i].port);
        gpio_set_mode(adcs[i].port, GPIO_MODE_INPUT, GPIO_CNF_INPUT_ANALOG, adcs[i].gpi);
    }

    // Configure DMA
    rcc_periph_clock_enable(RCC_DMA1);
    dma_channel_reset(DMA1, DMA_CHANNEL1);
    dma_set_priority(DMA1, DMA_CHANNEL1, DMA_CCR_PL_LOW);
    dma_set_memory_size(DMA1, DMA_CHANNEL1, DMA_CCR_MSIZE_16BIT);
    dma_set_peripheral_size(DMA1, DMA_CHANNEL1, DMA_CCR_PSIZE_16BIT);
    dma_enable_memory_increment_mode(DMA1, DMA_CHANNEL1);
    dma_enable_circular_mode(DMA1, DMA_CHANNEL1);
    dma_set_read_from_peripheral(DMA1, DMA_CHANNEL1);
    dma_set_peripheral_address(DMA1, DMA_CHANNEL1, (uint32_t)&ADC1_DR);
    dma_set_memory_address(DMA1, DMA_CHANNEL1, (uint32_t)adc_raw_values);
    dma_set_number_of_data(DMA1, DMA_CHANNEL1, num_adcs);
    dma_enable_transfer_complete_interrupt(DMA1, DMA_CHANNEL1);
    dma_enable_channel(DMA1, DMA_CHANNEL1);
    nvic_enable_irq(NVIC_DMA1_CHANNEL1_IRQ);

    // Configure ADC
    adc_power_off(ADC1);
    adc_set_sample_time_on_all_channels(ADC1, ADC_SMPR_SMP_239DOT5CYC);
	adc_calibrate(ADC1);
    adc_enable_scan_mode(ADC1);
	adc_set_regular_sequence(ADC1, num_adcs, adcChannels);
    //!@todo Is this not needed for F1? adc_enable_dma_circular_mode(ADC1);
    adc_enable_dma(ADC1);
    adc_power_on(ADC1);
}

void dma1_channel1_isr(void) {
    if (dma_get_interrupt_flag(DMA1, DMA_CHANNEL1, DMA_TCIF)) {
		dma_clear_interrupt_flags(DMA1, DMA_CHANNEL1, DMA_TCIF);
    }
}

void process_adcs(void) {
    static uint16_t value;

    // Check if conversion already in progress
    if ((ADC1_SR & (ADC_SR_STRT | ADC_SR_EOC)) == ADC_SR_STRT)
        return; //!@todo Check that ADC_SR_EOC flag is not reset by DMA

    // Transform raw results through EMA filter
    for (uint8_t i = 0; i < num_adcs; ++i) {
        value = (EMA_F * adc_raw_values[i] + ((1 - EMA_F) * adcs[i].value));
        if (adcs[i].value != value) {
            adcs[i].value = value;
            uint16_t payload[2] = {i};
            payload[1] = value;
            send_msg(CAN_MSG_ADC | panel_info.id, ((uint8_t*)payload + 1), 3);
        }
    }

    // Start next conversion (depend on being called at sample rate)
    adc_clear_flag(ADC1, ADC_SR_EOC | ADC_SR_STRT);
    adc_start_conversion_regular(ADC1);
}

uint16_t get_adc_value(uint8_t adc) {
    if (adc < num_adcs)
        return 0;
    return adcs[adc].value;
}

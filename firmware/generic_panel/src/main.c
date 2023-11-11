/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Provides the core firmware for hardware panels implemented on STM32F103C microcontrollers using libopencm3 framework.
  Panels are generic using the same firmware and are setup at compile time using preprocessor directive PANEL_TYPE
*/

#include <libopencm3/cm3/nvic.h>     // Provides interrupt handlers
#include <libopencm3/cm3/scb.h>      // Provides system reset
#include <libopencm3/cm3/systick.h>  // Provides system clock
#include <libopencm3/stm32/desig.h>  // Provides UID
#include <libopencm3/stm32/gpio.h>   // Provides GPIO constants
#include <libopencm3/stm32/rcc.h>    // Provides reset and clock
#include <stdint.h>                  // Provides fixed size integers
#include <stdlib.h>                  // Provides free, malloc, sizeof

#include "adc.h"          // Provides ADC interface
#include "can.h"          // Provides CAN interface
#include "global.h"       // Provides enums, structures, etc.
#include "panel_types.h"  // Definition of panel types
#include "switches.h"     // Provides switch interface
#include "ws2812.h"       // Provides WS2812 LED interface

// Enumerations
enum RUN_MODE {
    RUN_MODE_DETECT,
    RUN_MODE_RUN,
    RUN_MODE_FIRMWARE
};

// Global variables
static volatile uint64_t ms_uptime = 0;  // Uptime in ms
static volatile uint8_t ms_tick = 0;     // Flag indicating ms timer ticked without being processed/cleared
uint8_t run_mode = 0;                    // Current run mode - see RUN_MODE
uint32_t update_firmware_offset;         // Offset in firmware update
uint32_t firmware_checksum;              //!@todo Use stronger firmware validation, e.g. BLAKE2
volatile uint32_t watchdog_ts;           // Time that last operation occured
struct PANEL_ID_T panel_info;
volatile uint8_t detect_state = DETECT_STATE_INIT;
extern uint16_t num_leds;

// Function forward declarations
void set_run_mode(uint8_t mode);

int main(void) {
    // Use internal clock with 64MHz system clock output
    rcc_clock_setup_in_hsi_out_64mhz();

    // Configure 1ms systick counter
    systick_set_clocksource(STK_CSR_CLKSOURCE_AHB);
    systick_set_reload(rcc_ahb_frequency / 1000 - 1);
    systick_interrupt_enable();
    systick_counter_enable();

    // Configure debug LED (on Bluepill)
    //!@todo Remove this debug code
    rcc_periph_clock_enable(RCC_GPIOC);
    gpio_set_mode(GPIOC, GPIO_MODE_OUTPUT_2_MHZ, GPIO_CNF_OUTPUT_PUSHPULL, GPIO13);
    uint32_t next_sec = 0;  // 1s counter (ms since epoch until next second boundary)

    // Populate panel info
    desig_get_unique_id(panel_info.uid);
    panel_info.version = VERSION;
    panel_info.type = PANEL_TYPE;

    // Initialise LEDs
    {  // Enclose in {} to avoid leds array remaining in memory
        // Get highest number LED
        uint8_t max_led = 0;
        uint8_t leds[] = WSLEDS;
        for (uint8_t i = 0; i < sizeof(leds); ++i)
            if (leds[i] > max_led)
                max_led = leds[i];
        ws2812_init(max_led);
    }
    // Initialise ADCs
    init_adcs();
    // Initialise switches
    init_switches();
    // Initialise CAN bus
    init_can();

    set_run_mode(RUN_MODE_DETECT);

    // Main program loop
    while (1) {
        uint32_t now = ms_uptime;
        if (ms_tick) {
            // 1ms actions
            if (run_mode == RUN_MODE_RUN) {
                process_switches(now);
                process_adcs();
            }
            ws2812_refresh(now);
            ms_tick = 0;
        }
        if (now > next_sec) {
            // 1s actions
            //!@todo Remove this debug code
            next_sec = ms_uptime + 1000;
            gpio_toggle(GPIOC, GPIO13);
        }
        if (run_mode == RUN_MODE_DETECT) {
            switch (detect_state) {
                case DETECT_STATE_INIT:
                    // Start of detection processs - send id[16:95]
                    detect_state = DETECT_STATE_PENDING_1;
                    send_msg(CAN_MSG_REQ_ID_1, (uint8_t *)panel_info.uid, 8);
                    watchdog_ts = ms_uptime;
                    break;
                case DETECT_STATE_RTS_2:
                    // Phase 2 - send id[0:5] + panel type
                    detect_state = DETECT_STATE_PENDING_2;
                    send_msg(CAN_MSG_REQ_ID_2, (uint8_t *)(panel_info.uid + 8), 8);
                    watchdog_ts = ms_uptime;
                    break;
                case DETECT_STATE_RX_ID:
                    set_run_mode(RUN_MODE_RUN);
                    break;
                default:
                    break;
            }
            if (now - watchdog_ts > MSG_TIMEOUT)
                detect_state = DETECT_STATE_INIT;
        }
    }
    return 0;  // We never get here but compiler warns if main does not return int
}

void sys_tick_handler(void) {
    ms_uptime++;  // Increment 1ms monotonic clock
    ms_tick = 1;  // Set 1ms flag
}

void can_rx1_isr() {
    static uint32_t id;
    static bool ext_msg;
    static bool req_tx;
    static uint8_t filter_num;
    static uint8_t data_len;
    static uint8_t data[8];

    can_receive(CAN1, 0, true, &id, &ext_msg, &req_tx, &filter_num, &data_len, data, NULL);

    switch (filter_num) {
        case CAN_FILTER_NUM_DETECT:
            switch (detect_state) {
                case DETECT_STATE_PENDING_1:
                    // Sent REQ_ID_1, pending ACK_ID_1
                    if (id == CAN_MSG_ACK_ID_1 && data_len == 8) {
                        // Brain has acknowledged stage 1 so check if we are a contender for stage 2
                        //!@todo Casting in this way is not recommended - may need to check each byte individually
                        uint64_t *a = (uint64_t *)data;
                        uint64_t *b = (uint64_t *)panel_info.uid;
                        if (*a == *b) {
                            // Matches so progress to stage 2
                            detect_state = DETECT_STATE_RTS_2;
                            //!@todo Should we send next message from here to avoid wait until detect() is called in main loop?
                            watchdog_ts = ms_uptime;
                        } else {
                            // Not a winner so restart detection
                            set_run_mode(RUN_MODE_DETECT);
                            //!@todo Maybe restart will add too much bus contention - may be better to just wait for timeout
                        }
                    }
                    break;
                case DETECT_STATE_PENDING_2:
                    // Sent REQ_ID_2, pending SET_ID
                    if (id == CAN_MSG_SET_ID) {
                        // Brain has assigned a panel id so check if it was for us
                        //!@todo Casting in this way is not recommended - may need to check each byte individually
                        uint32_t *a = (uint32_t *)data;
                        if (data_len > 3 && *a == panel_info.uid[2]) {
                            // Matches so set panel id
                            panel_info.id = data[4];
                            detect_state = DETECT_STATE_RX_ID;
                        } else {
                            // Not a winner so restart detection
                            set_run_mode(RUN_MODE_DETECT);
                        }
                    }
                    break;
            }
            break;
        case CAN_FILTER_NUM_FIRMWARE:
            switch (id) {
                case CAN_MSG_FU_BLOCK:
                    //!@todo Casting in this way is not recommended - may need to check each byte individually
                    uint32_t *fu_data = (uint32_t *)data;
                    if (data_len > 3) {
                        //!@todo flash(update_firmware_offset++, *fu_data);
                        firmware_checksum += *fu_data;
                    }
                    if (data_len > 7) {
                        //!@todo flash(update_firmware_offset++, *(fu_data + 1));
                        firmware_checksum += *fu_data;
                    }
                    watchdog_ts = ms_uptime;
                    break;
                case CAN_MSG_FU_END:
                    // Simple checksum - should use stronger validation, e.g. BLAKE2
                    uint64_t *checksum = (uint64_t *)data;
                    if (*checksum == firmware_checksum) {
                        // Set partion bootable and reboot
                    }
                    scb_reset_system();
                    break;
            }
            break;
        case CAN_FILTER_NUM_RUN:
            // Run mode
            switch (id & CAN_MSG_LED) {
                case CAN_MSG_LED:
                    if (data_len > 7)
                        ws2812_set_mode(data[0], data[7]);
                    if (data_len > 6)
                        ws2812_set_colour_2(data[0], data[4], data[5], data[6]);
                    if (data_len > 3)
                        ws2812_set_colour_1(data[0], data[1], data[2], data[3]);
                    if (data_len == 2)
                        ws2812_set_mode(data[0], data[1]);
            }
            break;
        case CAN_FILTER_NUM_BROADCAST:
            switch (id) {
                case CAN_MSG_FU_START:
                    set_run_mode(RUN_MODE_FIRMWARE);
                    break;
            }
            break;
        default:
            // Not a known or expected CAN message id
            break;
    }
}

void set_run_mode(uint8_t mode) {
    // Disable all CAN input acceptance filters
    for (uint32_t i = 0; i < 14; ++i)
        can_filter_id_mask_32bit_init(i, 0, 0, 0, false);

    switch (mode) {
        case RUN_MODE_RUN:
            can_filter_id_mask_32bit_init(
                CAN_FILTER_NUM_RUN,
                CAN_FILTER_ID_RUN | panel_info.id,
                CAN_FILTER_MASK_RUN,
                0,
                true);
            can_filter_id_mask_32bit_init(
                CAN_FILTER_NUM_BROADCAST,
                CAN_FILTER_ID_BROADCAST,
                CAN_FILTER_MASK_BROADCAST,
                0,
                true);
            // Extinguise all LEDs - will be illuminated by config messages
            for (uint8_t led = 0; led < num_leds; ++led)
                ws2812_set_mode(led, WS2812_MODE_OFF);
            // Send panel version to inform brain of presence, e.g. allow for firmware update if required
            send_msg(CAN_MSG_VERSION, (uint8_t *)&panel_info.id, 8);
            break;
        case RUN_MODE_DETECT:
            can_filter_id_mask_32bit_init(
                CAN_FILTER_NUM_DETECT,
                CAN_FILTER_ID_DETECT,
                CAN_FILTER_MASK_DETECT,
                0,
                true);
            detect_state = DETECT_STATE_INIT;
            // Flash all LEDs red/green to indicate detect mode
            for (uint8_t led = 0; led < num_leds; ++led) {
                ws2812_set_colour_1(led, 200, 0, 0);
                ws2812_set_colour_1(led, 0, 200, 0);
                ws2812_set_mode(led, WS2812_MODE_FAST_FLASH);
            }
            break;
        case RUN_MODE_FIRMWARE:
            update_firmware_offset = 0;
            can_filter_id_mask_32bit_init(
                CAN_FILTER_NUM_FIRMWARE,
                CAN_FILTER_ID_FIRMWARE,
                CAN_FILTER_MASK_FIRMWARE,
                0,
                true);
            // Flash all LEDs ble to indicate firmware upload mode
            for (uint8_t led = 0; led < num_leds; ++led) {
                ws2812_set_colour_1(led, 0, 0, 200);
                ws2812_set_colour_1(led, 0, 0, 50);
                ws2812_set_mode(led, WS2812_MODE_FAST_FLASH);
            }
            break;
    }
    watchdog_ts = ms_uptime;
    run_mode = mode;
}

/* riban modular

  Copyright 2023 riban ltd <info@riban.co.uk>

  This file is part of riban modular.
  riban modular is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
  riban modular is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.
  You should have received a copy of the GNU General Public License along with riban modular. If not, see <https://www.gnu.org/licenses/>.

  Impelentation of CAN device driver using libopencm3 library
  Based on example code by Piotr Esden-Tempski <piotr@esden.net>
*/

#include "can.h"
#include <stddef.h>
#include <libopencm3/cm3/nvic.h>
#include <libopencm3/cm3/systick.h>
#include <libopencm3/stm32/can.h>
#include <libopencm3/stm32/gpio.h>
#include <libopencm3/stm32/flash.h>
#include <libopencm3/stm32/rcc.h>

int init_can(void) {
	/* Enable peripheral clocks. */
	rcc_periph_clock_enable(RCC_AFIO);
	rcc_periph_clock_enable(RCC_GPIOB);
	rcc_periph_clock_enable(RCC_CAN1);

	AFIO_MAPR |= AFIO_MAPR_CAN1_REMAP_PORTB;

	/* Configure CAN pin: RX (input pull-up). */
	gpio_set_mode(GPIO_BANK_CAN1_PB_RX, GPIO_MODE_INPUT,
		      GPIO_CNF_INPUT_PULL_UPDOWN, GPIO_CAN1_PB_RX);
	gpio_set(GPIO_BANK_CAN1_PB_RX, GPIO_CAN1_PB_RX);

	/* Configure CAN pin: TX. */
	gpio_set_mode(GPIO_BANK_CAN1_PB_TX, GPIO_MODE_OUTPUT_50_MHZ,
		      GPIO_CNF_OUTPUT_ALTFN_PUSHPULL, GPIO_CAN1_PB_TX);

	/* NVIC setup. */
	nvic_enable_irq(NVIC_USB_LP_CAN_RX0_IRQ);
	nvic_set_priority(NVIC_USB_LP_CAN_RX0_IRQ, 1);

	/* Reset CAN. */
	can_reset(CAN1);

	/* CAN cell init.
	 * Setting the bitrate to 1MBit. APB1 = 32MHz, 
	 * prescaler = 2 -> 16MHz time quanta frequency.
	 * 1tq sync + 9tq bit segment1 (TS1) + 6tq bit segment2 (TS2) = 
	 * 16time quanto per bit period, therefor 16MHz/16 = 1MHz
	 */
	if (can_init(CAN1,
		     false,           // TTCM: Disable time triggered comm mode
		     true,            // ABOM: Automatic bus-off
		     true,            // AWUM: Enable wake on CAN Rx
		     false,           // NART: Do not disable automatic retransmission
		     false,           // RFLM: Receive FIFO not locked - will overwrite old data even if overrun occured
		     false,           // TXFP: Transmit FIFO not locked - will overwrite old data even if overrun occured
		     CAN_BTR_SJW_1TQ, // SJW: Resynchronization time quanta jump width
		     CAN_BTR_TS1_9TQ, // TS1: Time segment 1 time quanta width
		     CAN_BTR_TS2_6TQ, // TS2: Time segment 2 time quanta width
		     2,				  // BRP: Baud rate prescaler
		     false,			  // Disable loopback
		     false))          // Disable silent mode
        return 1;

	/* Enable CAN RX interrupt. */
	can_enable_irq(CAN1, CAN_IER_FMPIE0);
    return 0;
}

int send_msg(uint32_t id, uint8_t* msg, uint8_t len) {
	return can_transmit(CAN1, id, 0, 0, len, msg);
}
/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

                                      ---

    A special exception to the GPL can be applied should you wish to distribute
    a combined work that includes ChibiOS/RT, without being obliged to provide
    the source code for any proprietary components. See the file exception.txt
    for full details of how and when the exception can be applied.
*/

#ifndef _BOARD_H_
#define _BOARD_H_

/*
 * Setup for the CQ STARM board.
 */
#undef	SET_USB_CONDITION		/* No functionality to disconnect USB */
#define	SET_LED_CONDITION(on) on	/* To emit light, call palSetPad */
#define GPIO_LED	GPIOC_LED
#define IOPORT_LED	GPIOC
#define FLASH_PAGE_SIZE 1024

/*
 * Board identifier.
 */
#define BOARD_CQ_STARM
#define BOARD_NAME "CQ STARM"

/*
 * Board frequencies.
 */
#define STM32_LSECLK            32768
#define STM32_HSECLK            8000000

/*
 * MCU type, this macro is used by both the ST library and the ChibiOS/RT
 * native STM32 HAL.
 */
#define STM32F10X_MD

/*
 * IO pins assignments.
 */
#define GPIOC_LED               6

#if 0
#define GPIOA_BUTTON            0
#define GPIOA_SPI1NSS           4

#define GPIOB_SPI2NSS           12
#define GPIOC_MMCWP             6
#define GPIOC_MMCCP             7
#define GPIOC_CANCNTL           10
#define GPIOC_DISC              11
#endif

/*
 * I/O ports initial setup, this configuration is established soon after reset
 * in the initialization code.
 *
 * The digits have the following meaning:
 *   0 - Analog input.
 *   1 - Push Pull output 10MHz.
 *   2 - Push Pull output 2MHz.
 *   3 - Push Pull output 50MHz.
 *   4 - Digital input.
 *   5 - Open Drain output 10MHz.
 *   6 - Open Drain output 2MHz.
 *   7 - Open Drain output 50MHz.
 *   8 - Digital input with PullUp or PullDown resistor depending on ODR.
 *   9 - Alternate Push Pull output 10MHz.
 *   A - Alternate Push Pull output 2MHz.
 *   B - Alternate Push Pull output 50MHz.
 *   C - Reserved.
 *   D - Alternate Open Drain output 10MHz.
 *   E - Alternate Open Drain output 2MHz.
 *   F - Alternate Open Drain output 50MHz.
 * Please refer to the STM32 Reference Manual for details.
 */

/*
 * Port A setup.
 * Everything input with pull-up except:
 * PA4  - Normal input      (ADC_IN4 : VoutX of LIS344ALH).
 * PA5  - Alternate output  (MMC SPI1 SCK).
 * PA6  - Normal input      (MMC SPI1 MISO).
 * PA7  - Alternate output  (MMC SPI1 MOSI).
 * PA11 - (USBDM)
 * PA12 - (USBDP)
 */
#define VAL_GPIOACRL            0xB4B48888      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x88888888      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFFFF

/*
 * Port B setup.
 * Everything input with pull-up except:
 * PB13 - Alternate output  (MMC SPI2 SCK).
 * PB14 - Normal input      (MMC SPI2 MISO).
 * PB15 - Alternate output  (MMC SPI2 MOSI).
 */
#define VAL_GPIOBCRL            0x88888888      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0xB4B88888      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF

/*
 * Port C setup.
 * Everything input with pull-up except:
 * PC4  - Normal input      (ADC_IN14 : VoutY of LIS344ALH).
 * PC5  - Normal input      (ADC_IN15 : VoutZ of LIS344ALH).
 * PC6  - Push Pull output (LED).
 * (PC9  - SDCard CD)
 * (PC12 - SDCard CS)
 * PC14 - Normal input (XTAL).
 * PC15 - Normal input (XTAL).
 */
#define VAL_GPIOCCRL            0x83448888      /*  PC7...PC0 */
#define VAL_GPIOCCRH            0x44888888      /* PC15...PC8 */
#define VAL_GPIOCODR            0xFFFFFFFF

/*
 * Port D setup.
 * Everything input with pull-up except:
 * (PD9 - USB_DC)
 */
#define VAL_GPIODCRL            0x88888888      /*  PD7...PD0 */
#define VAL_GPIODCRH            0x88888888      /* PD15...PD8 */
#define VAL_GPIODODR            0xFFFFFFFF

/*
 * Port E setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOECRL            0x88888888      /*  PE7...PE0 */
#define VAL_GPIOECRH            0x88888888      /* PE15...PE8 */
#define VAL_GPIOEODR            0xFFFFFFFF

#endif /* _BOARD_H_ */

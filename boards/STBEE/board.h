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

#include "config.h"
/*
 * Setup for the STBee board.
 */
#define	SET_USB_CONDITION(en) (!en)	/* To connect USB, call palClearPad */
#define	SET_LED_CONDITION(on) (!on)	/* To emit light, call palClearPad */
#define GPIO_USB	GPIOD_USB_ENABLE
#define IOPORT_USB	GPIOD
#define GPIO_LED	GPIOD_LED1
#define IOPORT_LED	GPIOD

/* NeuG settings for ADC2.  */
#define NEUG_ADC_SETTING2_SMPR1 ADC_SMPR1_SMP_AN10(ADC_SAMPLE_1P5) \
                              | ADC_SMPR1_SMP_AN11(ADC_SAMPLE_1P5)
#define NEUG_ADC_SETTING2_SMPR2 0
#define NEUG_ADC_SETTING2_SQR3  ADC_SQR3_SQ1_N(ADC_CHANNEL_IN10)   \
                              | ADC_SQR3_SQ2_N(ADC_CHANNEL_IN11)
#define NEUG_ADC_SETTING2_NUM_CHANNELS 2

/*
 * Board identifier.
 */
#define BOARD_STBEE
#define BOARD_NAME "STBee"

#if defined(PINPAD_CIR_SUPPORT) || defined(PINPAD_DIAL_SUPPORT)
#define HAVE_7SEGLED	1
/*
 * Timer assignment for CIR
 */
#define TIMx	TIM3
#endif

/*
 * Board frequencies.
 */
#define STM32_LSECLK            32768
#define STM32_HSECLK            12000000

/*
 * MCU type, this macro is used by both the ST library and the ChibiOS/RT
 * native STM32 HAL.
 */
#define STM32F10X_HD

/*
 * IO pins assignments.
 */
#define GPIOD_LED1             4
#define GPIOD_USB_ENABLE       3
#define GPIOA_USER             0

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

#if defined(PINPAD_CIR_SUPPORT) || defined(PINPAD_DIAL_SUPPORT)
/*
 * Port A setup.
 * PA6  - (TIM3_CH1) input with pull-up
 * PA7  - (TIM3_CH2) input with pull-down
 * PA11 - input with pull-up (USBDM)
 * PA12 - input with pull-up (USBDP)
 * Everything input with pull-up except:
 * PA0  - Normal input.
 */
#define VAL_GPIOACRL            0x88888884      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x88888888      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFF7F

/* Port B setup. */
#define GPIOB_CIR		0
#define GPIOB_BUTTON            2
#define GPIOB_ROT_A             6
#define GPIOB_ROT_B             7

#define GPIOB_7SEG_DP           15
#define GPIOB_7SEG_A            14
#define GPIOB_7SEG_B            13
#define GPIOB_7SEG_C            12
#define GPIOB_7SEG_D            11
#define GPIOB_7SEG_E            10
#define GPIOB_7SEG_F            9
#define GPIOB_7SEG_G            8

#define VAL_GPIOBCRL            0x88888888      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0x66666666      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF
#else
/*
 * Port A setup.
 * PA11 - input with pull-up (USBDM)
 * PA12 - input with pull-up (USBDP)
 * Everything input with pull-up except:
 * PA0 - Normal input.
 */
#define VAL_GPIOACRL            0x88888884      /*  PA7...PA0 */
#define VAL_GPIOACRH            0x88888888      /* PA15...PA8 */
#define VAL_GPIOAODR            0xFFFFFFFF

/* Port B setup. */
/* Everything input with pull-up */
#define VAL_GPIOBCRL            0x88888888      /*  PB7...PB0 */
#define VAL_GPIOBCRH            0x88888888      /* PB15...PB8 */
#define VAL_GPIOBODR            0xFFFFFFFF
#endif

/*
 * Port C setup.
 * PC0  - Digital input with PullUp.  AN10 for NeuG
 * PC1  - Digital input with PullUp.  AN11 for NeuG
 * Everything input with pull-up except:
 */
#define VAL_GPIOCCRL            0x88888888      /*  PC7...PC0 */
#define VAL_GPIOCCRH            0x88888888      /* PC15...PC8 */
#define VAL_GPIOCODR            0xFFFFFFFF

/*
 * Port D setup.
 * Everything input with pull-up except:
 * PD3  - Push pull output  (USB_DISC 1:USB-DISABLE 0:USB-ENABLE) 2MHz
 * PD4  - Open Drain output 2MHz (LED1).
 */
#define VAL_GPIODCRL            0x88862888      /*  PD7...PD0 */
#define VAL_GPIODCRH            0x88888888      /* PD15...PD8 */
#define VAL_GPIODODR            0xFFFFFFFF

/*
 * Port E setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOECRL            0x88888888      /*  PE7...PE0 */
#define VAL_GPIOECRH            0x88888888      /* PE15...PE8 */
#define VAL_GPIOEODR            0xFFFFFFFF

/*
 * Port F setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOFCRL            0x88888888      /*  PF7...PF0 */
#define VAL_GPIOFCRH            0x88888888      /* PF15...PF8 */
#define VAL_GPIOFODR            0xFFFFFFFF

/*
 * Port G setup.
 * Everything input with pull-up except:
 */
#define VAL_GPIOGCRL            0x88888888      /*  PG7...PG0 */
#define VAL_GPIOGCRH            0x88888888      /* PG15...PG8 */
#define VAL_GPIOGODR            0xFFFFFFFF

#if !defined(_FROM_ASM_)
#ifdef __cplusplus
extern "C" {
#endif
  void boardInit(void);
#ifdef __cplusplus
}
#endif
#endif /* _FROM_ASM_ */

#endif /* _BOARD_H_ */

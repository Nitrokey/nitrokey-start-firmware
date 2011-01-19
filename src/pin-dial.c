/*
 * pin-dial.c -- PIN input device support (rotary encoder + push switch)
 *
 * Copyright (C) 2011 Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Gnuk, a GnuPG USB Token implementation.
 *
 * Gnuk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnuk is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"
#include "ch.h"
#include "hal.h"
#include "board.h"
#include "gnuk.h"

uint8_t pin_input_buffer[MAX_PIN_CHARS];
uint8_t pin_input_len;

#define LED_DISP_BLINK_INTERVAL0	MS2ST(150)
#define LED_DISP_BLINK_INTERVAL1	MS2ST(100)

/*
 * PB6 TIM4_CH1
 * PB7 TIM4_CH2
 *
 * TIM4_SMCR
 *  SMS = 001
 * TIM4_CCER
 *  CC1P = 0
 *  CC2P = 0
 * TIM4_CCMR1
 *  CC1S= 01
 *  CC2S= 01
 * TIM4_CR1
 *  CEN= 1
 */

#define OFF   '\x00'
#define ENTER '\x0a'
static struct led_pattern { uint8_t c, v; } led_pattern[] = 
{
		     /* char : dp a b c d e f g */
  { ENTER, 0xf8 },   /* |-   :  1 1 1 1 1 0 0 0  (enter) */
  { ' ', 0xff },     /* SPC  :  1 1 1 1 1 1 1 1 */
  { '0', 0x81 },     /* 0    :  1 0 0 0 0 0 0 1 */
  { '1', 0xcf },     /* 1    :  1 1 0 0 1 1 1 1 */
  { '2', 0x92 },     /* 2    :  1 0 0 1 0 0 1 0 */
  { '3', 0x86 },     /* 3    :  1 0 0 0 0 1 1 0 */
  { '4', 0xcc },     /* 4    :  1 1 0 0 1 1 0 0 */
  { '5', 0xa4 },     /* 5    :  1 0 1 0 0 1 0 0 */
  { '6', 0xa0 },     /* 6    :  1 0 1 0 0 0 0 0 */
  { '7', 0x8d },     /* 7    :  1 0 0 0 1 1 0 1 */
  { '8', 0x80 },     /* 8    :  1 0 0 0 0 0 0 0 */
  { '9', 0x84 },     /* 9    :  1 0 0 0 0 1 0 0 */
  { 'A', 0x88 },     /* A    :  1 0 0 0 1 0 0 0 */
  { 'B', 0xe0 },     /* b    :  1 1 1 0 0 0 0 0 */
  { 'C', 0xb1 },     /* C    :  1 0 1 1 0 0 0 1 */
  { 'D', 0xc2 },     /* d    :  1 1 0 0 0 0 1 0 */
  { 'E', 0xb0 },     /* E    :  1 0 1 1 0 0 0 0 */
  { 'F', 0xb8 },     /* F    :  1 0 1 1 1 0 0 0 */

  { 'G', 0xa1 },     /* G    :  1 0 1 0 0 0 0 1 */
  { '\xff', 0xce },  /* -|   :  1 1 0 0 1 1 1 0  (backspace) */
};

#define LENGTH_LED_PATTERN (int)(sizeof led_pattern / sizeof (struct led_pattern))

static void
led_disp (uint8_t c)
{
  uint16_t v = palReadPort (IOPORT2) | 0x00ff;

  if (c == OFF)
    v |= 0xff00;
  else
    {
      int i;

      v &= 0x80ff;
      for (i = 0; i < LENGTH_LED_PATTERN; i++)
	if (led_pattern[i].c == c)
	  {
	    v |= ((led_pattern[i].v & 0x7f) << 8); /* Don't touch DP.  */
	    break;
	  }
	else if (led_pattern[i].c > c)
	  {
	    v |= 0x7f00;		/* default: SPC */
	    break;
	  }
    }

  palWritePort (IOPORT2, v);
}

static void
blink_dp (void)
{
  uint16_t v = palReadPort (IOPORT2) | 0x00ff;

  v ^= 0x8000;
  palWritePort (IOPORT2, v);
}

static Thread *pin_thread;
#define EV_SW_PUSH (eventmask_t)1

void
dial_sw_interrupt (void)
{
  dial_sw_disable ();
  chEvtSignalI (pin_thread, EV_SW_PUSH);
  palClearPad (IOPORT1, GPIOA_LED2);
}


msg_t
pin_main (void *arg)
{
  int msg_code = (int)arg;
  uint16_t count, count_prev;
  uint8_t input_mode;
  uint8_t sw_push_count;
  uint8_t sw_event;

  (void)msg_code;

  pin_thread = chThdSelf ();
  led_disp (' ');

  TIM4->CNT = 0;
  TIM4->CR1 |= TIM_CR1_CEN;
  input_mode = 0;
  count = count_prev = 0;
  pin_input_len = 0;
  sw_push_count = 0;
  sw_event = 0;

  while (!chThdShouldTerminate ())
    {
      eventmask_t m;

      blink_dp ();
      dial_sw_enable ();
      m = chEvtWaitOneTimeout (ALL_EVENTS, LED_DISP_BLINK_INTERVAL0);

      if (m == EV_SW_PUSH || sw_push_count)
	{
	  if (palReadPad (IOPORT2, GPIOB_BUTTON) == 0)
	    sw_push_count++;
	  else			/* ignore this bounce */
	    {
	      palSetPad (IOPORT1, GPIOA_LED2);
	      sw_push_count = 0;
	    }
	}

      if (sw_push_count >= 2)
	sw_event = 1;

      count = (TIM4->CNT) / 2;

      if (input_mode == 1)
	{
	  if (count_prev != count)
	    input_mode = 0;
	  else
	    {
	      led_disp (ENTER);
	      if (sw_event)
		{
		  palSetPad (IOPORT1, GPIOA_LED2);
		  break;
		}
	    }
	}

      if (input_mode == 0)
	{
	  uint8_t c;

	  if (count < 10)
	    c = count + '0';
	  else
	    c = (count - 10) + 'A';

	  led_disp (c);

	  if (sw_event)
	    {
	      pin_input_buffer[pin_input_len] = c;
	      if (pin_input_len < MAX_PIN_CHARS - 1)
		pin_input_len++;
	      input_mode = 1;
	      sw_event = sw_push_count = 0;
	      palSetPad (IOPORT1, GPIOA_LED2);
	    }
	}

      chThdSleep (LED_DISP_BLINK_INTERVAL1);
      count_prev = count;
    }

  led_disp (OFF);
  TIM4->CR1 &= ~TIM_CR1_CEN;
  dial_sw_disable ();
  return 0;
}

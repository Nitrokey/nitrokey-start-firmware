/*
 * pin-cir.c -- PIN input device support (Consumer Infra-Red)
 *
 * Copyright (C) 2010, 2011 Free Software Initiative of Japan
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

#if 0
#define DEBUG_CIR 1
#endif

uint8_t pin_input_buffer[MAX_PIN_CHARS];
uint8_t pin_input_len;

/*
 * Philips RC-5 Protocol: 14-bit

 * Philips RC-6 Protocol: (START + 1 + MODE + TR + 32-bit / 16-bit)
 *                                     3-bit     (mode 6 / mode 0)
 *
 * NEC Protocol: (START + 32-bit + STOP) + (START + STOP) x N
 *      32-bit: 8-bit command inverted + 8-bit command + 16-bit address
 * 	Example: Controller of Toshiba REGZA
 * 	Example: Controller of Ceiling Light by NEC
 *
 * Sony Protocol: (START + 12-bit) or (START + 15-bit)
 *      12-bit: 5-bit address + 7-bit command
 * 	Example: Controller of Sony BRAVIA
 *
 * Unknown Protocol 1: (START + 48-bit + STOP) x N
 *      Example: Controller of Sharp AQUOS
 *      Example: Controller of Ceiling Light by Mitsubishi
 *
 * Unsupported:
 *
 * Unknown Protocol 2: (START + 112-bit + STOP)
 *      Example: Controller of Mitsubishi air conditioner
 *
 * Unknown Protocol 3: (START + 128-bit + STOP)
 *      Example: Controller of Fujitsu air conditioner
 *
 * Unknown Protocol 4: (START + 152-bit + STOP)
 *      Example: Controller of Sanyo air conditioner
 */

/*
 * PB0 / TIM3_CH3
 *
 * 72MHz
 *
 * Prescaler = 72
 *
 * 1us
 *
 *
 * TIM3_CR1
 *  CKD  = 10 (sampling x4)
 *  ARPE = 0  (not buffered)
 *  CMS  = 00 (up counter)
 *  DIR  = 0  (up counter)
 *  OPM  = 0  (up counter)
 *  URS  = 1  (update request source: overflow only)
 *  UDIS = 0  (UEV (update event) enabled)
 *  CEN  = 1  (counter enable)
 *
 * TIM3_CR2
 *  TI1S = 1 (TI1 is XOR of ch1, ch2, ch3)
 *  MMS  = 000 (TRGO at Reset)
 *  CCDS = 0 (DMA on capture)
 *  RSVD = 000
 *
 * TIM3_SMCR
 *  ETP  = 0
 *  ECE  = 0
 *  ETPS = 00
 *  ETF  = 0000
 *  MSM  = 0
 *  TS   = 101 (TI1FP1 selected)
 *  RSVD = 0
 *  SMS  = 100 (Reset-mode)
 *
 * TIM3_DIER
 *
 * TIM3_SR
 *
 * TIM3_EGR
 *
 * TIM3_CCMR1
 *  CC1S = 01 (TI1 selected)
 *  CC2S = 10 (TI1 selected)
 *
 * TIM3_CCMR2
 *
 * TIM3_CCER
 *  CC2P = 1 (polarity = falling edge: TI1FP1)
 *  CC2E = 1
 *  CC1P = 0 (polarity = rising edge: TI1FP1)
 *  CC1E = 1
 *
 * TIM3_CNT
 * TIM3_PSC = 71
 * TIM3_ARR = 18000
 *
 * TIM3_CCR1  period
 * TIM3_CCR2  duty
 *
 * TIM3_DCR
 * TIM3_DMAR
 */

#define PINDISP_TIMEOUT_INTERVAL0	MS2ST(25)
#define PINDISP_TIMEOUT_INTERVAL1	MS2ST(300)

static void
pindisp (uint8_t c)
{
  switch (c)
    {
    case 'G':
      palWritePort (IOPORT2, 0xa1ff);
      break;
    case 'P':
      palWritePort (IOPORT2, 0x98ff);
      break;
    case '.':
      palWritePort (IOPORT2, 0x7fff);
      break;
    default:
      palWritePort (IOPORT2, 0xffff);
    }
}

#if defined(DEBUG_CIR)
static uint16_t intr_ext;
static uint16_t intr_trg;
static uint16_t intr_ovf;

#define MAX_CIRINPUT_BIT 512
static uint16_t cirinput[MAX_CIRINPUT_BIT];
static uint16_t *cirinput_p;
#endif

static uint32_t cir_data;
static uint8_t cir_proto;
#define CIR_PROTO_RC5   1
#define CIR_PROTO_RC6   2
#define CIR_PROTO_OTHER 3
#define CIR_PROTO_SONY  4

#define CIR_KEY_RC6_ENTER     0x5c
#define CIR_KEY_RC6_BACKSPACE 0xa4
#define CIR_KEY_SONY_ENTER      0x16 /* 'kettei' */
#define CIR_KEY_SONY_BACKSPACE  0xa3 /* 'modoru' */

/* CIR_DATA_ZERO: Used for zero-bit handling of RC-5/RC-6 */
static uint8_t cir_data_zero;

static uint8_t cir_seq;

static systime_t cir_input_last;
#define CIR_PERIOD_INHIBIT_CHATTER 200 /* mili second */

/*
 * RC-5 protocol doesn't have start bit, while any other protocols
 * have the one.
 */
#define CIR_BIT_START_RC5_DETECT 2000

#define CIR_BIT_START_RC5_LENGTH (889 + 889/2)
#define CIR_BIT_PERIOD_RC6 444
#define CIR_BIT_PERIOD 1500
#define CIR_BIT_SIRC_PERIOD_ON 1000

static void
cir_init (void)
{
  cir_data = 0;
  cir_seq = 0;
  /* Don't touch cir_proto here */
  cir_ext_enable ();
}

static Thread *pin_thread;

static int
cir_key_is_backspace (void)
{
  return (cir_proto == CIR_PROTO_RC6 && cir_data == CIR_KEY_RC6_BACKSPACE);
}

static int
cir_key_is_enter (void)
{
  return (cir_proto == CIR_PROTO_RC6 && cir_data == CIR_KEY_RC6_ENTER)
    || (cir_proto == CIR_PROTO_SONY && cir_data == CIR_KEY_SONY_ENTER);
}

msg_t
pin_main (void *arg)
{
  uint8_t s = 0;

  (void)arg;
  pin_thread = chThdSelf ();

#if defined(DEBUG_CIR)
  cirinput_p = cirinput;
#endif

  pin_input_len = 0;
  chEvtClear (ALL_EVENTS);
  cir_init ();

  while (!chThdShouldTerminate ())
    {
      eventmask_t m;

      m = chEvtWaitOneTimeout (ALL_EVENTS, PINDISP_TIMEOUT_INTERVAL1);

      if (m)
	{
#if defined(DEBUG_CIR)
	  uint16_t *p;

	  DEBUG_INFO ("****\r\n");
	  DEBUG_SHORT (intr_ext);
	  DEBUG_SHORT (intr_trg);
	  DEBUG_SHORT (intr_ovf);
	  DEBUG_INFO ("----\r\n");
	  for (p = cirinput; p < cirinput_p; p++)
	    DEBUG_SHORT (*p);
	  DEBUG_INFO ("====\r\n");

	  cirinput_p = cirinput;
#endif
	  DEBUG_INFO ("**** CIR data:");
	  DEBUG_WORD (cir_data);
	  DEBUG_BYTE (cir_seq);

	  if (cir_key_is_backspace ())
	    {
	      if (pin_input_len > 0)
		pin_input_len--;
	    }
	  else if (cir_key_is_enter ())
	    {
	      pindisp (' ');
	      chThdExit (0);
	    }
	  else if (pin_input_len < MAX_PIN_CHARS)
	    pin_input_buffer[pin_input_len++] = (uint8_t)cir_data;

	  cir_init ();
	}

      switch (s++)
	{
	case 0:
	  pindisp ('G');
	  break;
	case 1:
	  pindisp ('P');
	  break;
	case 2:
	  pindisp ('G');
	  break;
	case 3:
	  pindisp ('.');
	  break;
	default:
	  pindisp (' ');
	  s = 0;
	  break;
	}

      chThdSleep (PINDISP_TIMEOUT_INTERVAL0);
    }

  return 0;
}

void
cir_ext_interrupt (void)
{
  cir_ext_disable ();

#if defined(DEBUG_CIR)
  intr_ext++;
  if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
    {
      *cirinput_p++ = 0x0000;
      *cirinput_p++ = (uint16_t)chTimeNow ();
    }
#endif

  TIM3->EGR = TIM_EGR_UG;	/* Generate UEV to load PSC and ARR */
  /* Enable Timer */
  TIM3->SR &= ~(TIM_SR_UIF
		| TIM_SR_CC1IF | TIM_SR_CC2IF
		| TIM_SR_TIF
		| TIM_SR_CC1OF | TIM_SR_CC2OF);
  TIM3->DIER = TIM_DIER_UIE /*overflow*/ | TIM_DIER_TIE /*trigger*/;
  TIM3->CR1 |= TIM_CR1_CEN;
}

#define CIR_PERIOD_ON_RC5_OR_RC6 (((cir_proto == CIR_PROTO_RC5) ? 2 : 1) \
				  * CIR_BIT_PERIOD_RC6 * 3 / 2)

void
cir_timer_interrupt (void)
{
  uint16_t period, on, off;

  period = TIM3->CCR1;
  on = TIM3->CCR2;
  off = period - on;

  if ((TIM3->SR & TIM_SR_TIF))
    {
      if (cir_seq == 0)
	{
	  if (on >= CIR_BIT_START_RC5_DETECT)
	    {
	      cir_proto = CIR_PROTO_OTHER;
	      cir_data_zero = 0;
	    }
	  else
	    {
	      cir_proto = CIR_PROTO_RC5;
	      cir_data = 1;
	      if (on >= CIR_BIT_START_RC5_LENGTH)
		{
		  cir_data <<= 1;
		  cir_seq++;
		  if (off >= CIR_BIT_START_RC5_LENGTH)
		    cir_data_zero = 0;
		  else
		    cir_data_zero = 1;
		}
	      else
		cir_data_zero = 0;
	    }
	}
      else if (cir_proto == CIR_PROTO_OTHER)
	{
	  cir_data >>= 1;
	  cir_data |= (period >= CIR_BIT_PERIOD) ? 0x80000000 : 0;

	  /* Detection of RC-6 protocol */
	  if (cir_data_zero && on > CIR_BIT_PERIOD_RC6*3/2)
	    /* TR-bit 0 is detected */
	    {
	      cir_proto = CIR_PROTO_RC6;
	      cir_seq = 0;
	      cir_data = 0;	/* ignore MODE bits */
	      if (on > CIR_BIT_PERIOD_RC6*5/2)
		{
		  cir_data = 1;
		  if (off > CIR_BIT_PERIOD_RC6*3/2)
		    cir_data_zero = 1;
		  else
		    cir_data_zero = 0;
		}
	      else		/* Off must be short */
		{
		  cir_data_zero = 1;
		}
	    }
	  else if ((!cir_data_zero
	       && on > CIR_BIT_PERIOD_RC6*3/2 && off > CIR_BIT_PERIOD_RC6*3/2))
	    /* TR-bit 1 is detected */
	    {
	      cir_proto = CIR_PROTO_RC6;
	      cir_seq = 0;
	      cir_data = 0;	/* ignore MODE bits */
	      cir_data_zero = 0;
	    }
	  else
	    {
	      /* Check if it looks like TR-bit of RC6 protocol */
	      if (off <= CIR_BIT_PERIOD_RC6*3/2)
		cir_data_zero = 0;
	      else
		cir_data_zero = 1;
	    }
	}
      else if (cir_proto == CIR_PROTO_RC5 || cir_proto == CIR_PROTO_RC6)
	{
	  if (cir_data_zero)
	    {
	      cir_data <<= 1;

	      if (on > CIR_PERIOD_ON_RC5_OR_RC6)
		{
		  cir_data <<= 1;
		  cir_data |= 1;
		  cir_seq++;
		  if (off > CIR_PERIOD_ON_RC5_OR_RC6)
		    cir_data_zero = 1;
		  else
		    cir_data_zero = 0;
		}
	      else		/* Off must be short */
		cir_data_zero = 1;
	    }
	  else
	    {
	      cir_data <<= 1;
	      cir_data |= 1;

	      /* On must be short */
	      if (off > CIR_PERIOD_ON_RC5_OR_RC6)
		cir_data_zero = 1;
	      else
		cir_data_zero = 0;
	    }
	}

      cir_seq++;

#if defined(DEBUG_CIR)
      if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
	{
	  *cirinput_p++ = on;
	  *cirinput_p++ = off;
	}
      intr_trg++;
#endif

      TIM3->EGR = TIM_EGR_UG;	/* Generate UEV */
      TIM3->SR &= ~TIM_SR_TIF;
    }
  else
    /* overflow occurred */
    {
      systime_t now = chTimeNow ();

      TIM3->SR &= ~TIM_SR_UIF;

      if (on > 0)
	{
	  uint8_t ignore_input = 0;

	  /* Disable the timer */
	  TIM3->CR1 &= ~TIM_CR1_CEN;
	  TIM3->DIER = 0;

	  if (cir_seq == 12 || cir_seq == 15)
	    {
#if defined(DEBUG_CIR)
	      if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
		{
		  *cirinput_p++ = on;
		  *cirinput_p++ = 0xffff;
		}
#endif
	      cir_proto = CIR_PROTO_SONY;
	      cir_data >>= 1;
	      cir_data |= (on >= CIR_BIT_SIRC_PERIOD_ON) ? 0x80000000 : 0;
	      cir_data >>= (32 - cir_seq);
	      cir_seq++;
	    }
	  else
	    {
	      if (cir_proto == CIR_PROTO_RC5 || cir_proto == CIR_PROTO_RC6)
		{
		  cir_data <<= 1;
		  cir_seq++;
		  if (cir_data_zero)
		    {
		      if (on > CIR_PERIOD_ON_RC5_OR_RC6)
			{
			  cir_data <<= 1;
			  cir_data |= 1;
			  cir_seq++;
			}
		    }
		  else
		    cir_data |= 1;
		}
	      /* Or else, it must be the stop bit, just ignore */
	    }

	  if (now - cir_input_last < CIR_PERIOD_INHIBIT_CHATTER)
	    ignore_input = 1;
	  /* Remove ADDRESS bits and filter COMMAND bits */
	  else if (cir_proto == CIR_PROTO_SONY)
	    {
	      if (cir_seq == 1 + 12)
		cir_data = (cir_data + '1') & 0x007f;
	      else if (cir_seq == 1 + 15)
		{
		  cir_proto = CIR_PROTO_SONY;
		  cir_data = cir_data & 0x00ff;
		}
	      else
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_OTHER)
	    {
	      if (cir_seq == 1 + 32) /* Tested on Toshiba */
		/* XXX: Check integrity???
		 * (cir_data >> 16)&0xff == cir_data >> 24) ^ 0xff
		 */
		cir_data = (cir_data >> 16) & 0x00ff;
	      else if (cir_seq == 1 + 48) /* Tested on Sharp */
		/* check integrity ??? */
		cir_data = (cir_data >> 16) & 0x00ff;
	      else
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_RC6)
	    {
	      if (cir_seq == 16 || cir_seq == 32)
		cir_data &= 0x00ff;
	      else
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_RC5)
	    {
	      if (cir_seq == 14)
		cir_data &= 0x003f;
	      else
		ignore_input = 1;
	    }
	  else
	    ignore_input = 1;

	  if (ignore_input)
	    /* Ignore data received and enable CIR again */
	    cir_init ();
	  else
	    /* Notify thread */
	    chEvtSignal (pin_thread, (eventmask_t)1);

	  cir_input_last = now;

#if defined(DEBUG_CIR)
	  if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
	    {
	      *cirinput_p++ = 0xffff;
	      *cirinput_p++ = (uint16_t)chTimeNow ();
	    }
	  intr_ovf++;
#endif
	}
    }
}

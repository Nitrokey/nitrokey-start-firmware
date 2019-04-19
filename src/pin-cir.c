/*
 * pin-cir.c -- PIN input device support (Consumer Infra-Red)
 *
 * Copyright (C) 2010, 2011, 2013 Free Software Initiative of Japan
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

#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "board.h"
#include "gnuk.h"
#include "mcu/stm32f103.h"

#ifdef DEBUG
#define DEBUG_CIR 1
#endif

static int
cir_ext_disable (void)
{
  int rcvd = (EXTI->PR & EXTI_PR) != 0;

  EXTI->IMR &= ~EXTI_IMR;
  EXTI->PR |= EXTI_PR;
  return rcvd;
}

static void
cir_ext_enable (void)
{
  EXTI->PR |= EXTI_PR;
  EXTI->IMR |= EXTI_IMR;
}

static chopstx_mutex_t cir_input_mtx;
static chopstx_cond_t cir_input_cnd;
static int input_avail;

uint8_t pin_input_buffer[MAX_PIN_CHARS];
uint8_t pin_input_len;

/*
 * Supported/tested TV controllers:
 *
 *   Controller of Toshiba REGZA
 *   Controller of Sony BRAVIA
 *   Controller of Sharp AQUOS
 *   Dell Wireless Travel Remote MR425
 *
 * The code supports RC-5 protocol in fact, but I don't have any
 * controller at hand which I can test with, so I don't have any data
 * for controller of RC-5.
 *
 * Current code assumes following mapping:
 *
 *  --------------------------------------
 *    Protocol          Controller
 *  --------------------------------------
 *     RC-6               Dell MR425
 *     NEC                Toshiba REGZA
 *     Sharp              Sharp AQUOS
 *     Sony               Sony BRAVIA
 *  --------------------------------------
 *
 * In future, when I will have other controllers, this mapping will be
 * (should be) configurable, at compile time at least, preferably at
 * runtime.
 */

/*
 * Philips RC-5 Protocol: 14-bit (MSB first)
 *
 * Philips RC-6 Protocol: (START + 1 + MODE + TR + 32-bit / 16-bit) (MSB first)
 *                                     3-bit      (mode 6 / mode 0)
 *      Example: Controller of DELL (mode 6)
 *
 * NEC Protocol: (START + 32-bit + STOP) + (START + STOP) x N  (LSB first)
 *      32-bit: 8-bit command inverted + 8-bit command + 16-bit address
 * 	Example: Controller of Toshiba REGZA
 * 	Example: Controller of Ceiling Light by NEC
 *
 * Sony Protocol: (START + 12-bit) or (START + 15-bit)  (LSB first)
 *      12-bit: 5-bit address + 7-bit command
 * 	Example: Controller of Sony BRAVIA
 *
 * Sharp Protocol: (START + 48-bit + STOP) x N  (LSB first)
 *      48-bit: 32-bit address + 12-bit command + 4-bit parity
 *      Example: Controller of Sharp AQUOS
 *      Example: Controller of Ceiling Light by Mitsubishi
 *
 * Unsupported:
 *
 * Unknown Protocol 2: (START + 112-bit + STOP)  (LSB first)
 *      Example: Controller of Mitsubishi air conditioner
 *
 * Unknown Protocol 3: (START + 128-bit + STOP)  (LSB first)
 *      Example: Controller of Fujitsu air conditioner
 *
 * Unknown Protocol 4: (START + 152-bit + STOP)  (LSB first)
 *      Example: Controller of Sanyo air conditioner
 *
 */

/*
 * The implementation note of CIR signal decoding (on STM32).
 *
 * (1) Use EXTI interrupt to detect the first edge of signal.
 * (2) Use Timer (with PWM input mode) to measure timings of square wave.
 *
 */

/*
 * Timer settings.
 *
 * See THE reference manual (RM0008) section 15.3.6 PWM input mode.
 *
 * 72MHz
 * Prescaler = 72
 *
 * 1us
 *
 * TIMx_CR1
 *  CKD  = 00 (tDTS = tCK_INT)
 *  ARPE = 1  (buffered)
 *  CMS  = 00 (up counter)
 *  DIR  = 0  (up counter)
 *  OPM  = 0  (up counter)
 *  URS  = 1  (update request source: overflow only)
 *  UDIS = 0  (UEV (update event) enabled)
 *  CEN  = 1  (counter enable)
 *
 * TIMx_CR2
 *  TI1S = 1 (TI1 is XOR of ch1, ch2, ch3)
 *  MMS  = 000 (TRGO at Reset)
 *  CCDS = 0 (DMA on capture)
 *  RSVD = 000
 *
 * TIMx_SMCR
 *  ETP  = 0
 *  ECE  = 0
 *  ETPS = 00
 *  ETF  = 0000
 *  MSM  = 0
 *  TS   = 101 (TI1FP1 selected)
 *  RSVD = 0
 *  SMS  = 100 (Reset-mode)
 *
 * TIMx_DIER
 *
 * TIMx_SR
 *
 * TIMx_EGR
 *
 * TIMx_CCMR1
 *  CC1S = 01 (TI1 selected)
 *  CC2S = 10 (TI1 selected)
 *  IC1F = 1001 (fSAMPLING=fDTS/8, N=8)
 *  IC2F = 1001 (fSAMPLING=fDTS/8, N=8)
 *
 * TIMx_CCMR2
 *
 * TIMx_CCER
 *  CC2P = 1 (polarity = falling edge: TI1FP1)
 *  CC2E = 1
 *  CC1P = 0 (polarity = rising edge: TI1FP1)
 *  CC1E = 1
 *
 * TIMx_CNT
 * TIMx_PSC = 71
 * TIMx_ARR = 18000
 *
 * TIMx_CCR1  period
 * TIMx_CCR2  duty
 *
 * TIMx_DCR
 * TIMx_DMAR
 */

#if defined(DEBUG_CIR)
static uint16_t intr_ext;
static uint16_t intr_trg;
static uint16_t intr_ovf;
static uint16_t intr_err;

#define MAX_CIRINPUT_BIT 512
static uint16_t cirinput[MAX_CIRINPUT_BIT];
static uint16_t *cirinput_p;
#endif

static uint32_t cir_data;
static uint16_t cir_data_more;
static uint8_t cir_proto;
#define CIR_PROTO_RC5   1
#define CIR_PROTO_RC6   2
#define CIR_PROTO_OTHER 3
#define CIR_PROTO_SONY  4
#define CIR_PROTO_NEC   5
#define CIR_PROTO_SHARP 6


/* CIR_DATA_ZERO: Used for zero-bit handling of RC-5/RC-6 */
static uint8_t cir_data_zero;

static uint8_t cir_seq;

static void
cir_ll_init (void)
{
  cir_data = 0;
  cir_seq = 0;
  /* Don't touch cir_proto here */
  cir_ext_enable ();
}


#define CH_RETURN    0x0d
#define CH_BACKSPACE 0x08

struct codetable {
  uint16_t cir_code;
  uint8_t  char_code;
};

/* NOTE: no way to input '0' */
static const struct codetable
cir_codetable_dell_mr425[] = {
  {0x10, '7' },          /* Speaker Louder       */
  {0x11, '8' },          /* Speaker Quieter      */
  {0x0d, '9' },          /* Speaker Mute         */
  {0xce, 'a' },          /* Black triangle UP    */
  {0xcf, 'b' },          /* Black triangle DOWN  */
  {0x58, 'c' },          /* White triangle UP    */
  {0x5a, 'd' },          /* White triangle LEFT  */
  {0x5c, CH_RETURN },    /* Check                */
  {0x5b, 'e' },          /* White triangle RIGHT */
  {0xa4, CH_BACKSPACE }, /* Back                 */
  {0x59, 'f' },          /* White triangle DOWN  */
  {0x2f, '1' },          /* Rewind               */
  {0x2c, '2' },          /* Play / Pause         */
  {0x2e, '3' },          /* Forward              */
  {0x21, '4' },          /* Skip backward        */
  {0x31, '5' },          /* Stop                 */
  {0x20, '6' },          /* Skip forward         */

  {0, 0} /* <<END>>   */
};

#define CIR_ADDR_SHARP_AQUOS 0x028f
static const struct codetable
cir_codetable_aquos[] = {
  { 0x0116, ' ' }, /* Power */
  { 0x025e, '0' }, /* d */
  { 0x024e, '1' }, /* 1 */
  { 0x024f, '2' }, /* 2 */
  { 0x0250, '3' }, /* 3 */
  { 0x0251, '4' }, /* 4 */
  { 0x0252, '5' }, /* 5 */
  { 0x0253, '6' }, /* 6 */
  { 0x0254, '7' }, /* 7 */
  { 0x0255, '8' }, /* 8 */
  { 0x0256, '9' }, /* 9 */
  { 0x0257, 'a' }, /* 10/0 */
  { 0x0258, 'b' }, /* 11 */
  { 0x0259, 'c' }, /* 12 */
  { 0x0111, 'd' }, /* Ch ^ */
  { 0x0112, 'e' }, /* Ch v */
  { 0x0114, 'f' }, /* Vol + */
  { 0x0115, 'g' }, /* Vol - */
  { 0x0117, 'h' }, /* Mute */
  { 0x0280, 'i' }, /* BLUE */
  { 0x0281, 'j' }, /* RED */
  { 0x0282, 'k' }, /* GREEN */
  { 0x0283, 'l' }, /* YELLOW */
  { 0x011b, 'm' }, /* DISPLAY CONTROL (gamen hyouji) */
  { 0x01d5, 'n' }, /* DISPLAY SIZE */
  { 0x0157, 'o' }, /* UP */
  { 0x01d7, 'p' }, /* LEFT */
  { 0x01d8, 'q' }, /* RIGHT */
  { 0x0120, 'r' }, /* DOWN */
  { 0x0152, CH_RETURN }, /* Commit (kettei) */
  { 0x01e4, CH_BACKSPACE }, /* Back (modoru) */
  { 0x01f5, 's' }, /* Quit (shuuryou) */
  { 0x0b03, 't' }, /* Rewind (hayamodoshi) */
  { 0x0b01, 'u' }, /* Play (saisei) */
  { 0x0b04, 'v' }, /* Forward (hayaokuri) */
  { 0x0b02, 'w' }, /* Stop (teishi) */
  { 0x028a, 'x' }, /* BS */
  { 0x028b, 'y' }, /* CS */
  { 0x025f, 'z' }, /* Program information (bangumi jouhou) */
  { 0x0260, '\\' }, /* Program table (bangumi hyou) */
  { 0x0118, '|' }, /* Sound channel (onsei kirikae) */
  { 0x028e, '[' }, /* Ground Analog (chijou A) */
  { 0x0289, ']' }, /* Ground Digital (chijou D) */

  { 0x0b07, '\"' }, /* Feature select (kinou sentaku) */
  { 0x026b, '.' }, /* TV/Radio/Data (terebi/rajio/data) */
  { 0x025a, ',' }, /* 3 code input (3 keta nyuuryoku) */
  { 0x0267, ':' }, /* subtitle (jimaku) */
  { 0x0159, ';' }, /* hold (seishi) */

  { 0x01c4, 'A' }, /* Menu */
  { 0x011a, 'B' }, /* Off timer */
  { 0x0121, 'C' }, /* CATV */
  { 0x0b05, 'D' }, /* Record */
  { 0x0b06, 'E' }, /* Recording stop */
  { 0x0113, 'F' }, /* Inputs (nyuuryoku kirikae) */
  { 0x0275, 'G' }, /* other programs (ura bangumi) */
  { 0x0266, 'H' }, /* signal control (eizou kirikae) */
  { 0x01e7, 'I' }, /* AV position */
  { 0x027f, 'J' }, /* i.LINK */
  { 0x0b00, 'K' }, /* Recorder power */
  { 0x028f, 'L' }, /* as you like it (okonomi senkyoku) */

  {0, 0} /* <<END>>   */
};

#define CIR_ADDR_TOSHIBA_REGZA 0xbf40
static const struct codetable
cir_codetable_regza[] = {
  { 0x12, ' ' }, /* Power */
  { 0x14, '0' }, /* d (data) */
  { 0x01, '1' }, /* 1 */
  { 0x02, '2' }, /* 2 */
  { 0x03, '3' }, /* 3 */
  { 0x04, '4' }, /* 4 */
  { 0x05, '5' }, /* 5 */
  { 0x06, '6' }, /* 6 */
  { 0x07, '7' }, /* 7 */
  { 0x08, '8' }, /* 8 */
  { 0x09, '9' }, /* 9 */
  { 0x0a, 'a' }, /* 10 */
  { 0x0b, 'b' }, /* 11 */
  { 0x0c, 'c' }, /* 12 */
  { 0x1b, 'd' }, /* Ch ^ */
  { 0x1f, 'e' }, /* Ch v */
  { 0x1a, 'f' }, /* Vol + */
  { 0x1e, 'g' }, /* Vol - */
  { 0x10, 'h' }, /* Mute */
  { 0x73, 'i' }, /* BLUE */
  { 0x74, 'j' }, /* RED */
  { 0x75, 'k' }, /* GREEN */
  { 0x76, 'l' }, /* YELLOW */
  { 0x1c, 'm' }, /* Display control */
  { 0x2b, 'n' }, /* Display size */
  { 0x3e, 'o' }, /* UP */
  { 0x5f, 'p' }, /* LEFT */
  { 0x5b, 'q' }, /* RIGHT */
  { 0x3f, 'r' }, /* DOWN */
  { 0x3d, CH_RETURN }, /* Commit (kettei) */
  { 0x3b, CH_BACKSPACE }, /* Back (modoru) */
  { 0x3c, 's' }, /* Quit (shuuryou) */
  { 0x2c, 't' }, /* << (Rewind) */
  { 0x2d, 'u' }, /* >/|| (Play/Stop) */
  { 0x2e, 'v' }, /* >> (Forward) */
  { 0x2b, 'w' }, /* Stop (teishi) */
  { 0x7c, 'x' }, /* BS */
  { 0x7d, 'y' }, /* CS */
  { 0x71, 'z' }, /* Program information (bangumi setsumei) */
  { 0x77, '\\' }, /* Mini program table (mini bangumihyou) */
  { 0x13, '|' }, /* Sound (onta kirikae) */
  { 0x7a, '[' }, /* Ground Digital (chideji) */
  { 0x7b, ']' }, /* Ground Analog (chiana) */

  { 0xd0, '\"' }, /* Settings Menu (settei menu) */
  { 0x6d, '.' }, /* Radio/Data (rajio/data) */
  { 0x60, ',' }, /* CH 10-key input (search) */
  { 0x52, ':' }, /* subtitle (jimaku) */
  { 0x50, ';' }, /* hold (seishi) */

  { 0x3a, 'A' }, /* Input- (nyuuryokukirikae-) */
  { 0x0f, 'B' }, /* Input+ (nyuuryokukirikae+) */
  { 0x29, 'C' }, /* Two screens (nigamen) */
  { 0x25, 'D' }, /* Broadband */
  { 0x27, 'E' }, /* |<< Skip backward */
  { 0x26, 'F' }, /* >>| Skip forward  */
  { 0x61, '!' }, /* 1 NHK1 */
  { 0x62, '@' }, /* 2 NHK2 */
  { 0x63, '#' }, /* 3 NHKh */
  { 0x64, '$' }, /* 4 BS Nihon TV */
  { 0x65, '%' }, /* 5 BS Asahi */
  { 0x66, '^' }, /* 6 BS-i */
  { 0x67, '&' }, /* 7 BSJ */
  { 0x68, '*' }, /* 8 BS Fuji */
  { 0x69, '(' }, /* 9 WOW */
  { 0x6a, ')' }, /* 10 Star */
  { 0x6b, '-' }, /* 11 BS11 */
  { 0x6c, '+' }, /* 12 TwellV */
  { 0x27, '=' }, /* Quick (Delete) */
  { 0x34, '<' }, /* REGZA link */
  { 0x6e, '>' }, /* Program Table */
  { 0x20, '/' }, /* ^^ */
  { 0x22, '\'' }, /* << */
  { 0x23, '?' }, /* >> */
  { 0x21, '_' }, /* vv */

  {0, 0} /* <<END>>   */
};

static const struct codetable
cir_codetable_bravia[] = {
  { 0x15, ' ' }, /* Power */
  { 0x95, '0' }, /* d (16-bit: 0x4b) */
  { 0x00, '1' }, /* 1 */
  { 0x01, '2' }, /* 2 */
  { 0x02, '3' }, /* 3 */
  { 0x03, '4' }, /* 4 */
  { 0x04, '5' }, /* 5 */
  { 0x05, '6' }, /* 6 */
  { 0x06, '7' }, /* 7 */
  { 0x07, '8' }, /* 8 */
  { 0x08, '9' }, /* 9 */
  { 0x09, 'a' }, /* 10 */
  { 0x0a, 'b' }, /* 11 */
  { 0x0b, 'c' }, /* 12 */
  { 0x10, 'd' }, /* CH+ */
  { 0x11, 'd' }, /* CH- */
  { 0x12, 'f' }, /* Vol+ */
  { 0x13, 'g' }, /* Vol- */
  { 0x14, 'h' }, /* Mute */
  { 0xa4, 'i' }, /* BLUE (16-bit: 0x4b) */
  { 0xa5, 'j' }, /* RED (16-bit: 0x4b) */
  { 0xa6, 'k' }, /* GREEN (16-bit: 0x4b) */
  { 0xa7, 'l' }, /* YELLOW (16-bit: 0x4b) */
  { 0x3a, 'm' }, /* DISPLAY control (gamen hyouji) */
  { 0x3d, 'n' }, /* Display Wide (waido kirikae) */
  { 0x74, 'o' }, /* UP */
  { 0x75, 'p' }, /* DOWN */
  { 0x33, 'q' }, /* RIGHT */
  { 0x34, 'r' }, /* LEFT */
  { 0x65, CH_RETURN }, /* Commit (kettei) */
  { 0xa3, CH_BACKSPACE }, /* Back (modoru) (16-bit: 0x4b) */
  { 0xac, 's' }, /* BS (16-bit: 0x4b) */
  { 0xab, 't' }, /* CS (16-bit: 0x4b) */
  { 0x5b, 'u' }, /* Program table (bangumi hyou) (16-bit: 0x52) */
  { 0x17, 'v' }, /* Sound channel (onsei kirikae) */
  { 0xa8, 'w' }, /* subtitle (jimaku) (16-bit: 0x4b) */
  { 0x5c, 'x' }, /* hold (memo) */
  { 0xb6, 'y' }, /* Tool (16-bit: 0x4b) */
  { 0x8c, 'z' }, /* 10 key input (10ki-) (16-bit: 0x4b) */
  { 0x60, '!' }, /* Menu */
  { 0xae, '@' }, /* Analog (16-bit: 0x4b) */
  { 0xb2, '#' }, /* Digital (16-bit: 0x4b) */
  { 0x25, '$' }, /* Input (nyuuryoku kirikae) */

  {0, 0} /* <<END>>   */,
};

static int
ch_is_backspace (int ch)
{
  return ch == CH_BACKSPACE;
}

static int
ch_is_enter (int ch)
{
  return ch == CH_RETURN;
}

/* liner search is good enough for this small amount of data */
static uint8_t
find_char_codetable (uint32_t cir_code, const struct codetable *ctp)
{
  while (ctp->cir_code != 0x0000 || ctp->char_code != 0x00)
    if (ctp->cir_code == cir_code)
      return ctp->char_code;
    else
      ctp++;

  /* Not found */
  return cir_code & 0xff;
}

static int
hex (int x)
{
  if (x < 10)
    return x + '0';
  else
    return (x - 10) + 'a';
}

static int
check_input (void *arg)
{
  (void)arg;
  return input_avail;
}

static int
cir_getchar (uint32_t timeout)
{
  chopstx_poll_cond_t poll_desc;
  struct chx_poll_head *pd_array[1] = { (struct chx_poll_head *)&poll_desc };
  uint16_t cir_addr;
#if defined(DEBUG_CIR)
  uint16_t *p;
#endif

#if defined(DEBUG_CIR)
  cirinput_p = cirinput;
#endif

  cir_ll_init ();

  poll_desc.type = CHOPSTX_POLL_COND;
  poll_desc.ready = 0;
  poll_desc.cond = &cir_input_cnd;
  poll_desc.mutex = &cir_input_mtx;
  poll_desc.check = check_input;
  poll_desc.arg = NULL;

  input_avail = 0;
  if (chopstx_poll (&timeout, 1, pd_array) == 0)
    return -1;

  /* Sleep 200ms to avoid detecting chatter inputs.  */
  chopstx_usec_wait (200 * 1000);

#if defined(DEBUG_CIR)
  DEBUG_INFO ("****\r\n");
  DEBUG_SHORT (intr_ext);
  DEBUG_SHORT (intr_trg);
  DEBUG_SHORT (intr_ovf);
  DEBUG_INFO ("----\r\n");
  for (p = cirinput; p < cirinput_p; p++)
    {
      DEBUG_SHORT (*p);
    }
  DEBUG_INFO ("====\r\n");

  cirinput_p = cirinput;

  DEBUG_INFO ("**** CIR data:");
  DEBUG_WORD (cir_data);
  if (cir_seq > 48)
    {
      DEBUG_SHORT (cir_data_more);
    }
  DEBUG_BYTE (cir_seq);
#endif

  switch (cir_proto)
    {
    case CIR_PROTO_RC5:
      cir_data &= 0x003f;
      goto err;
    case CIR_PROTO_RC6:
      cir_addr = cir_data >> 8; /* in case of cir_seq == 16.  32??? */
      cir_data &= 0x00ff;
      return find_char_codetable (cir_data, cir_codetable_dell_mr425);
    case CIR_PROTO_NEC:
      cir_addr = cir_data&0xffff;
      if (cir_addr == CIR_ADDR_TOSHIBA_REGZA)
	{
	  cir_data = (cir_data >> 16) & 0x00ff;
	  return find_char_codetable (cir_data, cir_codetable_regza);
	}
      else
	goto err;
    case CIR_PROTO_SHARP:
      cir_addr = cir_data&0x0fff;
      if (cir_addr == CIR_ADDR_SHARP_AQUOS)
	{
	  cir_data = (cir_data>>16)&0x0fff;
	  return find_char_codetable (cir_data, cir_codetable_aquos);
	}
      else
	goto err;
    case CIR_PROTO_SONY:
      /* Remove ADDRESS bits and filter COMMAND bits */
      if (cir_seq == 1 + 12)
	{
	  cir_addr = cir_data >> 7;
	  cir_data = cir_data & 0x007f;
	  /* ADDRESS = 0x01 (5-bit) */
	}
      else
	{
	  cir_addr = cir_data >> 8;
	  cir_data = cir_data & 0x00ff;
	  /* ADDRESS = 0x4b or 0x52 (7-bit) */
	}
      return find_char_codetable (cir_data, cir_codetable_bravia);
    err:
    default:
      /* encode debug information */
      pin_input_len = 16;
      pin_input_buffer[0] = hex (cir_proto >> 4);
      pin_input_buffer[1] = hex (cir_proto & 0x0f);
      pin_input_buffer[2] = ':';
      pin_input_buffer[3] = hex ((cir_data >> 28) & 0x0f);
      pin_input_buffer[4] = hex ((cir_data >> 24) & 0x0f);
      pin_input_buffer[5] = hex ((cir_data >> 20) & 0x0f);
      pin_input_buffer[6] = hex ((cir_data >> 16) & 0x0f);
      pin_input_buffer[7] = hex ((cir_data >> 12) & 0x0f);
      pin_input_buffer[8] = hex ((cir_data >> 8) & 0x0f);
      pin_input_buffer[9] = hex ((cir_data >> 4) & 0x0f);
      pin_input_buffer[10] = hex (cir_data & 0x0f);
      pin_input_buffer[11] = ':';
      pin_input_buffer[12] = hex ((cir_data_more >> 12) & 0x0f);
      pin_input_buffer[13] = hex ((cir_data_more >> 8) & 0x0f);
      pin_input_buffer[14] = hex ((cir_data_more >> 4) & 0x0f);
      pin_input_buffer[15] = hex (cir_data_more & 0x0f);
      return CH_RETURN;
    }
}

/*
 * RC-5 protocol doesn't have a start bit, while any other protocols
 * have the one.
 */
#define CIR_BIT_START_RC5_DETECT 1600 /* RC-5: 889us, Sony start: 2400us */

#define CIR_BIT_START_RC5_LENGTH (889 + 889/2)
#define CIR_BIT_PERIOD_RC6 444
#define CIR_BIT_PERIOD 1500
#define CIR_BIT_SIRC_PERIOD_ON 1000

/*
 * Let user input PIN string.
 * Return length of the string.
 * The string itself is in PIN_INPUT_BUFFER.
 */
int
pinpad_getline (int msg_code, uint32_t timeout)
{
  (void)msg_code;

  DEBUG_INFO (">>>\r\n");

  pin_input_len = 0;
  while (1)
    {
      int ch;

      ch = cir_getchar (timeout);
      if (ch < 0)
	return 0;		/* timeout */

      if (ch_is_backspace (ch))
	{
	  led_blink (LED_TWOSHOTS);
	  if (pin_input_len > 0)
	    pin_input_len--;
	}
      else if (ch_is_enter (ch))
	break;
      else if (pin_input_len < MAX_PIN_CHARS)
	{
	  led_blink (LED_ONESHOT);
	  pin_input_buffer[pin_input_len++] = ch;
	}
    }

  cir_ext_disable ();

  return pin_input_len;
}

/**
 * @brief  Interrupt handler of EXTI.
 * @note   This handler will be invoked at the beginning of signal.
 *         Setup timer to measure period and duty using PWM input mode.
 */
static void
cir_ext_interrupt (void)
{
  int rcvd = cir_ext_disable ();

  if (!rcvd)
    return;

#if defined(DEBUG_CIR)
  intr_ext++;
  if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
    *cirinput_p++ = 0x0000;
#endif

  TIMx->EGR = TIM_EGR_UG;	/* Generate UEV to load PSC and ARR */
  /* Enable Timer */
  TIMx->SR &= ~(TIM_SR_UIF
		| TIM_SR_CC1IF | TIM_SR_CC2IF
		| TIM_SR_TIF
		| TIM_SR_CC1OF | TIM_SR_CC2OF);
  TIMx->DIER = TIM_DIER_UIE /*overflow*/ | TIM_DIER_TIE /*trigger*/;
  TIMx->CR1 |= TIM_CR1_CEN;
}

#define CIR_PERIOD_ON_RC5_OR_RC6 (((cir_proto == CIR_PROTO_RC5) ? 2 : 1) \
				  * CIR_BIT_PERIOD_RC6 * 3 / 2)

/**
 * @brief  Interrupt handler of timer.
 * @note   Timer is PWM input mode, this handler will be invoked on each cycle
 */
static void
cir_timer_interrupt (void)
{
  uint16_t period, on, off;

  period = TIMx->CCR1;
  on = TIMx->CCR2;
  off = period - on;

  if ((TIMx->SR & TIM_SR_TIF))
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
	  if (cir_seq == 1 + 16)
	    cir_data_more = (uint16_t)(cir_data >> 16);

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

      TIMx->EGR = TIM_EGR_UG;	/* Generate UEV */
      TIMx->SR &= ~TIM_SR_TIF;
    }
  else if ((TIMx->SR & TIM_SR_UIF))
    /* overflow occurred */
    {
      TIMx->SR &= ~TIM_SR_UIF;

      if (on > 0)
	{
	  uint8_t ignore_input = 0;

	  /* Disable the timer */
	  TIMx->CR1 &= ~TIM_CR1_CEN;
	  TIMx->DIER = 0;

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

	  if (cir_proto == CIR_PROTO_SONY)
	    {
	      if (cir_seq != 1 + 12 && cir_seq != 1 + 15)
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_OTHER)
	    {
	      if (cir_seq == 1 + 32)
		{
		  if (((cir_data >> 16) & 0xff) == ((cir_data >> 24) ^ 0xff))
		    cir_proto = CIR_PROTO_NEC;
		  else
		    ignore_input = 1;
		}
	      else if (cir_seq == 1 + 48)
		{
		  if ((cir_data >> 28) ==
		      (((cir_data_more >> 12) & 0x0f)
		       ^ ((cir_data_more >> 8) & 0x0f)
		       ^ ((cir_data_more >> 4) & 0x0f)
		       ^ (cir_data_more & 0x0f)
		       ^ ((cir_data >> 24) & 0x0f)
		       ^ ((cir_data >> 20) & 0x0f) ^ ((cir_data >> 16) & 0x0f)
		       ^ ((cir_data >> 12) & 0x0f) ^ ((cir_data >> 8) & 0x0f)
		       ^ ((cir_data >> 4) & 0x0f) ^ (cir_data & 0x0f)))
		    cir_proto = CIR_PROTO_SHARP;
		  else
		    ignore_input = 1;
		}
	      else
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_RC6)
	    {
	      if (cir_seq != 16 && cir_seq != 32)
		ignore_input = 1;
	    }
	  else if (cir_proto == CIR_PROTO_RC5)
	    {
	      if (cir_seq != 14)
		ignore_input = 1;
	    }
	  else
	    ignore_input = 1;

	  if (ignore_input)
	    /* Ignore data received and enable CIR again */
	    cir_ll_init ();
	  else
	    {
	      /*
	       * Notify the thread, when it's waiting the input.
	       * If else, the input is thrown away.
	       */
	      chopstx_mutex_lock (&cir_input_mtx);
	      input_avail = 1;
	      chopstx_cond_signal (&cir_input_cnd);
	      chopstx_mutex_unlock (&cir_input_mtx);
	    }

#if defined(DEBUG_CIR)
	  if (cirinput_p - cirinput < MAX_CIRINPUT_BIT)
	    *cirinput_p++ = 0xffff;

	  intr_ovf++;
#endif
	}
    }
#if defined(DEBUG_CIR)
  else
    intr_err++;
#endif
}


#define STACK_PROCESS_6
#define STACK_PROCESS_7
#include "stack-def.h"
#define STACK_ADDR_TIM ((uintptr_t)process6_base)
#define STACK_SIZE_TIM (sizeof process6_base)
#define STACK_ADDR_EXT ((uintptr_t)process7_base)
#define STACK_SIZE_EXT (sizeof process7_base)

#define PRIO_TIM 4

static void *
tim_main (void *arg)
{
  chopstx_intr_t interrupt;

  (void)arg;
  chopstx_claim_irq (&interrupt, INTR_REQ_TIM);

  while (1)
    {
      chopstx_intr_wait (&interrupt);
      cir_timer_interrupt ();
      chopstx_intr_done (&interrupt);
    }

  return NULL;
}


#define PRIO_EXT 4

static void *
ext_main (void *arg)
{
  chopstx_intr_t interrupt;

  (void)arg;
  chopstx_claim_irq (&interrupt, INTR_REQ_EXTI);

  while (1)
    {
      chopstx_intr_wait (&interrupt);
      cir_ext_interrupt ();
      chopstx_intr_done (&interrupt);
    }

  return NULL;
}


void
cir_init (void)
{
  chopstx_mutex_init (&cir_input_mtx);
  chopstx_cond_init (&cir_input_cnd);

  /*
   * We use XOR function for three signals: TIMx_CH1, TIMx_CH2, and TIMx_CH3.
   *
   * This is because we want to invert the signal (of Vout from CIR
   * receiver module) for timer.
   *
   * For FST-01, TIM2_CH3 is the signal.  We set TIM2_CH1 = 1 and
   * TIM2_CH2 = 0.
   *
   * For STM8S, TIM2_CH2 is the signal.  We set TIM2_CH1 = 1 and
   * TIMx_CH3 = 0.
   */

  /* EXTIx <= Py */
  AFIO->EXTICR[AFIO_EXTICR_INDEX] = AFIO_EXTICR1_EXTIx_Py;
  EXTI->IMR &= ~EXTI_IMR;
  EXTI->FTSR |= EXTI_FTSR_TR;

  /* TIM */
#ifdef ENABLE_RCC_APB1
  RCC->APB1ENR |= RCC_APBnENR_TIMxEN;
  RCC->APB1RSTR = RCC_APBnRSTR_TIMxRST;
  RCC->APB1RSTR = 0;
#elif ENABLE_RCC_APB2
  RCC->APB2ENR |= RCC_APBnENR_TIMxEN;
  RCC->APB2RSTR = RCC_APBnRSTR_TIMxRST;
  RCC->APB2RSTR = 0;
#endif

  TIMx->CR1 = TIM_CR1_URS | TIM_CR1_ARPE;
  TIMx->CR2 = TIM_CR2_TI1S;
  TIMx->SMCR = TIM_SMCR_TS_0 | TIM_SMCR_TS_2 | TIM_SMCR_SMS_2;
  TIMx->DIER = 0;		/* Disable interrupt for now */
  TIMx->CCMR1 = TIM_CCMR1_CC1S_0 | TIM_CCMR1_IC1F_0 | TIM_CCMR1_IC1F_3
    | TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_3;
  TIMx->CCMR2 = 0;
  TIMx->CCER =  TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC2P;
  TIMx->PSC = 72 - 1;		/* 1 MHz */
  TIMx->ARR = 18000;		/* 18 ms */
  /* Generate UEV to upload PSC and ARR */
  TIMx->EGR = TIM_EGR_UG;

  chopstx_create (PRIO_TIM, STACK_ADDR_TIM, STACK_SIZE_TIM, tim_main, NULL);
  chopstx_create (PRIO_EXT, STACK_ADDR_EXT, STACK_SIZE_EXT, ext_main, NULL);
}

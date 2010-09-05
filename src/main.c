/*
 * main.c - main routine of Gnuk
 *
 * Copyright (C) 2010 Free Software Initiative of Japan
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
#include "usb_lld.h"

#include "gnuk.h"

#include "usb_lib.h"
#include "usb_istr.h"
#include "usb_desc.h"
#include "hw_config.h"
#include "usb_pwr.h"

#ifdef DEBUG
struct stdout {
  Mutex m;
  CondVar start_cnd;
  CondVar finish_cnd;
  const char *str;
  int size;
};

static struct stdout stdout;

static void
stdout_init (void)
{
  chMtxInit (&stdout.m);
  chCondInit (&stdout.start_cnd);
  chCondInit (&stdout.finish_cnd);
  stdout.size = 0;
  stdout.str = NULL;
}

void
_write (const char *s, int size)
{
  if (size == 0)
    return;

  chMtxLock (&stdout.m);
  while (stdout.str)
    chCondWait (&stdout.finish_cnd);
  stdout.str = s;
  stdout.size = size;
  chCondSignal (&stdout.start_cnd);
  chCondWait (&stdout.finish_cnd);
  chMtxUnlock ();
}

extern uint32_t count_in;
extern __IO uint32_t count_out;
extern uint8_t buffer_in[VIRTUAL_COM_PORT_DATA_SIZE];
extern uint8_t buffer_out[VIRTUAL_COM_PORT_DATA_SIZE];

static WORKING_AREA(waSTDOUTthread, 128);
static msg_t
STDOUTthread (void *arg)
{
  (void)arg;

 again:

  while (1)
    {
      if (bDeviceState == CONFIGURED)
	break;

      chThdSleepMilliseconds (100);
    }

  while (1)
    {
      const char *p;
      int len;

      if (bDeviceState != CONFIGURED)
	break;

      chMtxLock (&stdout.m);
      if (stdout.str == NULL)
	chCondWait (&stdout.start_cnd);

      p = stdout.str;
      len = stdout.size;
      while (len > 0)
	{
	  int i;

	  if (len < VIRTUAL_COM_PORT_DATA_SIZE)
	    {
	      for (i = 0; i < len; i++)
		buffer_in[i] = p[i];
	      count_in = len;
	      len = 0;
	    }
	  else
	    {
	      for (i = 0; i < VIRTUAL_COM_PORT_DATA_SIZE; i++)
		buffer_in[i] = p[i];
	      len -= VIRTUAL_COM_PORT_DATA_SIZE;
	      count_in = VIRTUAL_COM_PORT_DATA_SIZE;
	      p += count_in;
	    }

	  USB_SIL_Write (EP3_IN, buffer_in, count_in);
	  SetEPTxValid (ENDP3);

	  while (count_in > 0)
	    chThdSleepMilliseconds (1);
	}

      stdout.str = NULL;
      stdout.size = 0;
      chCondBroadcast (&stdout.finish_cnd);
      chMtxUnlock ();
    }

  goto again;
  return 0;
}
#else
void
_write (const char *s, int size)
{
  (void)s;
  (void)size;
}
#endif

static WORKING_AREA(waUSBthread, 128);
extern msg_t USBthread (void *arg);

static WORKING_AREA(waGPGthread, 128*16);
extern msg_t GPGthread (void *arg);

Thread *blinker_thread;
/*
 * Red LEDs blinker
 */
#define EV_LED (eventmask_t)1

/*
 * Entry point, note, the main() function is already a thread in the system
 * on entry.
 */
int
main (int argc, char **argv)
{
  eventmask_t m;
  int count = 0;

  (void)argc;
  (void)argv;

  blinker_thread = chThdSelf ();

  flash_init ();
  gpg_do_table_init ();

  usb_lld_init ();
  USB_Init();

#ifdef DEBUG
  stdout_init ();

  /*
   * Creates 'stdout' thread.
   */
  chThdCreateStatic (waSTDOUTthread, sizeof(waSTDOUTthread), NORMALPRIO, STDOUTthread, NULL);
#endif

  chThdCreateStatic (waUSBthread, sizeof(waUSBthread), NORMALPRIO, USBthread, NULL);
  chThdCreateStatic (waGPGthread, sizeof(waGPGthread), NORMALPRIO, GPGthread, NULL);

  while (1)
    {
#if 0
      if (palReadPad(IOPORT1, GPIOA_BUTTON))
	palSetPad (IOPORT3, GPIOC_LED);
#endif

      m = chEvtWaitOneTimeout (ALL_EVENTS, 100);
      if (m == EV_LED)
	palClearPad (IOPORT3, GPIOC_LED);

#ifdef DEBUG_MORE
      if (bDeviceState == CONFIGURED && (count % 100) == 0)
	{
	  DEBUG_WORD (count / 100);
	  _write ("\r\nThis is ChibiOS 2.0.2 on Olimex STM32-H103.\r\n"
		  "Testing USB driver.\n\n"
		  "Hello world\r\n\r\n", 47+21+15);
	}
#endif

      m = chEvtWaitOneTimeout (ALL_EVENTS, 100);
      if (m == EV_LED)
	palSetPad (IOPORT3, GPIOC_LED);

      count++;
    }

  return 0;
}

void
fatal (void)
{
  _write ("fatal\r\n", 7);
  for (;;);
}

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

#include "ch.h"
#include "hal.h"
#include "usb_lld.h"

#include "gnuk.h"

#include "usb_lib.h"
#include "usb_istr.h"
#include "usb_desc.h"
#include "hw_config.h"
#include "usb_pwr.h"

Thread *blinker_thread;
/*
 * Red LEDs blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t
Thread1 (void *arg)
{
  (void)arg;
  blinker_thread = chThdSelf ();
  while (1)
    {
      palClearPad (IOPORT3, GPIOC_LED);
      chEvtWaitOne (ALL_EVENTS);
      palSetPad (IOPORT3, GPIOC_LED);
      chEvtWaitOne (ALL_EVENTS);
    }
  return 0;
}

#ifdef DEBUG
static
struct stdout {
  Mutex m;
  CondVar start_cnd;
  CondVar finish_cnd;
  const char *str;
  int size;
} stdout;

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
extern void USB_Init (void);

static WORKING_AREA(waSTDOUTthread, 128);
static msg_t STDOUTthread (void *arg)
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

	  USB_SIL_Write (EP1_IN, buffer_in, count_in);
	  SetEPTxValid (ENDP1);

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
static void
stdout_init (void)
{
}

void
_write (const char *s, int size)
{
  (void)s;
  (void)size;
}
#endif

static WORKING_AREA(waUSBthread, 128*2);
extern msg_t USBthread (void *arg);

static WORKING_AREA(waGPGthread, 128*16);
extern msg_t GPGthread (void *arg);

/*
 * XXX: I have tried havege_rand, but it requires too much memory...
 */
/*
 * Multiply-with-carry method by George Marsaglia
 */
static uint32_t m_w;
static uint32_t m_z;

static void
random_init (void)
{
  static uint8_t s = 0;

 again:
  switch ((s & 1))
    {
    case 0:
      m_w = (m_w << 8) ^ hardclock ();
      break;
    case 1:
      m_z = (m_z << 8) ^ hardclock ();
      break;
    }

  s++;
  if (m_w == 0 || m_z == 0)
    goto again;
}

uint32_t
get_random (void)
{
  m_z = 36969 * (m_z & 65535) + (m_z >> 16);
  m_w = 18000 * (m_w & 65535) + (m_w >> 16);

  return (m_z << 16) + m_w;
}

uint8_t dek[16];
uint8_t *get_data_encryption_key (void)
{
  uint32_t r;
  r = get_random ();
  memcpy (dek, &r, 4);
  r = get_random ();
  memcpy (dek+4, &r, 4);
  r = get_random ();
  memcpy (dek+8, &r, 4);
  r = get_random ();
  memcpy (dek+12, &r, 4);
  return dek;
}

void dek_free (uint8_t *dek)
{
  (void)dek;
}


/*
 * Entry point, note, the main() function is already a thread in the system
 * on entry.
 */
int
main (int argc, char **argv)
{
  int count = 0;

  (void)argc;
  (void)argv;

  gpg_do_table_init ();

  usb_lld_init ();
  USB_Init();

  stdout_init ();
  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic (waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

#ifdef DEBUG
  /*
   * Creates 'stdout' thread.
   */
  chThdCreateStatic (waSTDOUTthread2, sizeof(waSTDOUTthread2), NORMALPRIO, STDOUTthread2, NULL);
#endif

  chThdCreateStatic (waUSBthread, sizeof(waUSBthread), NORMALPRIO, USBthread, NULL);
  chThdCreateStatic (waGPGthread, sizeof(waGPGthread), NORMALPRIO, GPGthread, NULL);

  while (1)
    {
#if 0
      if (palReadPad(IOPORT1, GPIOA_BUTTON))
	palSetPad (IOPORT3, GPIOC_LED);
#endif

      chThdSleepMilliseconds (100);

      if (bDeviceState != CONFIGURED)
	random_init ();

      if (bDeviceState == CONFIGURED && (count % 300) == 0)
	{
	  uint32_t r;
	  r = get_random ();

	  DEBUG_SHORT (r);
	  _write ("\r\nThis is ChibiOS 2.0.2 on Olimex STM32-H103.\r\n"
		  "Testing USB driver.\n\n"
		  "Hello world\r\n\r\n", 47+21+15);
	}
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

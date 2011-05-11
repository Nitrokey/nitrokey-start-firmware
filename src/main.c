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

#include "usb_lib.h"

#include "ch.h"
#include "gnuk.h"
#include "usb_lld.h"
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

Thread *stdout_thread;
uint32_t count_in;
uint8_t buffer_in[VIRTUAL_COM_PORT_DATA_SIZE];

static WORKING_AREA(waSTDOUTthread, 128);

static msg_t
STDOUTthread (void *arg)
{
  (void)arg;
  stdout_thread = chThdSelf ();

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

	  chEvtClear (EV_TX_READY);

	  USB_SIL_Write (EP3_IN, buffer_in, count_in);
	  SetEPTxValid (ENDP3);

	  chEvtWaitOne (EV_TX_READY);
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

/*
 * main thread does 1-bit LED display output
 */
#define LED_TIMEOUT_INTERVAL	MS2ST(100)
#define LED_TIMEOUT_ZERO	MS2ST(50)
#define LED_TIMEOUT_ONE		MS2ST(200)
#define LED_TIMEOUT_STOP	MS2ST(500)


#define ID_OFFSET 12
static void
device_initialize_once (void)
{
  const uint8_t *p = &gnukStringSerial[ID_OFFSET];

  if (p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff)
    {
      /*
       * This is the first time invocation.
       * Setup serial number by unique device ID.
       */
      const uint8_t *u = unique_device_id ();
      int i;

      for (i = 0; i < 4; i++)
	{
	  uint8_t b = u[i];
	  uint8_t nibble; 

	  nibble = (b >> 4);
	  nibble += (nibble >= 10 ? ('A' - 10) : '0');
	  flash_put_data_internal (&p[i*4], nibble << 8);
	  nibble = (b & 0x0f);
	  nibble += (nibble >= 10 ? ('A' - 10) : '0');
	  flash_put_data_internal (&p[i*4+2], nibble << 8);
	}
    }
}

static volatile uint8_t fatal_code;

/*
 * Entry point.
 *
 * NOTE: the main function is already a thread in the system on entry.
 *       See the hwinit1_common function.
 */
int
main (int argc, char **argv)
{
  int count = 0;

  (void)argc;
  (void)argv;

  device_initialize_once ();
  usb_lld_init ();
  USB_Init();

#ifdef DEBUG
  stdout_init ();

  /*
   * Creates 'stdout' thread.
   */
  chThdCreateStatic (waSTDOUTthread, sizeof(waSTDOUTthread),
		     NORMALPRIO, STDOUTthread, NULL);
#endif

  chThdCreateStatic (waUSBthread, sizeof(waUSBthread),
		     NORMALPRIO, USBthread, NULL);

  while (1)
    {
      count++;

      if (fatal_code != 0)
	{
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_INTERVAL);
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_INTERVAL);
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_STOP);
	  set_led (1);
	  if (fatal_code & 1)
	    chThdSleep (LED_TIMEOUT_ONE);
	  else
	    chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_INTERVAL);
	  set_led (1);
	  if (fatal_code & 2)
	    chThdSleep (LED_TIMEOUT_ONE);
	  else
	    chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_INTERVAL);
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_STOP);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_INTERVAL);
	} 

      if (bDeviceState != CONFIGURED)
	{
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_STOP * 3);
	}
      else
	/* Device configured */
	if (icc_state == ICC_STATE_START)
	  {
	    set_led (1);
	    chThdSleep (LED_TIMEOUT_ONE);
	    set_led (0);
	    chThdSleep (LED_TIMEOUT_STOP * 3);
	  }
	else
	  /* GPGthread  running */
	  {
	    set_led (1);
	    if ((auth_status & AC_ADMIN_AUTHORIZED) != 0)
	      chThdSleep (LED_TIMEOUT_ONE);
	    else
	      chThdSleep (LED_TIMEOUT_ZERO);
	    set_led (0);
	    chThdSleep (LED_TIMEOUT_INTERVAL);
	    set_led (1);
	    if ((auth_status & AC_OTHER_AUTHORIZED) != 0)
	      chThdSleep (LED_TIMEOUT_ONE);
	    else
	      chThdSleep (LED_TIMEOUT_ZERO);
	    set_led (0);
	    chThdSleep (LED_TIMEOUT_INTERVAL);
	    set_led (1);
	    if ((auth_status & AC_PSO_CDS_AUTHORIZED) != 0)
	      chThdSleep (LED_TIMEOUT_ONE);
	    else
	      chThdSleep (LED_TIMEOUT_ZERO);

	    if (icc_state == ICC_STATE_WAIT)
	      {
		set_led (0);
		chThdSleep (LED_TIMEOUT_STOP * 2);
	      }
	    else if (icc_state == ICC_STATE_RECEIVE)
	      {
		set_led (0);
		chThdSleep (LED_TIMEOUT_INTERVAL);
		set_led (1);
		chThdSleep (LED_TIMEOUT_ONE);
		set_led (0);
		chThdSleep (LED_TIMEOUT_STOP);
	      }
	    else
	      {
		set_led (0);
		chThdSleep (LED_TIMEOUT_INTERVAL);
		set_led (1);
		chThdSleep (LED_TIMEOUT_STOP);
		set_led (0);
		chThdSleep (LED_TIMEOUT_INTERVAL);
	      }
	  }

#ifdef DEBUG_MORE
      if (bDeviceState == CONFIGURED && (count % 10) == 0)
	{
	  DEBUG_SHORT (count / 10);
	  _write ("\r\nThis is ChibiOS 2.0.8 on STM32.\r\n"
		  "Testing USB driver.\n\n"
		  "Hello world\r\n\r\n", 35+21+15);
	}
#endif
    }

  return 0;
}

void
fatal (uint8_t code)
{
  fatal_code = code;
  _write ("fatal\r\n", 7);
  for (;;);
}

/*
 * main.c - main routine of Gnuk
 *
 * Copyright (C) 2010, 2011, 2012 Free Software Initiative of Japan
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
#include "gnuk.h"
#include "usb_lld.h"
#include "usb-cdc.h"

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
      while (1)
	{
	  int i;

	  if (len == 0)
	    if (count_in != VIRTUAL_COM_PORT_DATA_SIZE)
	      break;

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

	  usb_lld_write (ENDP3, buffer_in, count_in);

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

void
EP3_IN_Callback (void)
{
  if (stdout_thread)
    chEvtSignalI (stdout_thread, EV_TX_READY);
}

void
EP5_OUT_Callback (void)
{
  usb_lld_rx_enable (ENDP5);
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


#define ID_OFFSET 22
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
	  flash_put_data_internal (&p[i*4], nibble);
	  nibble = (b & 0x0f);
	  nibble += (nibble >= 10 ? ('A' - 10) : '0');
	  flash_put_data_internal (&p[i*4+2], nibble);
	}
    }
}

static volatile uint8_t fatal_code;

Thread *main_thread;

#define GNUK_INIT           0
#define GNUK_RUNNING        1
#define GNUK_INPUT_WAIT     2
#define GNUK_FATAL        255
/*
 * 0 for initializing
 * 1 for normal mode
 * 2 for input waiting
 * 255 for fatal
 */
static uint8_t main_mode;

static void display_interaction (void)
{
  eventmask_t m;

  while (1)
    {
      m = chEvtWaitOne (ALL_EVENTS);
      set_led (1);
      switch (m)
	{
	case LED_ONESHOT_SHORT:
	  chThdSleep (MS2ST (100));
	  break;
	case LED_ONESHOT_LONG:
	  chThdSleep (MS2ST (400));
	  break;
	case LED_TWOSHOT:
	  chThdSleep (MS2ST (50));
	  set_led (0);
	  chThdSleep (MS2ST (50));
	  set_led (1);
	  chThdSleep (MS2ST (50));
	  break;
	case LED_STATUS_MODE:
	  chThdSleep (MS2ST (400));
	  set_led (0);
	  return;
	case LED_FATAL_MODE:
	  main_mode = GNUK_FATAL;
	  set_led (0);
	  return;
	default:
	  break;
	}
      set_led (0);
    }
}

static void display_fatal_code (void)
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

static void display_status_code (void)
{
  enum icc_state icc_state;

  if (icc_state_p == NULL)
    icc_state = ICC_STATE_START;
  else
    icc_state = *icc_state_p;

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
}

void
led_blink (int spec)
{
  if (spec == 0)
    chEvtSignal (main_thread, LED_ONESHOT_SHORT);
  else if (spec == 1)
    chEvtSignal (main_thread, LED_ONESHOT_LONG);
  else
    chEvtSignal (main_thread, LED_TWOSHOT);
}

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

  main_thread = chThdSelf ();

  flash_unlock ();
  device_initialize_once ();
  usb_lld_init ();
  random_init ();

  while (1)
    {
      if (bDeviceState != UNCONNECTED)
	break;

      chThdSleepMilliseconds (250);
    }

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

#ifdef PINPAD_DND_SUPPORT
  msc_init ();
#endif

  while (1)
    {
      eventmask_t m;

      count++;
      m = chEvtWaitOneTimeout (ALL_EVENTS, LED_TIMEOUT_INTERVAL);
      switch (m)
	{
	case LED_STATUS_MODE:
	  main_mode = GNUK_RUNNING;
	  break;
	case LED_FATAL_MODE:
	  main_mode = GNUK_FATAL;
	  break;
	case LED_INPUT_MODE:
	  main_mode = GNUK_INPUT_WAIT;
	  set_led (1);
	  chThdSleep (MS2ST (400));
	  set_led (0);
	  break;
	default:
	  break;
	}

      switch (main_mode)
	{
	case GNUK_FATAL:
	  display_fatal_code ();
	  break;
	case GNUK_INIT:
	  set_led (1);
	  chThdSleep (LED_TIMEOUT_ZERO);
	  set_led (0);
	  chThdSleep (LED_TIMEOUT_STOP * 3);
	  break;
	case GNUK_INPUT_WAIT:
	  display_interaction ();
	  break;
	case GNUK_RUNNING:
	default:
	  display_status_code ();
	  break;
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
  chEvtSignal (main_thread, LED_FATAL_MODE);
  _write ("fatal\r\n", 7);
  for (;;);
}

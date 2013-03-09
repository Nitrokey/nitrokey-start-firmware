/*
 * main.c - main routine of Gnuk
 *
 * Copyright (C) 2010, 2011, 2012, 2013 Free Software Initiative of Japan
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
#include "sys.h"
#include "adc.h"
#include "gnuk.h"
#include "usb_lld.h"
#include "usb-cdc.h"
#include "random.h"

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
    chEvtSignalFlagsI (stdout_thread, EV_TX_READY);
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
#define MAIN_TIMEOUT_INTERVAL	MS2ST(5000)

#define LED_TIMEOUT_INTERVAL	MS2ST(75)
#define LED_TIMEOUT_ZERO	MS2ST(25)
#define LED_TIMEOUT_ONE		MS2ST(100)
#define LED_TIMEOUT_STOP	MS2ST(200)


/* It has two-byte prefix and content is "FSIJ-1.0.1-" (2 + 11*2).  */
#define ID_OFFSET 24
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

static Thread *main_thread;

static void display_fatal_code (void)
{
  while (1)
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
      chThdSleep (LED_TIMEOUT_INTERVAL*10);
    }
}

static uint8_t led_inverted;

static eventmask_t emit_led (int on_time, int off_time)
{
  eventmask_t m;

  set_led (!led_inverted);
  m = chEvtWaitOneTimeout (ALL_EVENTS, on_time);
  set_led (led_inverted);
  if (m) return m;
  if ((m = chEvtWaitOneTimeout (ALL_EVENTS, off_time)))
    return m;
  return 0;
}

static eventmask_t display_status_code (void)
{
  enum icc_state icc_state;
  eventmask_t m;

  if (icc_state_p == NULL)
    icc_state = ICC_STATE_START;
  else
    icc_state = *icc_state_p;

  if (icc_state == ICC_STATE_START)
    return emit_led (LED_TIMEOUT_ONE, LED_TIMEOUT_STOP);
  else
    /* GPGthread  running */
    {
      if ((m = emit_led ((auth_status & AC_ADMIN_AUTHORIZED)?
			  LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO,
			  LED_TIMEOUT_INTERVAL)))
	return m;
      if ((m = emit_led ((auth_status & AC_OTHER_AUTHORIZED)?
			  LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO,
			  LED_TIMEOUT_INTERVAL)))
	return m;
      if ((m = emit_led ((auth_status & AC_PSO_CDS_AUTHORIZED)?
			  LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO,
			  LED_TIMEOUT_INTERVAL)))
	return m;

      if (icc_state == ICC_STATE_WAIT)
	{
	  if ((m = chEvtWaitOneTimeout (ALL_EVENTS, LED_TIMEOUT_STOP * 2)))
	    return m;
	}
      else
	{
	  if ((m = chEvtWaitOneTimeout (ALL_EVENTS, LED_TIMEOUT_INTERVAL)))
	    return m;

	  if ((m = emit_led (icc_state == ICC_STATE_RECEIVE?
			      LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO,
			      LED_TIMEOUT_STOP)))
	    return m;
	}

      return 0;
    }
}

void
led_blink (int spec)
{
  chEvtSignalFlags (main_thread, spec);
}

/*
 * In Gnuk 1.0.[12], reGNUal was not relocatable.
 * Now, it's relocatable, but we need to calculate its entry address
 * based on it's pre-defined address.
 */
#define REGNUAL_START_ADDRESS_COMPATIBLE 0x20001400
static uint32_t
calculate_regnual_entry_address (const uint8_t *addr)
{
  const uint8_t *p = addr + 4;
  uint32_t v = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);

  v -= REGNUAL_START_ADDRESS_COMPATIBLE;
  v += (uint32_t)addr;
  return v;
}


/*
 * Entry point.
 *
 * NOTE: the main function is already a thread in the system on entry.
 *       See the hwinit1_common function.
 */
int
main (int argc, char *argv[])
{
  unsigned int count = 0;
  uint32_t entry;

  (void)argc;
  (void)argv;

  flash_unlock ();
  device_initialize_once ();

  halInit ();
  adc_init ();
  chSysInit ();

  main_thread = chThdSelf ();

  usb_lld_init (usb_initial_feature);
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

      if (icc_state_p != NULL && *icc_state_p == ICC_STATE_EXEC_REQUESTED)
	break;

      m = chEvtWaitOneTimeout (ALL_EVENTS, MAIN_TIMEOUT_INTERVAL);
    got_it:
      count++;
      switch (m)
	{
	case LED_ONESHOT:
	  if ((m = emit_led (MS2ST (100), MAIN_TIMEOUT_INTERVAL))) goto got_it;
	  break;
	case LED_TWOSHOTS:
	  if ((m = emit_led (MS2ST (50), MS2ST (50)))) goto got_it;
	  if ((m = emit_led (MS2ST (50), MAIN_TIMEOUT_INTERVAL))) goto got_it;
	  break;
	case LED_SHOW_STATUS:
	  if ((count & 0x07) != 0) continue; /* Display once for eight times */
	  if ((m = display_status_code ())) goto got_it;
	  break;
	case LED_START_COMMAND:
	  set_led (1);
	  led_inverted = 1;
	  break;
	case LED_FINISH_COMMAND:
	  m = chEvtWaitOneTimeout (ALL_EVENTS, LED_TIMEOUT_STOP);
	  led_inverted = 0;
	  set_led (0);
	  if (m)
	    goto got_it;
	  break;
	case LED_FATAL:
	  display_fatal_code ();
	  break;
	default:
	  if ((m = emit_led (LED_TIMEOUT_ZERO, LED_TIMEOUT_STOP)))
	    goto got_it;
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

  random_fini ();

  set_led (1);
  usb_lld_shutdown ();
  /* Disable SysTick */
  SysTick->CTRL = 0;
  /* Disable all interrupts */
  port_disable ();
  /* Set vector */
  SCB->VTOR = (uint32_t)&_regnual_start;
  entry = calculate_regnual_entry_address (&_regnual_start);
#ifdef DFU_SUPPORT
#define FLASH_SYS_START_ADDR 0x08000000
#define FLASH_SYS_END_ADDR (0x08000000+0x1000)
  {
    extern uint8_t _sys;
    uint32_t addr;
    handler *new_vector = (handler *)FLASH_SYS_START_ADDR;
    void (*func) (void (*)(void)) = (void (*)(void (*)(void)))new_vector[10];

    /* Kill DFU */
    for (addr = FLASH_SYS_START_ADDR; addr < FLASH_SYS_END_ADDR;
	 addr += FLASH_PAGE_SIZE)
      flash_erase_page (addr);

    /* copy system service routines */
    flash_write (FLASH_SYS_START_ADDR, &_sys, 0x1000);

    /* Leave Gnuk to exec reGNUal */
    (*func) ((void (*)(void))entry);
    for (;;);
  }
#else
  /* Leave Gnuk to exec reGNUal */
  flash_erase_all_and_exec ((void (*)(void))entry);
#endif

  /* Never reached */
  return 0;
}

void
fatal (uint8_t code)
{
  fatal_code = code;
  chEvtSignalFlags (main_thread, LED_FATAL);
  _write ("fatal\r\n", 7);
  for (;;);
}

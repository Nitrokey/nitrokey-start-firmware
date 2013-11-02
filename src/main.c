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

#include <stdint.h>
#include <string.h>
#include <chopstx.h>
#include <eventflag.h>

#include "config.h"

#include "sys.h"
#include "adc.h"
#include "gnuk.h"
#include "usb_lld.h"
#include "usb-cdc.h"
#include "random.h"
#include "stm32f103.h"

#ifdef DEBUG
#include "debug.h"

struct stdout stdout;

static void
stdout_init (void)
{
  chopstx_mutex_init (&stdout.m);
  chopstx_mutex_init (&stdout.m_dev);
  chopstx_cond_init (&stdout.cond_dev);
  stdout.connected = 0;
}

void
_write (const char *s, int len)
{
  int packet_len;

  if (len == 0)
    return;

  chopstx_mutex_lock (&stdout.m);

  chopstx_mutex_lock (&stdout.m_dev);
  if (!stdout.connected)
    chopstx_cond_wait (&stdout.cond_dev, &stdout.m_dev);
  chopstx_mutex_unlock (&stdout.m_dev);

  do
    {
      packet_len =
	(len < VIRTUAL_COM_PORT_DATA_SIZE) ? len : VIRTUAL_COM_PORT_DATA_SIZE;

      chopstx_mutex_lock (&stdout.m_dev);     
      usb_lld_write (ENDP3, s, packet_len);
      chopstx_cond_wait (&stdout.cond_dev, &stdout.m_dev);
      chopstx_mutex_unlock (&stdout.m_dev);

      s += packet_len;
      len -= packet_len;
    }
  /* Send a Zero-Length-Packet if the last packet is full size.  */
  while (len != 0 || packet_len == VIRTUAL_COM_PORT_DATA_SIZE);

  chopstx_mutex_unlock (&stdout.m);
}

void
EP3_IN_Callback (void)
{
  chopstx_mutex_lock (&stdout.m_dev);
  chopstx_cond_signal (&stdout.cond_dev);
  chopstx_mutex_unlock (&stdout.m_dev);
}

void
EP5_OUT_Callback (void)
{
  chopstx_mutex_lock (&stdout.m_dev);
  usb_lld_rx_enable (ENDP5);
  chopstx_mutex_unlock (&stdout.m_dev);
}
#else
void
_write (const char *s, int size)
{
  (void)s;
  (void)size;
}
#endif

extern void *USBthread (void *arg);

/*
 * main thread does 1-bit LED display output
 */
#define MAIN_TIMEOUT_INTERVAL	(5000*1000)

#define LED_TIMEOUT_INTERVAL	(75*1000)
#define LED_TIMEOUT_ZERO	(25*1000)
#define LED_TIMEOUT_ONE		(100*1000)
#define LED_TIMEOUT_STOP	(200*1000)


#define ID_OFFSET (2+SERIALNO_STR_LEN*2)
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
static struct eventflag led_event;

static void display_fatal_code (void)
{
  while (1)
    {
      set_led (1);
      chopstx_usec_wait (LED_TIMEOUT_ZERO);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_INTERVAL);
      set_led (1);
      chopstx_usec_wait (LED_TIMEOUT_ZERO);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_INTERVAL);
      set_led (1);
      chopstx_usec_wait (LED_TIMEOUT_ZERO);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_STOP);
      set_led (1);
      if (fatal_code & 1)
	chopstx_usec_wait (LED_TIMEOUT_ONE);
      else
	chopstx_usec_wait (LED_TIMEOUT_ZERO);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_INTERVAL);
      set_led (1);
      if (fatal_code & 2)
	chopstx_usec_wait (LED_TIMEOUT_ONE);
      else
	chopstx_usec_wait (LED_TIMEOUT_ZERO);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_INTERVAL);
      set_led (1);
      chopstx_usec_wait (LED_TIMEOUT_STOP);
      set_led (0);
      chopstx_usec_wait (LED_TIMEOUT_INTERVAL*10);
    }
}

static uint8_t led_inverted;

static eventmask_t emit_led (int on_time, int off_time)
{
  eventmask_t m;

  set_led (!led_inverted);
  m = eventflag_wait_timeout (&led_event, on_time);
  set_led (led_inverted);
  if (m) return m;
  if ((m = eventflag_wait_timeout (&led_event, off_time)))
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
	  if ((m = eventflag_wait_timeout (&led_event, LED_TIMEOUT_STOP * 2)))
	    return m;
	}
      else
	{
	  if ((m = eventflag_wait_timeout (&led_event, LED_TIMEOUT_INTERVAL)))
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
  eventflag_signal (&led_event, spec);
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

extern uint8_t __process1_stack_base__, __process1_stack_size__;
extern uint8_t __process4_stack_base__, __process4_stack_size__;

const uint32_t __stackaddr_ccid = (uint32_t)&__process1_stack_base__;
const size_t __stacksize_ccid = (size_t)&__process1_stack_size__;

const uint32_t __stackaddr_usb = (uint32_t)&__process4_stack_base__;
const size_t __stacksize_usb = (size_t)&__process4_stack_size__;

#define PRIO_CCID 3
#define PRIO_USB  4

extern void *usb_intr (void *arg);

static void gnuk_malloc_init (void);


static uint32_t bDeviceState;

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
  chopstx_t usb_thd;
  chopstx_t ccid_thd;

  (void)argc;
  (void)argv;

  gnuk_malloc_init ();

  flash_unlock ();
  device_initialize_once ();

  adc_init ();

  eventflag_init (&led_event, chopstx_main);

  random_init ();

#ifdef DEBUG
  stdout_init ();
#endif

  ccid_thd = chopstx_create (PRIO_CCID, __stackaddr_ccid,
			     __stacksize_ccid, USBthread, NULL);

#ifdef PINPAD_CIR_SUPPORT
  cir_init ();
#endif
#ifdef PINPAD_DND_SUPPORT
  msc_init ();
#endif

  usb_thd = chopstx_create (PRIO_USB, __stackaddr_usb, __stacksize_usb,
			    usb_intr, NULL);

  while (1)
    {
      if (bDeviceState != UNCONNECTED)
	break;

      chopstx_usec_wait (250*1000);
    }

  while (1)
    {
      eventmask_t m;

      if (icc_state_p != NULL && *icc_state_p == ICC_STATE_EXEC_REQUESTED)
	break;

      m = eventflag_wait_timeout (&led_event, MAIN_TIMEOUT_INTERVAL);
    got_it:
      count++;
      switch (m)
	{
	case LED_ONESHOT:
	  if ((m = emit_led (100*1000, MAIN_TIMEOUT_INTERVAL))) goto got_it;
	  break;
	case LED_TWOSHOTS:
	  if ((m = emit_led (50*1000, 50*1000))) goto got_it;
	  if ((m = emit_led (50*1000, MAIN_TIMEOUT_INTERVAL))) goto got_it;
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
	  m = eventflag_wait_timeout (&led_event, LED_TIMEOUT_STOP);
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
      if (stdout.connected && (count % 10) == 0)
	{
	  DEBUG_SHORT (count / 10);
	  _write ("\r\nThis is Gnuk on STM32F103.\r\n"
		  "Testing USB driver.\n\n"
		  "Hello world\r\n\r\n", 30+21+15);
	}
#endif
    }

  random_fini ();

  set_led (1);
  usb_lld_shutdown ();

  /* Finish application.  */
  chopstx_join (ccid_thd, NULL);

  chopstx_cancel (usb_thd);
  chopstx_join (usb_thd, NULL);

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
    void (*func) (void (*)(void)) = (void (*)(void (*)(void)))new_vector[9];

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
  eventflag_signal (&led_event, LED_FATAL);
  _write ("fatal\r\n", 7);
  for (;;);
}

/*
 * Malloc for Gnuk.
 *
 * Each memory chunk has header with size information.
 * The size of chunk is at least 16.
 *
 * Free memory is managed by FREE_LIST.
 *
 * When it is managed in FREE_LIST, three pointers, ->NEXT, ->PREV,
 * and ->NEIGHBOR is used.  NEXT and PREV is to implement doubly
 * linked list.  NEIGHBOR is to link adjacent memory chunk to be
 * reclaimed to system.
 */

extern uint8_t __heap_base__[];
extern uint8_t __heap_end__[];

#define MEMORY_END (__heap_end__)
#define MEMORY_ALIGNMENT 16
#define MEMORY_ALIGN(n) (((n) + MEMORY_ALIGNMENT - 1) & ~(MEMORY_ALIGNMENT - 1))

static uint8_t *heap_p;
static chopstx_mutex_t malloc_mtx;

struct mem_head {
  uint32_t size;
  /**/
  struct mem_head *next, *prev;	/* free list chain */
  struct mem_head *neighbor;	/* backlink to neighbor */
};

static struct mem_head *free_list;

static void
gnuk_malloc_init (void)
{
  chopstx_mutex_init (&malloc_mtx);
  heap_p = __heap_base__;
  free_list = NULL;
}

static void *
sbrk (size_t size)
{
  void *p = (void *)heap_p;

  if ((size_t)(MEMORY_END - heap_p) < size)
    return NULL;

  heap_p += size;
  return p;
}

static void
remove_from_free_list (struct mem_head *m)
{
  if (m->prev)
    m->prev->next = m->next;
  else
    free_list = m->next;
  if (m->next)
    m->next->prev = m->prev;
}


void *
gnuk_malloc (size_t size)
{
  struct mem_head *m;
  struct mem_head *m0;

  size = MEMORY_ALIGN (size + sizeof (uint32_t));

  chopstx_mutex_lock (&malloc_mtx);
  DEBUG_INFO ("malloc: ");
  DEBUG_SHORT (size);
  m = free_list;

  while (1)
    {
      if (m == NULL)
	{
	  m = (struct mem_head *)sbrk (size);
	  if (m)
	    m->size = size;
	  break;
	}

      if (m->size == size)
	{
	  remove_from_free_list (m);
	  m0 = free_list;
	  while (m0)
	    if (m0->neighbor == m)
	      m0->neighbor = NULL;
	    else
	      m0 = m0->next;
	  break;
	}

      m = m->next;
    }

  chopstx_mutex_unlock (&malloc_mtx);
  if (m == NULL)
    {
      DEBUG_WORD (0);
      return m;
    }
  else
    {
      DEBUG_WORD ((uint32_t)m + sizeof (uint32_t));
      return (void *)m + sizeof (uint32_t);
    }
}


void
gnuk_free (void *p)
{
  struct mem_head *m = (struct mem_head *)((void *)p - sizeof (uint32_t));
  struct mem_head *m0;

  chopstx_mutex_lock (&malloc_mtx);
  m0 = free_list;
  DEBUG_INFO ("free: ");
  DEBUG_SHORT (m->size);
  DEBUG_WORD ((uint32_t)p);

  m->neighbor = NULL;
  while (m0)
    {
      if ((void *)m + m->size == (void *)m0)
	m0->neighbor = m;
      else if ((void *)m0 + m0->size == (void *)m)
	m->neighbor = m0;

      m0 = m0->next;
    }

  if ((void *)m + m->size == heap_p)
    {
      struct mem_head *mn = m->neighbor;

      heap_p -= m->size;
      while (mn)
	{
	  heap_p -= mn->size;
	  remove_from_free_list (mn);
	  mn = mn->neighbor;
	}
    }
  else
    {
      m->next = free_list;
      m->prev = NULL;
      if (free_list)
	free_list->prev = m;
      free_list = m;
    }

  chopstx_mutex_unlock (&malloc_mtx);
}

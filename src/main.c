/*
 * main.c - main routine of Gnuk
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2015, 2016, 2017, 2018, 2021
 *               Free Software Initiative of Japan
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
#ifdef GNU_LINUX_EMULATION
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <stdio.h>
#include <stdlib.h>
#define main emulated_main
#else
#include "mcu/stm32f103.h"
#endif

/*
 * main thread does 1-bit LED display output
 */
#define LED_TIMEOUT_INTERVAL	(75*1000)
#define LED_TIMEOUT_ZERO	(25*1000)
#define LED_TIMEOUT_ONE		(100*1000)
#define LED_TIMEOUT_STOP	(200*1000)

#ifdef DFU_SUPPORT
static int
flash_write_any (uintptr_t dst_addr, const uint8_t *src, size_t len)
{
  int status;

  while (len)
    {
      uint16_t hw = *src++;

      hw |= (*src++ << 8);
      status = flash_program_halfword (dst_addr, hw);
      if (status != 0)
        return 0;              /* error return */

      dst_addr += 2;
      len -= 2;
    }

  return 1;
}
#endif

#ifdef GNU_LINUX_EMULATION
uint8_t *flash_addr_key_storage_start;
uint8_t *flash_addr_data_storage_start;
#else
#define ID_OFFSET (2+SERIALNO_STR_LEN*2)
static void
device_initialize_once (void)
{
  const uint8_t *p = &gnuk_string_serial[ID_OFFSET];

  if (p[0] == 0xff && p[1] == 0xff && p[2] == 0xff && p[3] == 0xff)
    {
      /*
       * This is the first time invocation.
       * Setup serial number by unique device ID.
       */
      const uint8_t *u = unique_device_id () + (MHZ < 96 ? 8: 0);
      int i;

      for (i = 0; i < 4; i++)
	{
	  uint8_t b = u[3-i];
	  uint8_t nibble;

	  nibble = (b >> 4);
	  nibble += (nibble >= 10 ? ('A' - 10) : '0');
	  flash_put_data_internal (&p[i*4], nibble);
	  nibble = (b & 0x0f);
	  nibble += (nibble >= 10 ? ('A' - 10) : '0');
	  flash_put_data_internal (&p[i*4+2], nibble);
	}

#ifdef DFU_SUPPORT
#define CHIP_ID_REG ((uint32_t *)0xE0042000)
      /*
       * Overwrite DFU bootloader with a copy of SYS linked to ORIGIN_REAL.
       * Then protect flash from readout.
       */
      {
        extern uint8_t _binary_build_stdaln_sys_bin_start;
        extern uint8_t _binary_build_stdaln_sys_bin_size;
        size_t stdaln_sys_size = (size_t) &_binary_build_stdaln_sys_bin_size;
        extern const uint32_t FT0[256], FT1[256], FT2[256];
        extern handler vector_table[];
        uintptr_t addr;
        uint32_t flash_page_size = 1024; /* 1KiB default */

        if (((*CHIP_ID_REG)&0x07) == 0x04) /* High density device.  */
          flash_page_size = 2048; /* It's 2KiB. */

        /* Kill DFU */
        for (addr = ORIGIN_REAL; addr < ORIGIN;
             addr += flash_page_size)
          flash_erase_page (addr);

        /* Copy SYS */
        addr = ORIGIN_REAL;
        flash_write_any(addr, &_binary_build_stdaln_sys_bin_start,
                        stdaln_sys_size);
        addr += stdaln_sys_size;
        flash_write_any(addr, (const uint8_t *) &FT0, sizeof(FT0));
        addr += sizeof(FT0);
        flash_write_any(addr, (const uint8_t *) &FT1, sizeof(FT1));
        addr += sizeof(FT1);
        flash_write_any(addr, (const uint8_t *) &FT2, sizeof(FT2));

        addr = ORIGIN_REAL + 0x1000;
        if (addr < ORIGIN) {
          /* Need to patch top of stack and reset vector there */
          handler *new_vector = (handler *) addr;
          flash_write((uintptr_t) &new_vector[0], (const uint8_t *)
                      &vector_table[0], sizeof(handler));
          flash_write((uintptr_t) &new_vector[1], (const uint8_t *)
                      &vector[1], sizeof(handler));
        }

        flash_protect();
        nvic_system_reset();
      }
#endif
    }
}
#endif


static volatile uint8_t fatal_code;
static struct eventflag led_event;
static chopstx_poll_cond_t led_event_poll_desc;
static struct chx_poll_head *const led_event_poll[] = {
  (struct chx_poll_head *)&led_event_poll_desc
};

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

static void
emit_led (uint32_t on_time, uint32_t off_time)
{
  set_led (!led_inverted);
  chopstx_poll (&on_time, 1, led_event_poll);
  set_led (led_inverted);
  chopstx_poll (&off_time, 1, led_event_poll);
}

static void
display_status_code (void)
{
  enum ccid_state ccid_state = ccid_get_ccid_state ();
  uint32_t usec;

  if (ccid_state == CCID_STATE_START)
    emit_led (LED_TIMEOUT_ONE, LED_TIMEOUT_STOP);
  else
    /* OpenPGP card thread is running */
    {
      emit_led ((auth_status & AC_ADMIN_AUTHORIZED)?
		LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO, LED_TIMEOUT_INTERVAL);
      emit_led ((auth_status & AC_OTHER_AUTHORIZED)?
		LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO, LED_TIMEOUT_INTERVAL);
      emit_led ((auth_status & AC_PSO_CDS_AUTHORIZED)?
		LED_TIMEOUT_ONE : LED_TIMEOUT_ZERO, LED_TIMEOUT_INTERVAL);

      if (ccid_state == CCID_STATE_WAIT)
	{
	  usec = LED_TIMEOUT_STOP * 2;
	  chopstx_poll (&usec, 1, led_event_poll);
	}
      else
	{
	  usec = LED_TIMEOUT_INTERVAL;
	  chopstx_poll (&usec, 1, led_event_poll);
	  emit_led (LED_TIMEOUT_ZERO, LED_TIMEOUT_STOP);
	}
    }
}

void
led_blink (int spec)
{
  if (spec == LED_START_COMMAND || spec == LED_FINISH_COMMAND)
    {
      led_inverted = (spec == LED_START_COMMAND);
      spec = LED_SYNC;
    }

  eventflag_signal (&led_event, spec);
}

#ifdef FLASH_UPGRADE_SUPPORT
/*
 * In Gnuk 1.0.[12], reGNUal was not relocatable.
 * Now, it's relocatable, but we need to calculate its entry address
 * based on it's pre-defined address.
 */
#define REGNUAL_START_ADDRESS_COMPATIBLE 0x20001400
static uintptr_t
calculate_regnual_entry_address (const uint8_t *addr)
{
  const uint8_t *p = addr + 4;
  uintptr_t v = p[0] + (p[1] << 8) + (p[2] << 16) + (p[3] << 24);

  v -= REGNUAL_START_ADDRESS_COMPATIBLE;
  v += (uintptr_t)addr;
  return v;
}
#endif

#define STACK_MAIN
#define STACK_PROCESS_1
#include "stack-def.h"
#define STACK_ADDR_CCID ((uintptr_t)process1_base)
#define STACK_SIZE_CCID (sizeof process1_base)

#define PRIO_CCID 3
#define PRIO_MAIN 5

extern void *ccid_thread (void *arg);

static void gnuk_malloc_init (void);


extern uint32_t bDeviceState;

/*
 * Entry point.
 */
int
main (int argc, const char *argv[])
{
#ifdef GNU_LINUX_EMULATION
  uintptr_t flash_addr;
  const char *flash_image_path;
  char *path_string = NULL;
#endif
#ifdef FLASH_UPGRADE_SUPPORT
  uintptr_t entry;
#endif
  chopstx_t ccid_thd;
  int wait_for_ack = 0;

  chopstx_conf_idle (1);

  gnuk_malloc_init ();

#ifdef GNU_LINUX_EMULATION
#define FLASH_IMAGE_NAME ".gnuk-flash-image"

  if (argc >= 4 || (argc == 2 && !strcmp (argv[1], "--help")))
    {
      fprintf (stdout, "Usage: %s [--vidpid=Vxxx:Pxxx] [flash-image-file]",
	       argv[0]);
      exit (0);
    }

  if (argc >= 2 && !strncmp (argv[1], "--debug=", 8))
    {
      debug = strtol (&argv[1][8], NULL, 10);
      argc--;
      argv++;
    }

  if (argc >= 2 && !strncmp (argv[1], "--vidpid=", 9))
    {
      extern uint8_t device_desc[];
      uint32_t id;
      char *p;

      id = (uint32_t)strtol (&argv[1][9], &p, 16);
      device_desc[8] = (id & 0xff);
      device_desc[9] = (id >> 8);

      if (p && p[0] == ':')
	{
	  id = (uint32_t)strtol (&p[1], NULL, 16);
	  device_desc[10] = (id & 0xff);
	  device_desc[11] = (id >> 8);
	}

      argc--;
      argv++;
    }

  if (argc == 1)
    {
      char *p = getenv ("HOME");

      if (p == NULL)
	{
	  fprintf (stderr, "Can't find $HOME\n");
	  exit (1);
	}

      path_string = malloc (strlen (p) + strlen (FLASH_IMAGE_NAME) + 2);

      p = stpcpy (path_string, p);
      *p++ = '/';
      strcpy (p, FLASH_IMAGE_NAME);
      flash_image_path = path_string;
    }
  else
    flash_image_path = argv[1];

  if (access (flash_image_path, F_OK) < 0)
    {
      int fd;
      char buf[8192];

      memset (buf, 0xff, sizeof buf);
      memset (buf+4*1024, 0, 2);
      fd = open (flash_image_path, O_CREAT|O_WRONLY, S_IRUSR|S_IWUSR);
      if (fd < 0)
	{
	  perror ("creating flash file");
	  exit (1);
	}

      if (write (fd, buf, sizeof buf) != sizeof buf)
	{
	  perror ("initializing flash file");
	  close (fd);
	  exit (1);
	}

      close (fd);
    }

  puts ("Gnuk (emulation with USBIP), a GnuPG USB Token implementation");
  puts ("Copyright (C) 2021 Free Software Initiative of Japan");
  puts ("This is free software under GPLv3+.");

  flash_addr = flash_init (flash_image_path);
  flash_addr_key_storage_start = (uint8_t *)flash_addr;
  flash_addr_data_storage_start = (uint8_t *)flash_addr + 4096;
#else
  (void)argc;
  (void)argv;
#endif

  flash_unlock ();

#ifdef GNU_LINUX_EMULATION
    if (path_string)
      free (path_string);
#else
  device_initialize_once ();
#endif

  adc_init ();

  eventflag_init (&led_event);

  random_init ();

#ifdef DEBUG
  stdout_init ();
#endif

  ccid_thd = chopstx_create (PRIO_CCID, STACK_ADDR_CCID, STACK_SIZE_CCID,
			     ccid_thread, NULL);

#ifdef PINPAD_CIR_SUPPORT
  cir_init ();
#endif
#ifdef PINPAD_DND_SUPPORT
  msc_init ();
#endif

  chopstx_setpriority (PRIO_MAIN);

  while (1)
    {
      if (bDeviceState != USB_DEVICE_STATE_UNCONNECTED)
	break;

      chopstx_usec_wait (250*1000);
    }

  eventflag_prepare_poll (&led_event, &led_event_poll_desc);

  while (1)
    {
      eventmask_t m;

      if (wait_for_ack)
	m = eventflag_wait_timeout (&led_event, LED_TIMEOUT_INTERVAL);
      else
	m = eventflag_wait (&led_event);

      switch (m)
	{
	case LED_ONESHOT:
	  emit_led (100*1000, LED_TIMEOUT_STOP);
	  break;
	case LED_TWOSHOTS:
	  emit_led (50*1000, 50*1000);
	  emit_led (50*1000, LED_TIMEOUT_STOP);
	  break;
	case LED_SHOW_STATUS:
	  display_status_code ();
	  break;
	case LED_FATAL:
	  display_fatal_code ();
	  break;
	case LED_SYNC:
	  set_led (led_inverted);
	  break;
	case LED_GNUK_EXEC:
	  goto exec;
	case LED_WAIT_FOR_BUTTON:
	  wait_for_ack ^= 1;
	  /* fall through */
	default:
	  emit_led (LED_TIMEOUT_ZERO, LED_TIMEOUT_ZERO);
	  break;
	}
    }

 exec:
  random_fini ();

  set_led (1);
  usb_lld_shutdown ();

  /* Finish application.  */
  chopstx_join (ccid_thd, NULL);

#ifdef FLASH_UPGRADE_SUPPORT
  /* Set vector */
  SCB->VTOR = (uintptr_t)&_regnual_start;
  entry = calculate_regnual_entry_address (&_regnual_start);
#ifdef DFU_SUPPORT
  {
    /* Use SYS at ORIGIN_REAL instead of the one at ORIGIN */
    handler *new_vector = (handler *)ORIGIN_REAL;
    void (*func) (void (*)(void)) = (void (*)(void (*)(void))) new_vector[9];

    /* Leave Gnuk to exec reGNUal */
    (*func) ((void (*)(void))entry);
    for (;;);
  }
#else
  /* Leave Gnuk to exec reGNUal */
  flash_erase_all_and_exec ((void (*)(void))entry);
#endif
#else
  exit (0);
#endif

  /* Never reached */
  return 0;
}

void
fatal (uint8_t code)
{
  extern void _write (const char *s, int len);

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

#ifdef GNU_LINUX_EMULATION
#define HEAP_SIZE (32*1024)
uint8_t __heap_base__[HEAP_SIZE];

#define HEAP_START __heap_base__
#define HEAP_END (__heap_base__ + HEAP_SIZE)
#define HEAP_ALIGNMENT 32
#else
extern uint8_t __heap_base__[];
extern uint8_t __heap_end__[];

#define HEAP_START __heap_base__
#define HEAP_END (__heap_end__)
#define HEAP_ALIGNMENT 16
#define HEAP_SIZE ((uintptr_t)__heap_end__ -  (uintptr_t)__heap_base__)
#endif

#define HEAP_ALIGN(n) (((n) + HEAP_ALIGNMENT - 1) & ~(HEAP_ALIGNMENT - 1))

static uint8_t *heap_p;
static chopstx_mutex_t malloc_mtx;

struct mem_head {
  uintptr_t size;
  /**/
  struct mem_head *next, *prev;	/* free list chain */
  struct mem_head *neighbor;	/* backlink to neighbor */
};

#define MEM_HEAD_IS_CORRUPT(x) \
    ((x)->size != HEAP_ALIGN((x)->size) || (x)->size > HEAP_SIZE)
#define MEM_HEAD_CHECK(x) if (MEM_HEAD_IS_CORRUPT(x)) fatal (FATAL_HEAP)

static struct mem_head *free_list;

static void
gnuk_malloc_init (void)
{
  chopstx_mutex_init (&malloc_mtx);
  heap_p = HEAP_START;
  free_list = NULL;
}

static void *
gnuk_sbrk (intptr_t size)
{
  void *p = (void *)heap_p;

  if ((HEAP_END - heap_p) < size)
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

  size = HEAP_ALIGN (size + sizeof (uintptr_t));

  chopstx_mutex_lock (&malloc_mtx);
  DEBUG_INFO ("malloc: ");
  DEBUG_SHORT (size);
  m = free_list;

  while (1)
    {
      if (m == NULL)
	{
	  m = (struct mem_head *)gnuk_sbrk (size);
	  if (m)
	    m->size = size;
	  break;
	}
      MEM_HEAD_CHECK (m);
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
      DEBUG_WORD ((uintptr_t)m + sizeof (uintptr_t));
      return (void *)m + sizeof (uintptr_t);
    }
}


void
gnuk_free (void *p)
{
  struct mem_head *m = (struct mem_head *)((void *)p - sizeof (uintptr_t));
  struct mem_head *m0;

  if (p == NULL)
    return;

  chopstx_mutex_lock (&malloc_mtx);
  m0 = free_list;
  DEBUG_INFO ("free: ");
  DEBUG_SHORT (m->size);
  DEBUG_WORD ((uintptr_t)p);

  MEM_HEAD_CHECK (m);
  m->neighbor = NULL;
  while (m0)
    {
      MEM_HEAD_CHECK (m0);
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
	  MEM_HEAD_CHECK (mn);
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

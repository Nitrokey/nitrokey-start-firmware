/*
    ChibiOS/RT - Copyright (C) 2006,2007,2008,2009,2010 Giovanni Di Sirio.

    This file is part of ChibiOS/RT.

    ChibiOS/RT is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    ChibiOS/RT is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.

                                      ---

    A special exception to the GPL can be applied should you wish to distribute
    a combined work that includes ChibiOS/RT, without being obliged to provide
    the source code for any proprietary components. See the file exception.txt
    for full details of how and when the exception can be applied.
*/

#include "ch.h"
#include "hal.h"
#include "test.h"
#include "usb_lld.h"

#include "usb_lib.h"
#include "usb_istr.h"
#include "usb_desc.h"
#include "hw_config.h"
#include "usb_pwr.h"

/*
 * Red LEDs blinker thread, times are in milliseconds.
 */
static WORKING_AREA(waThread1, 128);
static msg_t Thread1(void *arg) {

  (void)arg;
  while (TRUE) {
    palClearPad (IOPORT3, GPIOC_LED);
    chThdSleepMilliseconds (500);
    palSetPad (IOPORT3, GPIOC_LED);
    chThdSleepMilliseconds (500);
  }
  return 0;
}

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

static int
_write (const char *s, int size)
{
  chMtxLock (&stdout.m);
  if (stdout.str)
    chCondWait (&stdout.finish_cnd);
  stdout.str = s;
  stdout.size = size;
  chCondSignal (&stdout.start_cnd);
  chCondWait (&stdout.finish_cnd);
  chMtxUnlock ();
  return 0;
}

extern uint32_t count_in;
extern __IO uint32_t count_out;
extern uint8_t buffer_in[VIRTUAL_COM_PORT_DATA_SIZE];
extern uint8_t buffer_out[VIRTUAL_COM_PORT_DATA_SIZE];
extern void USB_Init (void);


static WORKING_AREA(waThread2, 128);
static msg_t Thread2 (void *arg)
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
      if (bDeviceState != CONFIGURED)
	break;

      chMtxLock (&stdout.m);
      if (stdout.str == NULL)
	chCondWait (&stdout.start_cnd);

      {
	/* assume DATA_SIZE is large enough */
	int i;

	for (i = 0; i < stdout.size; i++)
	  buffer_in[i] = stdout.str[i];
	count_in = stdout.size;
	USB_SIL_Write (EP1_IN, buffer_in, count_in);
	SetEPTxValid (ENDP1);
      }

      stdout.str = NULL;
      stdout.size = 0;
      chCondSignal (&stdout.finish_cnd);
      chMtxUnlock ();
    }

  goto again;
  return 0;
}

/*
 * Entry point, note, the main() function is already a thread in the system
 * on entry.
 */
int main(int argc, char **argv)
{
  int count = 0;

  (void)argc;
  (void)argv;

  usb_lld_init ();
  USB_Init();

  stdout_init ();
  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic (waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  /*
   * Creates 'stdout' thread.
   */
  chThdCreateStatic (waThread2, sizeof(waThread2), NORMALPRIO, Thread2, NULL);

  while (1)
    {
      if (palReadPad(IOPORT1, GPIOA_BUTTON))
	palSetPad (IOPORT3, GPIOC_LED);

      chThdSleepMilliseconds (100);

      if (bDeviceState == CONFIGURED && (count % 10) == 0)
	_write ("Hello world\r\n", 13);

      count++;
    }

  return 0;
}

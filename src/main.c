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

extern uint32_t count_in;
extern __IO uint32_t count_out;
extern uint8_t buffer_in[VIRTUAL_COM_PORT_DATA_SIZE];
extern uint8_t buffer_out[VIRTUAL_COM_PORT_DATA_SIZE];
extern void USB_Init (void);

/*
 * Entry point, note, the main() function is already a thread in the system
 * on entry.
 */
int main(int argc, char **argv) {

  (void)argc;
  (void)argv;

  usb_lld_init ();
  USB_Init();

  /*
   * Creates the blinker thread.
   */
  chThdCreateStatic (waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

  while (TRUE) {
    if (palReadPad(IOPORT1, GPIOA_BUTTON))
      palSetPad (IOPORT3, GPIOC_LED);
  
    if ((count_out != 0) && (bDeviceState == CONFIGURED)) {
      uint8_t i;

      for (i = 0; i<count_out; i++) {
	buffer_in[(count_in+i)%64] = buffer_out[i];
      }
      count_in += count_out;
      count_out = 0;
    }

    if (count_in > 0) {
      USB_SIL_Write (EP1_IN, buffer_in, count_in);
      SetEPTxValid (ENDP1);
    }

    chThdSleepMilliseconds (50);
  }
  return 0;
}

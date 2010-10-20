#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

void
hwinit0(void)
{
  hwinit0_common ();
}

void
hwinit1(void)
{
  hwinit1_common ();

  /*
   * Clear LED and SHUTDOWN output.
   */
  palClearPad (IOPORT5, GPIOE_LED);
  palClearPad (IOPORT3, GPIOC_SHUTDOWN);
}

void
USB_Cable_Config (FunctionalState NewState)
{
  if (NewState != DISABLE)
    palClearPad (IOPORT4, GPIOD_DISC);
  else
    palSetPad (IOPORT4, GPIOD_DISC);
}

void
set_led (int value)
{
  if (value)
    palClearPad (IOPORT5, GPIOE_LEDR);
  else
    palSetPad (IOPORT5, GPIOE_LEDR);
}

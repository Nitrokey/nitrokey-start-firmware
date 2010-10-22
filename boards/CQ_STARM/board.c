#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

void
hwinit0 (void)
{
  hwinit0_common ();
}

void
hwinit1 (void)
{
  hwinit1_common ();
}

void
USB_Cable_Config (FunctionalState NewState)
{
  /* CQ STARM has no functionality to stop USB.  */
  /*
   * It seems that users can add the functionality with USB_DC (PD9)
   * though
   */
  (void)NewState;
}

void
set_led (int value)
{
  if (value)
    palSetPad (IOPORT3, GPIOC_LED);
  else
    palClearPad (IOPORT3, GPIOC_LED);
}

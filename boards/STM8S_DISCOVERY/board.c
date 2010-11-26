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
  /* No functionality to stop USB.  */
  (void)NewState;
}

void
set_led (int value)
{
  if (value)
    palSetPad (IOPORT1, GPIOA_LED);
  else
    palClearPad (IOPORT1, GPIOA_LED);
}

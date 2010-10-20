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
  if (NewState != DISABLE)
    palClearPad (IOPORT3, GPIOC_DISC);
  else
    palSetPad (IOPORT3, GPIOC_DISC);
}

void
set_led (int value)
{
  if (value)
    palClearPad (IOPORT3, GPIOC_LED);
  else
    palSetPad (IOPORT3, GPIOC_LED);
}

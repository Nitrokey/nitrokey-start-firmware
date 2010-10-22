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
  /*
   * Disable JTAG and SWD, done after hwinit1_common as HAL resets AFIO
   */
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_DISABLE;
  /* We use LED2 as optional "error" indicator */
  palSetPad (IOPORT1, GPIOA_LED2);
}

void
USB_Cable_Config (FunctionalState NewState)
{
  if (NewState != DISABLE)
    palSetPad (IOPORT1, GPIOA_USB_ENABLE);
  else
    palClearPad (IOPORT1, GPIOA_USB_ENABLE);
}

void
set_led (int value)
{
  if (value)
    palClearPad (IOPORT1, GPIOA_LED1);
  else
    palSetPad (IOPORT1, GPIOA_LED1);
}

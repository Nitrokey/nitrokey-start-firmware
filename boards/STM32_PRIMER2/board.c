#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

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

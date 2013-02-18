#include "config.h"
#include "ch.h"
#include "hal.h"

/*
 * Board-specific initialization code.
 */
void boardInit(void)
{
  /*
   * Clear LED and SHUTDOWN output.
   */
  palClearPad (IOPORT5, GPIOE_LED);
  palClearPad (IOPORT3, GPIOC_SHUTDOWN);
}

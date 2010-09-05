#include "config.h"
#include "ch.h"
#include "hal.h"
#include "gnuk.h"

uint32_t
hardclock (void)
{
  uint32_t r = SysTick->VAL;

  DEBUG_INFO ("Random: ");
  DEBUG_WORD (r);
  return r;
}

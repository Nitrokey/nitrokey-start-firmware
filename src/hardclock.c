#include "ch.h"
#include "hal.h"

uint32_t
hardclock (void)
{
  return SysTick->VAL;
}

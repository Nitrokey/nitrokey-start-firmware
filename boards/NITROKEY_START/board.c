#include "config.h"
#include "ch.h"
#include "hal.h"

#include "../common/hwinit.c"

void
hwinit1 (void)
{
  hwinit1_common ();
  AFIO->MAPR |= AFIO_MAPR_SWJ_CFG_JTAGDISABLE;
}

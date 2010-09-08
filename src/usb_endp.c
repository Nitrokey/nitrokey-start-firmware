/*
 * Virtual COM port (for debug output only)
 */

#include "usb_lib.h"

#include "config.h"
#include "ch.h"
#include "gnuk.h"

void
EP3_IN_Callback(void)
{
  if (stdout_thread)
    chEvtSignalI (stdout_thread, EV_TX_READY);
}

void
EP5_OUT_Callback(void)
{
  SetEPRxValid (ENDP3);
}

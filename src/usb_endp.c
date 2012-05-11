/*
 * Virtual COM port (for debug output only)
 */

#include "config.h"
#include "ch.h"
#include "gnuk.h"
#include "usb_lld.h"

void
EP3_IN_Callback (void)
{
  if (stdout_thread)
    chEvtSignalI (stdout_thread, EV_TX_READY);
}

void
EP5_OUT_Callback (void)
{
  usb_lld_rx_enable (ENDP5);
}

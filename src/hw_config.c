#include "ch.h"
#include "hal.h"

#include "usb_lib.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "hw_config.h"
#include "platform_config.h"
#include "usb_pwr.h"

void
Enter_LowPowerMode (void)
{
  bDeviceState = SUSPENDED;
}

void
Leave_LowPowerMode (void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  if (pInfo->Current_Configuration != 0)
    bDeviceState = CONFIGURED;
  else
    bDeviceState = ATTACHED;
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
Get_SerialNum (void)
{
}

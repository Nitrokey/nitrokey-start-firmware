/* Hardware specific USB functions */
/*
 * For detail, please see the documentation of 
 * STM32F10x USB Full Speed Device Library (USB-FS-Device_Lib)
 * by STMicroelectronics' MCD Application Team
 */

#include "ch.h"
#include "hal.h"
#include "board.h"
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

const uint8_t *
unique_device_id (void)
{
  /* STM32F103 has 96-bit unique device identifier */
  const uint8_t *addr = (const uint8_t *)0x1ffff7e8;

  return addr;
}

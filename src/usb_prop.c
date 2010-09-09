/*
 * usb_prop.c - glue/interface code between Gnuk and USB-FS-Device_Lib
 *
 * Copyright (C) 2010 Free Software Initiative of Japan
 * Author: NIIBE Yutaka <gniibe@fsij.org>
 *
 * This file is a part of Gnuk, a GnuPG USB Token implementation.
 *
 * Gnuk is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Gnuk is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
 * License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* Packet size of USB Bulk transfer for full speed */
#define GNUK_MAX_PACKET_SIZE 64

#include "config.h"
#include "usb_lib.h"
#include "usb_conf.h"
#include "usb_prop.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "hw_config.h"

#ifdef ENABLE_VIRTUAL_COM_PORT
#include "usb-cdc-vport.c"
#endif

static void
gnuk_device_init (void)
{
  /*
   * Update the serial number string descriptor (if needed)
   */
  Get_SerialNum ();

  pInformation->Current_Configuration = 0;

  /* Connect the device */
  PowerOn ();

  /* Perform basic device initialization operations */
  USB_SIL_Init ();

  bDeviceState = UNCONNECTED;
}

static void
gnuk_device_reset (void)
{
  /* Set DEVICE as not configured */
  pInformation->Current_Configuration = 0;

  /* Current Feature initialization */
  pInformation->Current_Feature = Config_Descriptor.Descriptor[7];

  /* Set DEVICE with the default Interface*/
  pInformation->Current_Interface = 0;

  SetBTABLE (BTABLE_ADDRESS);

  /* Initialize Endpoint 0 */
  SetEPType (ENDP0, EP_CONTROL);
  SetEPTxStatus (ENDP0, EP_TX_STALL);
  SetEPRxAddr (ENDP0, ENDP0_RXADDR);
  SetEPTxAddr (ENDP0, ENDP0_TXADDR);
  Clear_Status_Out (ENDP0);
  SetEPRxCount (ENDP0, GNUK_MAX_PACKET_SIZE);
  SetEPRxValid (ENDP0);

  /* Initialize Endpoint 1 */
  SetEPType (ENDP1, EP_BULK);
  SetEPTxAddr (ENDP1, ENDP1_TXADDR);
  SetEPTxStatus (ENDP1, EP_TX_NAK);
  SetEPRxStatus (ENDP1, EP_RX_DIS);

  /* Initialize Endpoint 2 */
  SetEPType (ENDP2, EP_BULK);
  SetEPRxAddr (ENDP2, ENDP2_RXADDR);
  SetEPRxCount (ENDP2, GNUK_MAX_PACKET_SIZE);
  SetEPRxStatus (ENDP2, EP_RX_VALID);
  SetEPTxStatus (ENDP2, EP_TX_DIS);

#ifdef ENABLE_VIRTUAL_COM_PORT
  /* Initialize Endpoint 3 */
  SetEPType (ENDP3, EP_BULK);
  SetEPTxAddr (ENDP3, ENDP3_TXADDR);
  SetEPTxStatus (ENDP3, EP_TX_NAK);
  SetEPRxStatus (ENDP3, EP_RX_DIS);

  /* Initialize Endpoint 4 */
  SetEPType (ENDP4, EP_INTERRUPT);
  SetEPTxAddr (ENDP4, ENDP4_TXADDR);
  SetEPTxStatus (ENDP4, EP_TX_NAK);
  SetEPRxStatus (ENDP4, EP_RX_DIS);

  /* Initialize Endpoint 5 */
  SetEPType (ENDP5, EP_BULK);
  SetEPRxAddr (ENDP5, ENDP5_RXADDR);
  SetEPRxCount (ENDP5, VIRTUAL_COM_PORT_DATA_SIZE);
  SetEPRxStatus (ENDP5, EP_RX_VALID);
  SetEPTxStatus (ENDP5, EP_TX_DIS);
#endif

  /* Set this device to response on default address */
  SetDeviceAddress (0);

  bDeviceState = ATTACHED;
}

static void
gnuk_device_SetConfiguration (void)
{
  DEVICE_INFO *pInfo = &Device_Info;

  if (pInfo->Current_Configuration != 0)
    /* Device configured */
    bDeviceState = CONFIGURED;
}

static void
gnuk_device_SetDeviceAddress (void)
{
  bDeviceState = ADDRESSED;
}

/* IN from port 0 */
static void
gnuk_device_Status_In (void)
{
}

/* OUT to port 0 */
static void
gnuk_device_Status_Out (void)
{
}

static uint8_t *
gnuk_device_GetDeviceDescriptor (uint16_t Length)
{
  return Standard_GetDescriptorData (Length,
				     (PONE_DESCRIPTOR)&Device_Descriptor);
}

static uint8_t *
gnuk_device_GetConfigDescriptor (uint16_t Length)
{
  return Standard_GetDescriptorData (Length,
				     (PONE_DESCRIPTOR)&Config_Descriptor);
}

static uint8_t *
gnuk_device_GetStringDescriptor (uint16_t Length)
{
  uint8_t wValue0 = pInformation->USBwValue0;

  if (wValue0 > (sizeof (String_Descriptor) / sizeof (ONE_DESCRIPTOR)))
    return NULL;
  else
    return Standard_GetDescriptorData (Length,
				       (PONE_DESCRIPTOR)&String_Descriptor[wValue0]);
}

static RESULT
gnuk_device_Get_Interface_Setting (uint8_t Interface, uint8_t AlternateSetting)
{
  if (AlternateSetting > 0)
    return USB_UNSUPPORT;
  else if (Interface > 1)
    return USB_UNSUPPORT;

  return USB_SUCCESS;
}

#if !defined(ENABLE_VIRTUAL_COM_PORT)
static RESULT
gnuk_nothing_todo (uint8_t RequestNo)
{
  (void)RequestNo;
  return USB_UNSUPPORT;
}
#endif

/*
 * Interface to USB core
 */

const DEVICE_PROP Device_Property = {
  gnuk_device_init,
  gnuk_device_reset,
  gnuk_device_Status_In,
  gnuk_device_Status_Out,
#ifdef ENABLE_VIRTUAL_COM_PORT
  Virtual_Com_Port_Data_Setup,
  Virtual_Com_Port_NoData_Setup,
#else
  gnuk_nothing_todo,
  gnuk_nothing_todo,
#endif
  gnuk_device_Get_Interface_Setting,
  gnuk_device_GetDeviceDescriptor,
  gnuk_device_GetConfigDescriptor,
  gnuk_device_GetStringDescriptor,
  0,
  GNUK_MAX_PACKET_SIZE
};

const DEVICE Device_Table = {
  EP_NUM,
  1
};

const USER_STANDARD_REQUESTS User_Standard_Requests = {
  NOP_Process,			/* GetConfiguration */ 
  gnuk_device_SetConfiguration,
  NOP_Process,			/* GetInterface */
  NOP_Process,			/* SetInterface */
  NOP_Process,			/* GetStatus */
  NOP_Process,			/* ClearFeature */
  NOP_Process,			/* SetEndPointFeature */
  NOP_Process,			/* SetDeviceFeature */
  gnuk_device_SetDeviceAddress
};

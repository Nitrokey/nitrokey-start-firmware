/*
 * usb_prop.c - glue/interface code between Gnuk and USB-FS-Device_Lib
 *
 * Copyright (C) 2010, 2011 Free Software Initiative of Japan
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

static uint8_t gnukStringSerial[] = {
  13*2+2,			/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,	/* bDescriptorType */
  '0', 0, '.', 0, '1', 0, '1', 0, /* Version number of Gnuk */
  '-', 0,
  0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0,
};
#define ID_OFFSET 12

static void
gnuk_device_init (void)
{
  const uint8_t *u = unique_device_id ();
  int i;

  for (i = 0; i < 4; i++)
    {
      gnukStringSerial[i*4+ID_OFFSET+0] = (u[i*2] >> 4) + 'A';
      gnukStringSerial[i*4+ID_OFFSET+1] = 0;
      gnukStringSerial[i*4+ID_OFFSET+2] = (u[i*2+1] & 0x0f) + 'A';
      gnukStringSerial[i*4+ID_OFFSET+3] = 0;
    }

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
gnuk_device_SetInterface (void)
{
  uint16_t intf = pInformation->USBwIndex0;
  
  /* alternateSetting: pInformation->USBwValue0 should be 0 */

  if (intf == 0)
    {
      ClearDTOG_RX (ENDP2);
      ClearDTOG_TX (ENDP1);
    }
#ifdef ENABLE_VIRTUAL_COM_PORT
  else if (intf == 1)
    {
      ClearDTOG_TX (ENDP4);
    }
  else if (intf == 2)
    {
      ClearDTOG_RX (ENDP5);
      ClearDTOG_TX (ENDP3);
    }
#endif
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
  uint32_t  wOffset = pInformation->Ctrl_Info.Usb_wOffset;

  if (wValue0 == 3)
    /* Serial number is requested */
    if (Length == 0)
      {
	pInformation->Ctrl_Info.Usb_wLength = sizeof gnukStringSerial - wOffset;
	return 0;
      }
    else
      return gnukStringSerial + wOffset;
  else if (wValue0 > (sizeof (String_Descriptor) / sizeof (ONE_DESCRIPTOR)))
    return NULL;
  else
    return Standard_GetDescriptorData (Length,
				       (PONE_DESCRIPTOR)&String_Descriptor[wValue0]);
}

#ifdef ENABLE_VIRTUAL_COM_PORT
#define NUM_INTERFACES 3	/* two for CDC, one for CCID */
#else
#define NUM_INTERFACES 1	/* CCID only */
#endif

static RESULT
gnuk_device_Get_Interface_Setting (uint8_t Interface, uint8_t AlternateSetting)
{
  if (AlternateSetting > 0)	/* Any interface, we have no alternate */
    return USB_UNSUPPORT;
  else if (Interface > NUM_INTERFACES)
    return USB_UNSUPPORT;

  return USB_SUCCESS;
}

#define USB_CCID_REQ_ABORT			0x01
#define USB_CCID_REQ_GET_CLOCK_FREQUENCIES	0x02
#define USB_CCID_REQ_GET_DATA_RATES		0x03

static const uint8_t freq_table[] = { 0xf3, 0x0d, 0, 0, }; /* dwDefaultClock */
static uint8_t *
gnuk_clock_frequencies (uint16_t len)
{
  if (len == 0)
    {
      pInformation->Ctrl_Info.Usb_wLength = sizeof (freq_table);
      return NULL;
    }

  return (uint8_t *)freq_table;
}

static const uint8_t data_rate_table[] = { 0x80, 0x25, 0, 0, }; /* dwDataRate */
static uint8_t *
gnuk_data_rates (uint16_t len)
{
  if (len == 0)
    {
      pInformation->Ctrl_Info.Usb_wLength = sizeof (data_rate_table);
      return NULL;
    }

  return (uint8_t *)data_rate_table;
}

static RESULT
gnuk_setup_with_data (uint8_t RequestNo)
{
  if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    if (pInformation->USBwIndex0 == 0) /* Interface */
      {
	if (RequestNo == USB_CCID_REQ_GET_CLOCK_FREQUENCIES)
	  {
	    pInformation->Ctrl_Info.CopyData = gnuk_clock_frequencies;
	    pInformation->Ctrl_Info.Usb_wOffset = 0;
	    gnuk_clock_frequencies (0);
	    return USB_SUCCESS;
	  }
	else if (RequestNo == USB_CCID_REQ_GET_DATA_RATES)
	  {
	    pInformation->Ctrl_Info.CopyData = gnuk_data_rates;
	    pInformation->Ctrl_Info.Usb_wOffset = 0;
	    gnuk_data_rates (0);
	    return USB_SUCCESS;
	  }
	else
	  return USB_UNSUPPORT;
      }
    else
      {
#if defined(ENABLE_VIRTUAL_COM_PORT)
	return Virtual_Com_Port_Data_Setup (RequestNo);
#else
	return USB_UNSUPPORT;
#endif
      }
  else
    return USB_UNSUPPORT;
}

static RESULT
gnuk_setup_with_nodata (uint8_t RequestNo)
{
  if (Type_Recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    if (pInformation->USBwIndex0 == 0) /* Interface */
      {
	if (RequestNo == USB_CCID_REQ_ABORT)
	  /* wValue: bSeq, bSlot */
	  /* Abortion is not supported in Gnuk */
	  return USB_UNSUPPORT;
	else
	  return USB_UNSUPPORT;
      }
    else
      {
#if defined(ENABLE_VIRTUAL_COM_PORT)
	return Virtual_Com_Port_NoData_Setup (RequestNo);
#else
	return USB_UNSUPPORT;
#endif
      }
  else
    return USB_UNSUPPORT;
}

/*
 * Interface to USB core
 */

const DEVICE_PROP Device_Property = {
  gnuk_device_init,
  gnuk_device_reset,
  gnuk_device_Status_In,
  gnuk_device_Status_Out,
  gnuk_setup_with_data,
  gnuk_setup_with_nodata,
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
  gnuk_device_SetInterface,
  NOP_Process,			/* GetStatus */
  NOP_Process,			/* ClearFeature */
  NOP_Process,			/* SetEndPointFeature */
  NOP_Process,			/* SetDeviceFeature */
  gnuk_device_SetDeviceAddress
};

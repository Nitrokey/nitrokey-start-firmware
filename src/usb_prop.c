/*
 * usb_prop.c - interface code between Gnuk and USB
 *
 * Copyright (C) 2010, 2011, 2012 Free Software Initiative of Japan
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
#include "ch.h"
#include "usb_lld.h"
#include "usb_conf.h"

#ifdef ENABLE_VIRTUAL_COM_PORT
#include "usb-cdc.h"

struct line_coding
{
  uint32_t bitrate;
  uint8_t format;
  uint8_t paritytype;
  uint8_t datatype;
};

static const struct line_coding line_coding = {
  115200, /* baud rate: 115200    */
  0x00,   /* stop bits: 1         */
  0x00,   /* parity:    none      */
  0x08    /* bits:      8         */
};

static void
vcom_port_data_setup (uint8_t RequestNo)
{
  if (RequestNo != USB_CDC_REQ_GET_LINE_CODING)
    return;

  /* RequestNo == USB_CDC_REQ_SET_LINE_CODING is not supported */

  usb_lld_set_data_to_send (&line_coding, sizeof(line_coding));
}

static int
vcom_port_setup_with_nodata (uint8_t RequestNo)
{
  if (RequestNo == USB_CDC_REQ_SET_CONTROL_LINE_STATE)
    /* Do nothing and success  */
    return USB_SUCCESS;

  return USB_UNSUPPORT;
}

#define VCOM_NUM_INTERFACES 2
#else
#define VCOM_NUM_INTERFACES 0
#endif

#ifdef PINPAD_DND_SUPPORT
#include "usb-msc.h"
#define MSC_NUM_INTERFACES 1
#else
#define MSC_NUM_INTERFACES 0
#endif

#define NUM_INTERFACES (1+VCOM_NUM_INTERFACES+MSC_NUM_INTERFACES)
#define MSC_INTERFACE_NO (1+VCOM_NUM_INTERFACES)

uint32_t bDeviceState = UNCONNECTED; /* USB device status */

static void
gnuk_device_init (void)
{
  usb_lld_set_configuration (0);
  USB_Cable_Config (1);
  bDeviceState = UNCONNECTED;
}

static void
gnuk_setup_endpoints_for_interface (uint16_t interface)
{
  if (interface == 0)
    {
      /* Initialize Endpoint 1 */
      usb_lld_setup_endpoint (ENDP1, EP_BULK, 0, ENDP1_RXADDR, ENDP1_TXADDR,
			      GNUK_MAX_PACKET_SIZE);
    }
#ifdef ENABLE_VIRTUAL_COM_PORT
  else if (interface == 1)
    {
      /* Initialize Endpoint 4 */
      usb_lld_setup_endpoint (ENDP4, EP_INTERRUPT, 0, 0, ENDP4_TXADDR, 0);
    }
  else if (interface == 2)
    {
      /* Initialize Endpoint 3 */
      usb_lld_setup_endpoint (ENDP3, EP_BULK, 0, 0, ENDP3_TXADDR, 0);

      /* Initialize Endpoint 5 */
      usb_lld_setup_endpoint (ENDP5, EP_BULK, 0, ENDP5_RXADDR, 0,
			      VIRTUAL_COM_PORT_DATA_SIZE);
    }
#endif
#ifdef PINPAD_DND_SUPPORT
  else if (interface == MSC_INTERFACE_NO)
    {
      /* Initialize Endpoint 6 */
      usb_lld_setup_endpoint (ENDP6, EP_BULK, 0, ENDP6_RXADDR, ENDP6_TXADDR,
			      64);
      usb_lld_stall_rx (ENDP6);
    }
#endif
}

static void
gnuk_device_reset (void)
{
  int i;

  /* Set DEVICE as not configured */
  usb_lld_set_configuration (0);

  /* Current Feature initialization */
  usb_lld_set_feature (Config_Descriptor.Descriptor[7]);

  usb_lld_reset ();

  /* Initialize Endpoint 0 */
  usb_lld_setup_endpoint (ENDP0, EP_CONTROL, 0, ENDP0_RXADDR, ENDP0_TXADDR,
			  GNUK_MAX_PACKET_SIZE);

  for (i = 0; i < NUM_INTERFACES; i++)
    gnuk_setup_endpoints_for_interface (i);

  bDeviceState = ATTACHED;
}

#define USB_CCID_REQ_ABORT			0x01
#define USB_CCID_REQ_GET_CLOCK_FREQUENCIES	0x02
#define USB_CCID_REQ_GET_DATA_RATES		0x03

static const uint8_t freq_table[] = { 0xf3, 0x0d, 0, 0, }; /* dwDefaultClock */

static const uint8_t data_rate_table[] = { 0x80, 0x25, 0, 0, }; /* dwDataRate */

#if defined(PINPAD_DND_SUPPORT)
static const uint8_t lun_table[] = { 0, 0, 0, 0, };
#endif

static void
gnuk_setup_with_data (uint8_t recipient, uint8_t RequestNo, uint16_t index)
{
  if (recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) /* Interface */
    {
      if (index == 0)
	{
	  if (RequestNo == USB_CCID_REQ_GET_CLOCK_FREQUENCIES)
	    usb_lld_set_data_to_send (freq_table, sizeof (freq_table));
	  else if (RequestNo == USB_CCID_REQ_GET_DATA_RATES)
	    usb_lld_set_data_to_send (data_rate_table, sizeof (data_rate_table));
	}
#ifdef ENABLE_VIRTUAL_COM_PORT
      else if (index == 1)
	vcom_port_data_setup (RequestNo);
#endif
#ifdef PINPAD_DND_SUPPORT
      else if (index == MSC_INTERFACE_NO)
	{
	  if (RequestNo == MSC_GET_MAX_LUN_COMMAND)
	    usb_lld_set_data_to_send (lun_table, sizeof (lun_table));
	}
#endif
    }
}


static int
gnuk_setup_with_nodata (uint8_t recipient, uint8_t RequestNo, uint16_t index)
{
  if (recipient == (CLASS_REQUEST | INTERFACE_RECIPIENT)) /* Interface */
    if (index == 0)
      {
	if (RequestNo == USB_CCID_REQ_ABORT)
	  /* wValue: bSeq, bSlot */
	  /* Abortion is not supported in Gnuk */
	  return USB_UNSUPPORT;
	else
	  return USB_UNSUPPORT;
      }
#ifdef ENABLE_VIRTUAL_COM_PORT
    else if (index == 1)
      return vcom_port_setup_with_nodata (RequestNo);
#endif
#ifdef PINPAD_DND_SUPPORT
    else if (index == MSC_INTERFACE_NO)
      {
	if (RequestNo == MSC_MASS_STORAGE_RESET_COMMAND)
	  {
	    /* Should call resetting MSC thread, something like msc_reset() */
	    return USB_SUCCESS;
	  }
	else
	  return USB_UNSUPPORT;
      }
#endif
    else
      return USB_UNSUPPORT;
  else
    return USB_UNSUPPORT;
}

static int
gnuk_get_descriptor (uint8_t desc_type, uint16_t index, uint16_t value)
{
  (void)index;
  if (desc_type == DEVICE_DESCRIPTOR)
    {
      usb_lld_set_data_to_send (Device_Descriptor.Descriptor,
				Device_Descriptor.Descriptor_Size);
      return USB_SUCCESS;
    }
  else if (desc_type == CONFIG_DESCRIPTOR)
    {
      usb_lld_set_data_to_send (Config_Descriptor.Descriptor,
				Config_Descriptor.Descriptor_Size);
      return USB_SUCCESS;
    }
  else if (desc_type == STRING_DESCRIPTOR)
    {
      uint8_t desc_index = value & 0xff;

      if (desc_index < NUM_STRING_DESC)
	{
	  usb_lld_set_data_to_send (String_Descriptors[desc_index].Descriptor,
			    String_Descriptors[desc_index].Descriptor_Size);
	  return USB_SUCCESS;
	}
    }

  return USB_UNSUPPORT;
}

static int gnuk_usb_event (uint8_t event_type, uint16_t value)
{
  switch (event_type)
    {
    case USB_EVENT_RESET:
      break;
    case USB_EVENT_ADDRESS:
      bDeviceState = ADDRESSED;
      break;
    case USB_EVENT_CONFIG:
      if (usb_lld_current_configuration () == 0)
	{
	  int i;
	  extern void *main_thread;
#define LED_STATUS_MODE   (8)

	  if (value != 1)
	    return USB_UNSUPPORT;

	  usb_lld_set_configuration (value);
	  for (i = 0; i < NUM_INTERFACES; i++)
	    gnuk_setup_endpoints_for_interface (i);
	  bDeviceState = CONFIGURED;
	  chEvtSignalI (main_thread, LED_STATUS_MODE);
	  return USB_SUCCESS;
	}
      else
	{
	  if (value != 0)
	    return USB_UNSUPPORT;

	  usb_lld_set_configuration (0);
	  // Disable all endpoints???
	  bDeviceState = ADDRESSED;
	}
    default:
      break;
    }

  return USB_UNSUPPORT;
}

static int gnuk_interface (uint8_t cmd, uint16_t interface, uint16_t alt)
{
  static uint8_t zero = 0;

  if (interface >= NUM_INTERFACES)
    return USB_UNSUPPORT;

  switch (cmd)
    {
    case USB_SET_INTERFACE:
      if (alt != 0)
	return USB_UNSUPPORT;
      else
	{
	  gnuk_setup_endpoints_for_interface (interface);
	  return USB_SUCCESS;
	}

    case USB_GET_INTERFACE:
      usb_lld_set_data_to_send (&zero, 1);
      return USB_SUCCESS;

    default:
    case USB_QUERY_INTERFACE:
      return USB_SUCCESS;
    }
}

/*
 * Interface to USB core
 */

const struct usb_device_method Device_Method = {
  gnuk_device_init,
  gnuk_device_reset,
  gnuk_setup_with_data,
  gnuk_setup_with_nodata,
  gnuk_get_descriptor,
  gnuk_usb_event,
  gnuk_interface,
};

/*
 * usb_ctrl.c - USB control pipe device specific code for Gnuk
 *
 * Copyright (C) 2010, 2011, 2012, 2013, 2015, 2016, 2017, 2018
 *               Free Software Initiative of Japan
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

#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"

#ifdef DEBUG
#include "debug.h"
#endif

#include "usb_lld.h"
#include "usb_conf.h"
#include "gnuk.h"
#include "neug.h"

#ifdef ENABLE_VIRTUAL_COM_PORT
#include "usb-cdc.h"

struct line_coding
{
  uint32_t bitrate;
  uint8_t format;
  uint8_t paritytype;
  uint8_t datatype;
};

static struct line_coding line_coding = {
  115200, /* baud rate: 115200    */
  0x00,   /* stop bits: 1         */
  0x00,   /* parity:    none      */
  0x08    /* bits:      8         */
};

#define CDC_CTRL_DTR            0x0001

static int
vcom_port_data_setup (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;

  if (USB_SETUP_GET (arg->type))
    {
      if (arg->request == USB_CDC_REQ_GET_LINE_CODING)
	return usb_lld_ctrl_send (dev, &line_coding, sizeof (line_coding));
    }
  else  /* USB_SETUP_SET (req) */
    {
      if (arg->request == USB_CDC_REQ_SET_LINE_CODING
	  && arg->len == sizeof (line_coding))
	return usb_lld_ctrl_recv (dev, &line_coding, sizeof (line_coding));
      else if (arg->request == USB_CDC_REQ_SET_CONTROL_LINE_STATE)
	return usb_lld_ctrl_ack (dev);
    }

  return -1;
}
#endif

#ifdef PINPAD_DND_SUPPORT
#include "usb-msc.h"
#endif

uint32_t bDeviceState = USB_DEVICE_STATE_UNCONNECTED;

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

#ifndef HID_LED_STATUS_CARDCHANGE
/* NumLock=1, CapsLock=2, ScrollLock=4 */
#define HID_LED_STATUS_CARDCHANGE 0x04
#endif

#ifdef HID_CARD_CHANGE_SUPPORT
static uint8_t hid_idle_rate;	/* in 4ms */
static uint8_t hid_report_saved;
static uint16_t hid_report;
#endif

static void
gnuk_setup_endpoints_for_interface (struct usb_dev *dev,
				    uint16_t interface, int stop)
{
#if !defined(GNU_LINUX_EMULATION)
  (void)dev;
#endif

  if (interface == CCID_INTERFACE)
    {
      if (!stop)
	{
#ifdef GNU_LINUX_EMULATION
	  usb_lld_setup_endp (dev, ENDP1, 1, 1);
	  usb_lld_setup_endp (dev, ENDP2, 0, 1);
#else
	  usb_lld_setup_endpoint (ENDP1, EP_BULK, 0, ENDP1_RXADDR,
				  ENDP1_TXADDR, GNUK_MAX_PACKET_SIZE);
	  usb_lld_setup_endpoint (ENDP2, EP_INTERRUPT, 0, 0, ENDP2_TXADDR, 0);
#endif
	}
      else
	{
	  usb_lld_stall_rx (ENDP1);
	  usb_lld_stall_tx (ENDP1);
	  usb_lld_stall_tx (ENDP2);
	}
    }
#ifdef HID_CARD_CHANGE_SUPPORT
  else if (interface == HID_INTERFACE)
    {
      if (!stop)
#ifdef GNU_LINUX_EMULATION
	usb_lld_setup_endp (dev, ENDP7, 0, 1);
#else
	usb_lld_setup_endpoint (ENDP7, EP_INTERRUPT, 0, 0, ENDP7_TXADDR, 0);
#endif
      else
	usb_lld_stall_tx (ENDP7);
    }
#endif
#ifdef ENABLE_VIRTUAL_COM_PORT
  else if (interface == VCOM_INTERFACE_0)
    {
      if (!stop)
#ifdef GNU_LINUX_EMULATION
	usb_lld_setup_endp (dev, ENDP4, 0, 1);
#else
	usb_lld_setup_endpoint (ENDP4, EP_INTERRUPT, 0, 0, ENDP4_TXADDR, 0);
#endif
      else
	usb_lld_stall_tx (ENDP4);
    }
  else if (interface == VCOM_INTERFACE_1)
    {
      if (!stop)
	{
#ifdef GNU_LINUX_EMULATION
	  usb_lld_setup_endp (dev, ENDP3, 0, 1);
	  usb_lld_setup_endp (dev, ENDP5, 1, 0);
#else
	  usb_lld_setup_endpoint (ENDP3, EP_BULK, 0, 0, ENDP3_TXADDR, 0);
	  usb_lld_setup_endpoint (ENDP5, EP_BULK, 0, ENDP5_RXADDR, 0,
				  VIRTUAL_COM_PORT_DATA_SIZE);
#endif
	}
      else
	{
	  usb_lld_stall_tx (ENDP3);
	  usb_lld_stall_rx (ENDP5);
	}
    }
#endif
#ifdef PINPAD_DND_SUPPORT
  else if (interface == MSC_INTERFACE)
    {
      if (!stop)
#ifdef GNU_LINUX_EMULATION
	usb_lld_setup_endp (dev, ENDP6, 1, 1);
#else
	usb_lld_setup_endpoint (ENDP6, EP_BULK, 0,
				ENDP6_RXADDR, ENDP6_TXADDR, 64);
#endif
      else
	{
	  usb_lld_stall_tx (ENDP6);
	  usb_lld_stall_rx (ENDP6);
	}
    }
#endif
}

void
usb_device_reset (struct usb_dev *dev)
{
  usb_lld_reset (dev, USB_INITIAL_FEATURE);

  /* Initialize Endpoint 0 */
#ifdef GNU_LINUX_EMULATION
  usb_lld_setup_endp (dev, ENDP0, 1, 1);
#else
  usb_lld_setup_endpoint (ENDP0, EP_CONTROL, 0, ENDP0_RXADDR, ENDP0_TXADDR,
			  64);
#endif

  bDeviceState = USB_DEVICE_STATE_DEFAULT;
}

#define USB_CCID_REQ_ABORT			0x01
#define USB_CCID_REQ_GET_CLOCK_FREQUENCIES	0x02
#define USB_CCID_REQ_GET_DATA_RATES		0x03

static const uint8_t freq_table[] = { 0xa0, 0x0f, 0, 0, }; /* dwDefaultClock */
static const uint8_t data_rate_table[] = { 0x80, 0x25, 0, 0, }; /* dwDataRate */

#if defined(PINPAD_DND_SUPPORT)
static const uint8_t lun_table[] = { 0, 0, 0, 0, };
#endif

#ifdef FLASH_UPGRADE_SUPPORT
static const uint8_t *const mem_info[] = { &_regnual_start,  __heap_end__, };
#endif

#define USB_FSIJ_GNUK_MEMINFO     0
#define USB_FSIJ_GNUK_DOWNLOAD    1
#define USB_FSIJ_GNUK_EXEC        2
#define USB_FSIJ_GNUK_CARD_CHANGE 3

#ifdef FLASH_UPGRADE_SUPPORT
/* After calling this function, CRC module remain enabled.  */
static int
download_check_crc32 (struct usb_dev *dev, const uint32_t *end_p)
{
  uint32_t crc32 = *end_p;
  const uint32_t *p;

  crc32_rv_reset ();

  for (p = (const uint32_t *)&_regnual_start; p < end_p; p++)
    crc32_rv_step (rbit (*p));

  if ((rbit (crc32_rv_get ()) ^ crc32) == 0xffffffff)
    return usb_lld_ctrl_ack (dev);

  return -1;
}
#endif

int
usb_setup (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t type_rcp = arg->type & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_GET (arg->type))
	{
#ifdef FLASH_UPGRADE_SUPPORT
	  if (arg->request == USB_FSIJ_GNUK_MEMINFO)
	    return usb_lld_ctrl_send (dev, mem_info, sizeof (mem_info));
#else
	  return -1;
#endif
	}
      else /* SETUP_SET */
	{
#ifdef FLASH_UPGRADE_SUPPORT
	  uint8_t *addr = sram_address ((arg->value * 0x100) + arg->index);
#endif

	  if (arg->request == USB_FSIJ_GNUK_DOWNLOAD)
	    {
#ifdef FLASH_UPGRADE_SUPPORT
	      if (ccid_get_ccid_state () != CCID_STATE_EXITED)
		return -1;

	      if (addr < &_regnual_start || addr + arg->len > __heap_end__)
		return -1;

	      if (arg->index + arg->len < 256)
		memset (addr + arg->index + arg->len, 0,
			256 - (arg->index + arg->len));

	      return usb_lld_ctrl_recv (dev, addr, arg->len);
#else
	      return -1;
#endif
	    }
	  else if (arg->request == USB_FSIJ_GNUK_EXEC && arg->len == 0)
	    {
#ifdef FLASH_UPGRADE_SUPPORT
	      if (ccid_get_ccid_state () != CCID_STATE_EXITED)
		return -1;

	      if (((uintptr_t)addr & 0x03))
		return -1;

	      return download_check_crc32 (dev, (uint32_t *)addr);
#else
	      return -1;
#endif
	    }
	  else if (arg->request == USB_FSIJ_GNUK_CARD_CHANGE && arg->len == 0)
	    {
	      if (arg->value != 0 && arg->value != 1 && arg->value != 2)
		return -1;

	      ccid_card_change_signal (arg->value);
	      return usb_lld_ctrl_ack (dev);
	    }
	}
    }
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (arg->index == CCID_INTERFACE)
	{
	  if (USB_SETUP_GET (arg->type))
	    {
	      if (arg->request == USB_CCID_REQ_GET_CLOCK_FREQUENCIES)
		return usb_lld_ctrl_send (dev, freq_table, sizeof (freq_table));
	      else if (arg->request == USB_CCID_REQ_GET_DATA_RATES)
		return usb_lld_ctrl_send (dev, data_rate_table,
					  sizeof (data_rate_table));
	    }
	  else
	    {
	      if (arg->request == USB_CCID_REQ_ABORT)
		/* wValue: bSeq, bSlot */
		/* Abortion is not supported in Gnuk */
		return -1;
	    }
	}
#ifdef HID_CARD_CHANGE_SUPPORT
      else if (arg->index == HID_INTERFACE)
	{
	  switch (arg->request)
	    {
	    case USB_HID_REQ_GET_IDLE:
	      return usb_lld_ctrl_send (dev, &hid_idle_rate, 1);
	    case USB_HID_REQ_SET_IDLE:
	      return usb_lld_ctrl_recv (dev, &hid_idle_rate, 1);

	    case USB_HID_REQ_GET_REPORT:
	      /* Request of LED status and key press */
	      return usb_lld_ctrl_send (dev, &hid_report, 2);

	    case USB_HID_REQ_SET_REPORT:
	      /* Received LED set request */
	      if (arg->len == 1)
		return usb_lld_ctrl_recv (dev, &hid_report, arg->len);
	      else
		return usb_lld_ctrl_ack (dev);

	    case USB_HID_REQ_GET_PROTOCOL:
	    case USB_HID_REQ_SET_PROTOCOL:
	      /* This driver doesn't support boot protocol.  */
	      return -1;

	    default:
	      return -1;
	    }
	}
#endif
#ifdef ENABLE_VIRTUAL_COM_PORT
      else if (arg->index == VCOM_INTERFACE_0)
	return vcom_port_data_setup (dev);
#endif
#ifdef PINPAD_DND_SUPPORT
      else if (arg->index == MSC_INTERFACE)
	{
	  if (USB_SETUP_GET (arg->type))
	    {
	      if (arg->request == MSC_GET_MAX_LUN_COMMAND)
		return usb_lld_ctrl_send (dev, lun_table, sizeof (lun_table));
	    }
	  else
	    if (arg->request == MSC_MASS_STORAGE_RESET_COMMAND)
	      return usb_lld_ctrl_ack (dev);
	}
#endif
    }

  return -1;
}


void
usb_ctrl_write_finish (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t type_rcp = arg->type & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_SET (arg->type) && arg->request == USB_FSIJ_GNUK_EXEC)
	{
	  if (ccid_get_ccid_state () != CCID_STATE_EXITED)
	    return;

	  bDeviceState = USB_DEVICE_STATE_UNCONNECTED;
	  usb_lld_prepare_shutdown (); /* No further USB communication */
	  led_blink (LED_GNUK_EXEC);	/* Notify the main.  */
	}
    }
#if defined(HID_CARD_CHANGE_SUPPORT) || defined (ENABLE_VIRTUAL_COM_PORT)
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
# if defined(ENABLE_VIRTUAL_COM_PORT)
      if (arg->index == VCOM_INTERFACE_0 && USB_SETUP_SET (arg->type)
	  && arg->request == USB_CDC_REQ_SET_CONTROL_LINE_STATE)
	{
	  uint8_t connected_saved = stdout.connected;

	  if ((arg->value & CDC_CTRL_DTR) != 0)
	    {
	      if (stdout.connected == 0)
		/* It's Open call */
		stdout.connected++;
	    }
	  else
	    {
	      if (stdout.connected)
		/* Close call */
		stdout.connected = 0;
	    }

	  chopstx_mutex_lock (&stdout.m_dev);
	  if (stdout.connected != connected_saved)
	    chopstx_cond_signal (&stdout.cond_dev);
	  chopstx_mutex_unlock (&stdout.m_dev);
	}
# endif
# if defined(HID_CARD_CHANGE_SUPPORT)
      if (arg->index == HID_INTERFACE && arg->request == USB_HID_REQ_SET_REPORT)
	{
	  if ((hid_report ^ hid_report_saved) & HID_LED_STATUS_CARDCHANGE)
	    ccid_card_change_signal (CARD_CHANGE_TOGGLE);

	  hid_report_saved = hid_report;
	}
# endif
    }
#endif
}


int
usb_set_configuration (struct usb_dev *dev)
{
  int i;
  uint8_t current_conf;

  current_conf = usb_lld_current_configuration (dev);
  if (current_conf == 0)
    {
      if (dev->dev_req.value != 1)
	return -1;

      usb_lld_set_configuration (dev, 1);
      for (i = 0; i < NUM_INTERFACES; i++)
	gnuk_setup_endpoints_for_interface (dev, i, 0);
      bDeviceState = USB_DEVICE_STATE_CONFIGURED;
    }
  else if (current_conf != dev->dev_req.value)
    {
      if (dev->dev_req.value != 0)
	return -1;

      usb_lld_set_configuration (dev, 0);
      for (i = 0; i < NUM_INTERFACES; i++)
	gnuk_setup_endpoints_for_interface (dev, i, 1);
      bDeviceState = USB_DEVICE_STATE_ADDRESSED;
    }

  /* Do nothing when current_conf == value */
  return usb_lld_ctrl_ack (dev);
}


int
usb_set_interface (struct usb_dev *dev)
{
  uint16_t interface = dev->dev_req.index;
  uint16_t alt = dev->dev_req.value;

  if (interface >= NUM_INTERFACES)
    return -1;

  if (alt != 0)
    return -1;
  else
    {
      gnuk_setup_endpoints_for_interface (dev, interface, 0);
      return usb_lld_ctrl_ack (dev);
    }
}


int
usb_get_interface (struct usb_dev *dev)
{
  const uint8_t zero = 0;
  uint16_t interface = dev->dev_req.index;

  if (interface >= NUM_INTERFACES)
    return -1;

  return usb_lld_ctrl_send (dev, &zero, 1);
}

int
usb_get_status_interface (struct usb_dev *dev)
{
  const uint16_t status_info = 0;
  uint16_t interface = dev->dev_req.index;

  if (interface >= NUM_INTERFACES)
    return -1;

  return usb_lld_ctrl_send (dev, &status_info, 2);
}

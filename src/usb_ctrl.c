/*
 * usb_ctrl.c - USB control pipe device specific code for Gnuk
 *
 * Copyright (C) 2010, 2011, 2012, 2013 Free Software Initiative of Japan
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
#include "stm32f103.h"

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

static int
vcom_port_data_setup (uint8_t req, uint8_t req_no, struct control_info *detail)
{
  if (USB_SETUP_GET (req))
    {
      if (req_no == USB_CDC_REQ_GET_LINE_CODING)
	return usb_lld_reply_request (&line_coding, sizeof(line_coding), detail);
    }
  else  /* USB_SETUP_SET (req) */
    {
      if (req_no == USB_CDC_REQ_SET_LINE_CODING)
	{
	  usb_lld_set_data_to_recv (&line_coding, sizeof(line_coding));
	  return USB_SUCCESS;
	}
      else if (req_no == USB_CDC_REQ_SET_CONTROL_LINE_STATE)
	{
	  uint8_t connected_saved = stdout.connected;

	  if (detail->value != 0)
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

	  return USB_SUCCESS;
	}
    }

  return USB_UNSUPPORT;
}
#endif

#ifdef PINPAD_DND_SUPPORT
#include "usb-msc.h"
#endif

uint32_t bDeviceState = UNCONNECTED; /* USB device status */

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
gnuk_setup_endpoints_for_interface (uint16_t interface, int stop)
{
  if (interface == ICC_INTERFACE)
    {
      if (!stop)
	{
	  usb_lld_setup_endpoint (ENDP1, EP_BULK, 0, ENDP1_RXADDR,
				  ENDP1_TXADDR, GNUK_MAX_PACKET_SIZE);
	  usb_lld_setup_endpoint (ENDP2, EP_INTERRUPT, 0, 0, ENDP2_TXADDR, 0);
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
	usb_lld_setup_endpoint (ENDP7, EP_INTERRUPT, 0, 0, ENDP7_TXADDR, 0);
      else
	usb_lld_stall_tx (ENDP7);
    }
#endif
#ifdef ENABLE_VIRTUAL_COM_PORT
  else if (interface == VCOM_INTERFACE_0)
    {
      if (!stop)
	usb_lld_setup_endpoint (ENDP4, EP_INTERRUPT, 0, 0, ENDP4_TXADDR, 0);
      else
	usb_lld_stall_tx (ENDP4);
    }
  else if (interface == VCOM_INTERFACE_1)
    {
      if (!stop)
	{
	  usb_lld_setup_endpoint (ENDP3, EP_BULK, 0, 0, ENDP3_TXADDR, 0);
	  usb_lld_setup_endpoint (ENDP5, EP_BULK, 0, ENDP5_RXADDR, 0,
				  VIRTUAL_COM_PORT_DATA_SIZE);
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
	usb_lld_setup_endpoint (ENDP6, EP_BULK, 0,
				ENDP6_RXADDR, ENDP6_TXADDR, 64);
      else
	{
	  usb_lld_stall_tx (ENDP6);
	  usb_lld_stall_rx (ENDP6);
	}
    }
#endif
}

void
usb_cb_device_reset (void)
{
  int i;

  /* Set DEVICE as not configured */
  usb_lld_set_configuration (0);

  /* Current Feature initialization */
  usb_lld_set_feature (USB_INITIAL_FEATURE);

  usb_lld_reset ();

  /* Initialize Endpoint 0 */
  usb_lld_setup_endpoint (ENDP0, EP_CONTROL, 0, ENDP0_RXADDR, ENDP0_TXADDR,
			  GNUK_MAX_PACKET_SIZE);

  for (i = 0; i < NUM_INTERFACES; i++)
    gnuk_setup_endpoints_for_interface (i, 0);

  bDeviceState = ATTACHED;
}

#define USB_CCID_REQ_ABORT			0x01
#define USB_CCID_REQ_GET_CLOCK_FREQUENCIES	0x02
#define USB_CCID_REQ_GET_DATA_RATES		0x03

static const uint8_t freq_table[] = { 0xa0, 0x0f, 0, 0, }; /* dwDefaultClock */
static const uint8_t data_rate_table[] = { 0x80, 0x25, 0, 0, }; /* dwDataRate */

#if defined(PINPAD_DND_SUPPORT)
static const uint8_t lun_table[] = { 0, 0, 0, 0, };
#endif

static const uint8_t *const mem_info[] = { &_regnual_start,  __heap_end__, };

#define USB_FSIJ_GNUK_MEMINFO     0
#define USB_FSIJ_GNUK_DOWNLOAD    1
#define USB_FSIJ_GNUK_EXEC        2
#define USB_FSIJ_GNUK_CARD_CHANGE 3

static uint32_t rbit (uint32_t v)
{
  uint32_t r;

  asm ("rbit	%0, %1" : "=r" (r) : "r" (v));
  return r;
}

/* After calling this function, CRC module remain enabled.  */
static int download_check_crc32 (const uint32_t *end_p)
{
  uint32_t crc32 = *end_p;
  const uint32_t *p;

  RCC->AHBENR |= RCC_AHBENR_CRCEN;
  CRC->CR = CRC_CR_RESET;

  for (p = (const uint32_t *)&_regnual_start; p < end_p; p++)
    CRC->DR = rbit (*p);

  if ((rbit (CRC->DR) ^ crc32) == 0xffffffff)
    return USB_SUCCESS;

  return USB_UNSUPPORT;
}

int
usb_cb_setup (uint8_t req, uint8_t req_no, struct control_info *detail)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_GET (req))
	{
	  if (req_no == USB_FSIJ_GNUK_MEMINFO)
	    return usb_lld_reply_request (mem_info, sizeof (mem_info), detail);
	}
      else /* SETUP_SET */
	{
	  uint8_t *addr = (uint8_t *)(0x20000000 + detail->value * 0x100 + detail->index);

	  if (req_no == USB_FSIJ_GNUK_DOWNLOAD)
	    {
	      if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
		return USB_UNSUPPORT;

	      if (addr < &_regnual_start || addr + detail->len > __heap_end__)
		return USB_UNSUPPORT;

	      if (detail->index + detail->len < 256)
		memset (addr + detail->index + detail->len, 0, 256 - (detail->index + detail->len));

	      usb_lld_set_data_to_recv (addr, detail->len);
	      return USB_SUCCESS;
	    }
	  else if (req_no == USB_FSIJ_GNUK_EXEC && detail->len == 0)
	    {
	      if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
		return USB_UNSUPPORT;

	      if (((uint32_t)addr & 0x03))
		return USB_UNSUPPORT;

	      return download_check_crc32 ((uint32_t *)addr);
	    }
	  else if (req_no == USB_FSIJ_GNUK_CARD_CHANGE && detail->len == 0)
	    {
	      if (detail->value != 0 && detail->value != 1 && detail->value != 2)
		return USB_UNSUPPORT;

	      ccid_card_change_signal (detail->value);
	      return USB_SUCCESS;
	    }
	}
    }
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (detail->index == ICC_INTERFACE)
	{
	  if (USB_SETUP_GET (req))
	    {
	      if (req_no == USB_CCID_REQ_GET_CLOCK_FREQUENCIES)
		return usb_lld_reply_request (freq_table, sizeof (freq_table),
					      detail);
	      else if (req_no == USB_CCID_REQ_GET_DATA_RATES)
		return usb_lld_reply_request (data_rate_table,
					      sizeof (data_rate_table), detail);
	    }
	  else
	    {
	      if (req_no == USB_CCID_REQ_ABORT)
		/* wValue: bSeq, bSlot */
		/* Abortion is not supported in Gnuk */
		return USB_UNSUPPORT;
	    }
	}
#ifdef HID_CARD_CHANGE_SUPPORT
      else if (index == HID_INTERFACE)
	{
	  switch (req_no)
	    {
	    case USB_HID_REQ_GET_IDLE:
	      return usb_lld_reply_request (&hid_idle_rate, 1, detail);
	    case USB_HID_REQ_SET_IDLE:
	      usb_lld_set_data_to_recv (&hid_idle_rate, 1, detail);
	      return USB_SUCCESS;

	    case USB_HID_REQ_GET_REPORT:
	      /* Request of LED status and key press */
	      return usb_lld_reply_request (&hid_report, 2, detail);

	    case USB_HID_REQ_SET_REPORT:
	      /* Received LED set request */
	      if (detail->len == 1)
		usb_lld_set_data_to_recv (&hid_report, detail->len);
	      return USB_SUCCESS;

	    case USB_HID_REQ_GET_PROTOCOL:
	    case USB_HID_REQ_SET_PROTOCOL:
	      /* This driver doesn't support boot protocol.  */
	      return USB_UNSUPPORT;

	    default:
	      return USB_UNSUPPORT;
	    }
	}
#endif
#ifdef ENABLE_VIRTUAL_COM_PORT
      else if (index == VCOM_INTERFACE_0)
	return vcom_port_data_setup (req, req_no, detail);
#endif
#ifdef PINPAD_DND_SUPPORT
      else if (index == MSC_INTERFACE)
	{
	  if (USB_SETUP_GET (req))
	    {
	      if (req_no == MSC_GET_MAX_LUN_COMMAND)
		return usb_lld_reply_request (lun_table, sizeof (lun_table),
					      detail);
	    }
	  else
	    if (req_no == MSC_MASS_STORAGE_RESET_COMMAND)
	      /* Should call resetting MSC thread, something like msc_reset() */
	      return USB_SUCCESS;
	}
#endif
    }

  return USB_UNSUPPORT;
}


void
usb_cb_ctrl_write_finish (uint8_t req, uint8_t req_no, uint16_t value)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_SET (req) && req_no == USB_FSIJ_GNUK_EXEC)
	{
	  if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
	    return;

	  (void)value; (void)index;
	  usb_lld_prepare_shutdown (); /* No further USB communication */
	  *icc_state_p = ICC_STATE_EXEC_REQUESTED;
	}
    }
#ifdef HID_CARD_CHANGE_SUPPORT
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (index == HID_INTERFACE && req_no == USB_HID_REQ_SET_REPORT)
	{
	  if ((hid_report ^ hid_report_saved) & HID_LED_STATUS_CARDCHANGE)
	    ccid_card_change_signal (CARD_CHANGE_TOGGLE);

	  hid_report_saved = hid_report;
	}
    }
#endif
}


int usb_cb_handle_event (uint8_t event_type, uint16_t value)
{
  int i;
  uint8_t current_conf;

  switch (event_type)
    {
    case USB_EVENT_ADDRESS:
      bDeviceState = ADDRESSED;
      return USB_SUCCESS;
    case USB_EVENT_CONFIG:
      current_conf = usb_lld_current_configuration ();
      if (current_conf == 0)
	{
	  if (value != 1)
	    return USB_UNSUPPORT;

	  usb_lld_set_configuration (value);
	  for (i = 0; i < NUM_INTERFACES; i++)
	    gnuk_setup_endpoints_for_interface (i, 0);
	  ccid_card_change_signal (CCID_CARD_INIT);
	  bDeviceState = CONFIGURED;
	}
      else if (current_conf != value)
	{
	  if (value != 0)
	    return USB_UNSUPPORT;

	  usb_lld_set_configuration (0);
	  for (i = 0; i < NUM_INTERFACES; i++)
	    gnuk_setup_endpoints_for_interface (i, 1);
	  bDeviceState = ADDRESSED;
	}
      /* Do nothing when current_conf == value */
      return USB_SUCCESS;
    default:
      break;
    }

  return USB_UNSUPPORT;
}

int usb_cb_interface (uint8_t cmd, struct control_info *detail)
{
  const uint8_t zero = 0;
  uint16_t interface = detail->index;
  uint16_t alt = detail->value;

  if (interface >= NUM_INTERFACES)
    return USB_UNSUPPORT;

  switch (cmd)
    {
    case USB_SET_INTERFACE:
      if (alt != 0)
	return USB_UNSUPPORT;
      else
	{
	  gnuk_setup_endpoints_for_interface (interface, 0);
	  return USB_SUCCESS;
	}

    case USB_GET_INTERFACE:
      return usb_lld_reply_request (&zero, 1, detail);

    case USB_QUERY_INTERFACE:
    default:
      return USB_SUCCESS;
    }
}


#define INTR_REQ_USB 20

void *
usb_intr (void *arg)
{
  chopstx_intr_t interrupt;

  (void)arg;
  usb_lld_init (USB_INITIAL_FEATURE);
  chopstx_claim_irq (&interrupt, INTR_REQ_USB);
  usb_interrupt_handler ();

  while (1)
    {
      chopstx_intr_wait (&interrupt);

      /* Process interrupt. */
      usb_interrupt_handler ();
    }

  return NULL;
}

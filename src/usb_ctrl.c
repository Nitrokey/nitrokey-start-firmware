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

static const uint32_t zero = 0;

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
vcom_port_data_setup (uint8_t req, uint8_t req_no, uint16_t value)
{
  if (USB_SETUP_GET (req))
    {
      if (req_no == USB_CDC_REQ_GET_LINE_CODING)
	{
	  usb_lld_set_data_to_send (&line_coding, sizeof(line_coding));
	  return USB_SUCCESS;
	}
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

	  if (value != 0)
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

#define NUM_INTERFACES (2+VCOM_NUM_INTERFACES+MSC_NUM_INTERFACES)
#define MSC_INTERFACE_NO (2+VCOM_NUM_INTERFACES)

uint32_t bDeviceState = UNCONNECTED; /* USB device status */

#define USB_HID_REQ_GET_REPORT   1
#define USB_HID_REQ_GET_IDLE     2
#define USB_HID_REQ_GET_PROTOCOL 3
#define USB_HID_REQ_SET_REPORT   9
#define USB_HID_REQ_SET_IDLE     10
#define USB_HID_REQ_SET_PROTOCOL 11

#define HID_LED_STATUS_NUMLOCK 0x01

static uint8_t hid_idle_rate;	/* in 4ms */
static uint8_t hid_report_saved;
static uint16_t hid_report;

static void
gnuk_setup_endpoints_for_interface (uint16_t interface, int stop)
{
  if (interface == 0)
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
  else if (interface == 1)
    {
      if (!stop)
	usb_lld_setup_endpoint (ENDP7, EP_INTERRUPT, 0, 0, ENDP7_TXADDR, 0);
      else
	usb_lld_stall_tx (ENDP7);
    }
#ifdef ENABLE_VIRTUAL_COM_PORT
  else if (interface == 2)
    {
      if (!stop)
	usb_lld_setup_endpoint (ENDP4, EP_INTERRUPT, 0, 0, ENDP4_TXADDR, 0);
      else
	usb_lld_stall_tx (ENDP4);
    }
  else if (interface == 3)
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
  else if (interface == MSC_INTERFACE_NO)
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
  usb_lld_set_feature (usb_initial_feature);

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

static const uint8_t freq_table[] = { 0xf3, 0x0d, 0, 0, }; /* dwDefaultClock */

static const uint8_t data_rate_table[] = { 0x80, 0x25, 0, 0, }; /* dwDataRate */

#if defined(PINPAD_DND_SUPPORT)
static const uint8_t lun_table[] = { 0, 0, 0, 0, };
#endif

static const uint8_t *const mem_info[] = { &_regnual_start,  __heap_end__, };

#define USB_FSIJ_GNUK_MEMINFO  0
#define USB_FSIJ_GNUK_DOWNLOAD 1
#define USB_FSIJ_GNUK_EXEC     2

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
usb_cb_setup (uint8_t req, uint8_t req_no,
	      uint16_t value, uint16_t index, uint16_t len)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_GET (req))
	{
	  if (req_no == USB_FSIJ_GNUK_MEMINFO)
	    {
	      usb_lld_set_data_to_send (mem_info, sizeof (mem_info));
	      return USB_SUCCESS;
	    }
	}
      else /* SETUP_SET */
	{
	  uint8_t *addr = (uint8_t *)(0x20000000 + value * 0x100 + index);

	  if (req_no == USB_FSIJ_GNUK_DOWNLOAD)
	    {
	      if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
		return USB_UNSUPPORT;

	      if (addr < &_regnual_start || addr + len > __heap_end__)
		return USB_UNSUPPORT;

	      if (index + len < 256)
		memset (addr + index + len, 0, 256 - (index + len));

	      usb_lld_set_data_to_recv (addr, len);
	      return USB_SUCCESS;
	    }
	  else if (req_no == USB_FSIJ_GNUK_EXEC && len == 0)
	    {
	      if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
		return USB_UNSUPPORT;

	      if (((uint32_t)addr & 0x03))
		return USB_UNSUPPORT;

	      return download_check_crc32 ((uint32_t *)addr);
	    }
	}
    }
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (index == 0)
	{
	  if (USB_SETUP_GET (req))
	    {
	      if (req_no == USB_CCID_REQ_GET_CLOCK_FREQUENCIES)
		{
		  usb_lld_set_data_to_send (freq_table, sizeof (freq_table));
		  return USB_SUCCESS;
		}
	      else if (req_no == USB_CCID_REQ_GET_DATA_RATES)
		{
		  usb_lld_set_data_to_send (data_rate_table,
					    sizeof (data_rate_table));
		  return USB_SUCCESS;
		}
	    }
	  else
	    {
	      if (req_no == USB_CCID_REQ_ABORT)
		/* wValue: bSeq, bSlot */
		/* Abortion is not supported in Gnuk */
		return USB_UNSUPPORT;
	    }
	}
      else if (index == 1)
	{
	  switch (req_no)
	    {
	    case USB_HID_REQ_GET_IDLE:
	      usb_lld_set_data_to_send (&hid_idle_rate, 1);
	      return USB_SUCCESS;
	    case USB_HID_REQ_SET_IDLE:
	      usb_lld_set_data_to_recv (&hid_idle_rate, 1);
	      return USB_SUCCESS;

	    case USB_HID_REQ_GET_REPORT:
	      /* Request of LED status and key press */
	      usb_lld_set_data_to_send (&hid_report, 2);
	      return USB_SUCCESS;

	    case USB_HID_REQ_SET_REPORT:
	      /* Received LED set request */
	      if (len == 1)
		usb_lld_set_data_to_recv (&hid_report, len);
	      return USB_SUCCESS;

	    case USB_HID_REQ_GET_PROTOCOL:
	    case USB_HID_REQ_SET_PROTOCOL:
	      /* This driver doesn't support boot protocol.  */
	      return USB_UNSUPPORT;

	    default:
	      return USB_UNSUPPORT;
	    }
	}
#ifdef ENABLE_VIRTUAL_COM_PORT
      else if (index == 2)
	return vcom_port_data_setup (req, req_no, value);
#endif
#ifdef PINPAD_DND_SUPPORT
      else if (index == MSC_INTERFACE_NO)
	{
	  if (USB_SETUP_GET (req))
	    {
	      if (req_no == MSC_GET_MAX_LUN_COMMAND)
		{
		  usb_lld_set_data_to_send (lun_table, sizeof (lun_table));
		  return USB_SUCCESS;
		}
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
usb_cb_ctrl_write_finish (uint8_t req, uint8_t req_no, uint16_t value,
			  uint16_t index, uint16_t len)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_SET (req) && req_no == USB_FSIJ_GNUK_EXEC && len == 0)
	{
	  if (icc_state_p == NULL || *icc_state_p != ICC_STATE_EXITED)
	    return;

	  (void)value; (void)index;
	  usb_lld_prepare_shutdown (); /* No further USB communication */
	  *icc_state_p = ICC_STATE_EXEC_REQUESTED;
	}
    }
  else if (type_rcp == (CLASS_REQUEST | INTERFACE_RECIPIENT))
    {
      if (index == 1 && req_no == USB_HID_REQ_SET_REPORT)
	{
	  if ((hid_report ^ hid_report_saved) & HID_LED_STATUS_NUMLOCK)
	    ccid_card_change_signal ();

	  hid_report_saved = hid_report;
	}
    }
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

int usb_cb_interface (uint8_t cmd, uint16_t interface, uint16_t alt)
{
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
      usb_lld_set_data_to_send (&zero, 1);
      return USB_SUCCESS;

    default:
    case USB_QUERY_INTERFACE:
      return USB_SUCCESS;
    }
}


#define INTR_REQ_USB 20

void *
usb_intr (void *arg)
{
  chopstx_intr_t interrupt;

  (void)arg;
  usb_lld_init (usb_initial_feature);
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

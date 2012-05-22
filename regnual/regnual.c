/*
 * regnual.c -- Firmware installation for STM32F103 Flash ROM
 *
 * Copyright (C) 2012 Free Software Initiative of Japan
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

/*
 * ReGNUal
 */

#include "types.h"
#include "usb_lld.h"

extern void *memset (void *s, int c, size_t n);

extern void set_led (int);
extern uint8_t _flash_start,  _flash_end;
extern int flash_write (uint32_t dst_addr, const uint8_t *src, size_t len);
extern int flash_erase_page (uint32_t addr);
extern int flash_protect (void);
extern void nvic_system_reset (void);


#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* USB Standard Device Descriptor */
static const uint8_t regnual_device_desc[] = {
  18,   /* bLength */
  USB_DEVICE_DESCRIPTOR_TYPE,     /* bDescriptorType */
  0x10, 0x01,   /* bcdUSB = 1.1 */
  0xFF,   /* bDeviceClass: VENDOR */
  0x00,   /* bDeviceSubClass */
  0x00,   /* bDeviceProtocol */
  0x40,   /* bMaxPacketSize0 */
#include "../src/usb-vid-pid-ver.c.inc"
  1, /* Index of string descriptor describing manufacturer */
  2, /* Index of string descriptor describing product */
  3, /* Index of string descriptor describing the device's serial number */
  0x01    /* bNumConfigurations */
};

static const uint8_t regnual_config_desc[] = {
  9,
  USB_CONFIGURATION_DESCRIPTOR_TYPE, /* bDescriptorType: Configuration */
  9, 0,			/* wTotalLength: no of returned bytes */
  0,			/* bNumInterfaces: None, but control pipe */
  0x01,			/* bConfigurationValue: Configuration value */
  0x00,			/* iConfiguration: None */
#if defined(USB_SELF_POWERED)
  0xC0,				/* bmAttributes: self powered */
#else
  0x80,				/* bmAttributes: bus powered */
#endif
  50,				/* MaxPower 100 mA */
};

static const uint8_t regnual_string_lang_id[] = {
  4,				/* bLength */
  USB_STRING_DESCRIPTOR_TYPE,
  0x09, 0x04			/* LangID = 0x0409: US-English */
};

#include "../src/usb-string-vendor-product.c.inc"

static const uint8_t regnual_string_serial[] = {
  8*2+2,
  USB_STRING_DESCRIPTOR_TYPE,
  /* FSIJ-0.0 */
  'F', 0, 'S', 0, 'I', 0, 'J', 0, '-', 0, 
  '0', 0, '.', 0, '0', 0,
};

const struct Descriptor device_desc = {
  regnual_device_desc,
  sizeof (regnual_device_desc)
};

const struct Descriptor config_desc = {
  regnual_config_desc,
  sizeof (regnual_config_desc)
};

const struct Descriptor string_descs[] = {
  {regnual_string_lang_id, sizeof (regnual_string_lang_id)},
  {gnukStringVendor, sizeof (gnukStringVendor)},
  {gnukStringProduct, sizeof (gnukStringProduct)},
  {regnual_string_serial, sizeof (regnual_string_serial)},
};

#define NUM_STRING_DESC (sizeof (string_descs)/sizeof (struct Descriptor))

static void
regnual_device_reset (void)
{
  /* Set DEVICE as not configured */
  usb_lld_set_configuration (0);

  /* Current Feature initialization */
  usb_lld_set_feature (config_desc.Descriptor[7]);

  usb_lld_reset ();

  /* Initialize Endpoint 0 */
  usb_lld_setup_endpoint (ENDP0, EP_CONTROL, 0, ENDP0_RXADDR, ENDP0_TXADDR,
			  64);
}

#define USB_REGNUAL_MEMINFO    0
#define USB_REGNUAL_SEND       1
#define USB_REGNUAL_CRC32      2
#define USB_REGNUAL_FLASH      3
#define USB_REGNUAL_ERASE      4
#define USB_REGNUAL_PROTECT    5
#define USB_REGNUAL_FINISH     6

static uint8_t mem[256];

static const uint8_t *const mem_info[] = { &_flash_start,  &_flash_end, };


static void regnual_ctrl_write_finish (uint8_t req, uint8_t req_no,
				    uint16_t value, uint16_t index,
				    uint16_t len)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT)
      && USB_SETUP_SET (req) && len == 0)
    {
      if (req_no == USB_REGNUAL_FINISH && value == 0 && index == 0)
	nvic_system_reset ();
    }  
}

static int
regnual_setup (uint8_t req, uint8_t req_no,
	       uint16_t value, uint16_t index, uint16_t len)
{
  uint8_t type_rcp = req & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_GET (req))
	{
	  if (req_no == USB_REGNUAL_MEMINFO)
	    {
	      usb_lld_set_data_to_send (mem_info, sizeof (mem_info));
	      return USB_SUCCESS;
	    }
	  else if (req_no == USB_REGNUAL_CRC32)
	    {
	      static uint32_t crc32_check = 0; /* calculate CRC32 for mem */

	      usb_lld_set_data_to_send (&crc32_check, sizeof (uint32_t));
	      return USB_SUCCESS;
	    }
	}
      else /* SETUP_SET */
	{
	  if (req_no == USB_REGNUAL_SEND)
	    {
	      if (value != 0 || index + len > 256)
		return USB_UNSUPPORT;

	      if (index != 0)
		memset (mem, 0xff, 256);

	      usb_lld_set_data_to_recv (mem + index, len);
	      return USB_SUCCESS;
	    }
	  else if (req_no == USB_REGNUAL_FLASH && len == 0)
	    {
	      uint32_t dst_addr = (0x08000000 + value * 0x100);

	      if (flash_write (dst_addr, mem, 256) == 0)
		return USB_SUCCESS;
	    }
	  else if (req_no == USB_REGNUAL_ERASE && len == 0 && index == 0)
	    {
	      uint32_t dst_addr = (0x08000000 + value * 0x100);

	      if (flash_erase_page (dst_addr))
		return USB_SUCCESS;
	    }
	  else if (req_no == USB_REGNUAL_PROTECT && len == 0
		   && value == 0 && index == 0)
	    {
	      if (flash_protect ())
		return USB_SUCCESS;
	    }
	  else if (req_no == USB_REGNUAL_FINISH && len == 0
		   && value == 0 && index == 0)
	    return USB_SUCCESS;
	}
    }

  return USB_UNSUPPORT;
}

static int
regnual_get_descriptor (uint8_t desc_type, uint16_t index, uint16_t value)
{
  (void)index;
  if (desc_type == DEVICE_DESCRIPTOR)
    {
      usb_lld_set_data_to_send (device_desc.Descriptor,
				device_desc.Descriptor_Size);
      return USB_SUCCESS;
    }
  else if (desc_type == CONFIG_DESCRIPTOR)
    {
      usb_lld_set_data_to_send (config_desc.Descriptor,
				config_desc.Descriptor_Size);
      return USB_SUCCESS;
    }
  else if (desc_type == STRING_DESCRIPTOR)
    {
      uint8_t desc_index = value & 0xff;

      if (desc_index < NUM_STRING_DESC)
	{
	  usb_lld_set_data_to_send (string_descs[desc_index].Descriptor,
				    string_descs[desc_index].Descriptor_Size);
	  return USB_SUCCESS;
	}
    }

  return USB_UNSUPPORT;
}

static int regnual_usb_event (uint8_t event_type, uint16_t value)
{
  (void)value;

  switch (event_type)
    {
    case USB_EVENT_ADDRESS:
    case USB_EVENT_CONFIG:
      return USB_SUCCESS;
    default:
      break;
    }

  return USB_UNSUPPORT;
}

static int regnual_interface (uint8_t cmd, uint16_t interface, uint16_t alt)
{
  (void)cmd; (void)interface; (void)alt;
  return USB_UNSUPPORT;
}

const struct usb_device_method Device_Method = {
  regnual_device_reset,
  regnual_ctrl_write_finish,
  regnual_setup,
  regnual_get_descriptor,
  regnual_usb_event,
  regnual_interface,
};

static void wait (int count)
{
  int i;

  for (i = 0; i < count; i++)
    asm volatile ("" : : "r" (i) : "memory");
}

#define WAIT 2400000

int
main (int argc, char *argv[])
{
  (void)argc; (void)argv;

  set_led (0);

  usb_lld_init ();

  while (1)
    {
      set_led (1);
      wait (WAIT);
      set_led (0);
      wait (WAIT);
    }
}

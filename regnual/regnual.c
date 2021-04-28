/*
 * regnual.c -- Firmware installation for STM32F103 Flash ROM
 *
 * Copyright (C) 2012, 2013, 2015, 2016, 2017, 2018
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

/*
 * ReGNUal
 */

#include <string.h>

#include "types.h"
#include "usb_lld.h"
#include "sys.h"

extern void *memset (void *s, int c, size_t n);

extern void set_led (int);
extern int flash_write (uint32_t dst_addr, const uint8_t *src, size_t len);
extern int flash_protect (void);
extern void nvic_system_reset (void);


#define FLASH_START_ADDR 0x08000000 /* Fixed for all STM32F1.  */
#define FLASH_OFFSET     0x1000     /* First pages are not-writable.  */
#define FLASH_START      (FLASH_START_ADDR+FLASH_OFFSET)
#define FLASH_SIZE_REG   ((uint16_t *)0x1ffff7e0)
static uint32_t flash_end;


#define ENDP0_RXADDR        (0x40)
#define ENDP0_TXADDR        (0x80)

/* USB Standard Device Descriptor */
static const uint8_t regnual_device_desc[] = {
  18,   /* bLength */
  DEVICE_DESCRIPTOR,     /* bDescriptorType */
  0x10, 0x01,   /* bcdUSB = 1.1 */
  0xFF,   /* bDeviceClass: VENDOR */
  0x00,   /* bDeviceSubClass */
  0x00,   /* bDeviceProtocol */
  0x40,   /* bMaxPacketSize0 */
  0x00, 0x00,		/* idVendor  (will be replaced)     */
  0x00, 0x00,		/* idProduct (will be replaced)     */
  0x00, 0x00,		/* bcdDevice (will be replaced)     */
  1, /* Index of string descriptor describing manufacturer */
  2, /* Index of string descriptor describing product */
  3, /* Index of string descriptor describing the device's serial number */
  0x01    /* bNumConfigurations */
};

#if defined(USB_SELF_POWERED)
#define REGNUAL_FEATURE_INIT 0xC0  /* self powered */
#else
#define REGNUAL_FEATURE_INIT 0x80  /* bus powered */
#endif

static const uint8_t regnual_config_desc[] = {
  9,
  CONFIG_DESCRIPTOR,	/* bDescriptorType: Configuration */
  18, 0,		/* wTotalLength: no of returned bytes */
  1,			/* bNumInterfaces: single vendor interface */
  0x01,			/* bConfigurationValue: Configuration value */
  0x00,			/* iConfiguration: None */
  REGNUAL_FEATURE_INIT, /* bmAttributes: bus powered */
  50,			/* MaxPower 100 mA */

  /* Interface Descriptor */
  9,
  INTERFACE_DESCRIPTOR,	    /* bDescriptorType: Interface */
  0,		            /* bInterfaceNumber: Index of this interface */
  0,			    /* Alternate setting for this interface */
  0,			    /* bNumEndpoints: None */
  0xFF,
  0,
  0,
  0,				/* string index for interface */
};

static const uint8_t regnual_string_lang_id[] = {
  4,				/* bLength */
  STRING_DESCRIPTOR,
  0x09, 0x04			/* LangID = 0x0409: US-English */
};

#include "../src/usb-strings.c.inc"

static const uint8_t regnual_string_serial[] = {
  8*2+2,
  STRING_DESCRIPTOR,
  /* FSIJ-0.0 */
  'F', 0, 'S', 0, 'I', 0, 'J', 0, '-', 0, 
  '0', 0, '.', 0, '0', 0,
};


static void
usb_device_reset (struct usb_dev *dev)
{
  usb_lld_reset (dev, REGNUAL_FEATURE_INIT);

  /* Initialize Endpoint 0 */
  usb_lld_setup_endpoint (ENDP0, EP_CONTROL, 0, ENDP0_RXADDR, ENDP0_TXADDR,
			  64);
}

#define USB_REGNUAL_MEMINFO	0
#define USB_REGNUAL_SEND	1
#define USB_REGNUAL_RESULT	2
#define USB_REGNUAL_FLASH	3
#define USB_REGNUAL_PROTECT	4
#define USB_REGNUAL_FINISH	5

static uint32_t mem[256/4];
static uint32_t result;


static uint32_t rbit (uint32_t v)
{
  uint32_t r;

  asm ("rbit	%0, %1" : "=r" (r) : "r" (v));
  return r;
}

static uint32_t fetch (int i)
{
  uint32_t v;

  v = mem[i];
  return rbit (v);
}

struct CRC {
  volatile uint32_t DR;
  volatile uint8_t  IDR;
  uint8_t   RESERVED0;
  uint16_t  RESERVED1;
  volatile uint32_t CR;
};
static struct CRC *const CRC = (struct CRC *)0x40023000;

struct RCC {
  volatile uint32_t CR;
  volatile uint32_t CFGR;
  volatile uint32_t CIR;
  volatile uint32_t APB2RSTR;
  volatile uint32_t APB1RSTR;
  volatile uint32_t AHBENR;
  /* ... */
};
static struct RCC *const RCC = (struct RCC *)0x40021000;
#define RCC_AHBENR_CRCEN        0x00000040


#define  CRC_CR_RESET 0x01
static uint32_t calc_crc32 (void)
{
  int i;

  RCC->AHBENR |= RCC_AHBENR_CRCEN;
  CRC->CR = CRC_CR_RESET;

  for (i = 0; i < 256/4; i++)
    CRC->DR = fetch (i);

  return rbit (CRC->DR);
}


static void
usb_ctrl_write_finish (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t type_rcp = arg->type & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT)
      && USB_SETUP_SET (arg->type))
    {
      if (arg->request == USB_REGNUAL_SEND && arg->value == 0)
	result = calc_crc32 ();
      else if (arg->request == USB_REGNUAL_FLASH)
	{
	  uint32_t dst_addr = (0x08000000 + arg->value * 0x100);

	  result = flash_write (dst_addr, (const uint8_t *)mem, 256);
	}
      else if (arg->request == USB_REGNUAL_PROTECT && arg->value == 0)
	result = flash_protect ();
      else if (arg->request == USB_REGNUAL_FINISH && arg->value == 0)
	nvic_system_reset ();
    }
}

static int
usb_setup (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t type_rcp = arg->type & (REQUEST_TYPE|RECIPIENT);

  if (type_rcp == (VENDOR_REQUEST | DEVICE_RECIPIENT))
    {
      if (USB_SETUP_GET (arg->type))
	{
	  if (arg->request == USB_REGNUAL_MEMINFO)
	    {
	      const uint8_t *mem_info[2];

	      mem_info[0] = (const uint8_t *)FLASH_START;
	      mem_info[1] = (const uint8_t *)flash_end;
	      return usb_lld_ctrl_send (dev, mem_info, sizeof (mem_info));
	    }
	  else if (arg->request == USB_REGNUAL_RESULT)
	    return usb_lld_ctrl_send (dev, &result, sizeof (uint32_t));
	}
      else /* SETUP_SET */
	{
	  if (arg->request == USB_REGNUAL_SEND)
	    {
	      if (arg->value != 0 || arg->index + arg->len > 256)
		return -1;

	      if (arg->index + arg->len < 256)
		memset ((uint8_t *)mem + arg->index + arg->len, 0xff,
			256 - (arg->index + arg->len));

	      return usb_lld_ctrl_recv (dev, mem + arg->index, arg->len);
	    }
	  else if (arg->request == USB_REGNUAL_FLASH && arg->len == 0
		   && arg->index == 0)
	    {
	      uint32_t dst_addr = (0x08000000 + arg->value * 0x100);

	      if (dst_addr + 256 <= flash_end)
		return usb_lld_ctrl_ack (dev);
	    }
	  else if (arg->request == USB_REGNUAL_PROTECT && arg->len == 0
		   && arg->value == 0 && arg->index == 0)
	    return usb_lld_ctrl_ack (dev);
	  else if (arg->request == USB_REGNUAL_FINISH && arg->len == 0
		   && arg->value == 0 && arg->index == 0)
	    return usb_lld_ctrl_ack (dev);
	}
    }

  return -1;
}

static int
usb_get_descriptor (struct usb_dev *dev)
{
  struct device_req *arg = &dev->dev_req;
  uint8_t rcp = arg->type & RECIPIENT;
  uint8_t desc_type = (arg->value >> 8);
  uint8_t desc_index = (arg->value & 0xff);

  if (rcp != DEVICE_RECIPIENT)
    return -1;

  if (desc_type == DEVICE_DESCRIPTOR)
    return usb_lld_ctrl_send (dev, regnual_device_desc,
			      sizeof (regnual_device_desc));
  else if (desc_type == CONFIG_DESCRIPTOR)
    return usb_lld_ctrl_send (dev, regnual_config_desc,
			      sizeof (regnual_config_desc)); 
  else if (desc_type == STRING_DESCRIPTOR)
    {
      const uint8_t *str;
      int size;

      switch (desc_index)
	{
	case 0:
	  str = regnual_string_lang_id;
	  size = sizeof (regnual_string_lang_id);
	  break;
	case 1:
	  str = gnuk_string_vendor;
	  size = sizeof (gnuk_string_vendor);
	  break;
	case 2:
	  str = gnuk_string_product;
	  size = sizeof (gnuk_string_product);
	  break;
	case 3:
	  str = regnual_string_serial;
	  size = sizeof (regnual_string_serial);
	  break;
	default:
	  return -1;
	}

      return usb_lld_ctrl_send (dev, str, size);
    }

  return -1;
}

static int
usb_set_configuration (struct usb_dev *dev)
{
  uint8_t current_conf;

  current_conf = usb_lld_current_configuration (dev);
  if (current_conf == 0)
    {
      if (dev->dev_req.value != 1)
	return -1;

      usb_lld_set_configuration (dev, 1);
    }
  else if (current_conf != dev->dev_req.value)
    {
      if (dev->dev_req.value != 0)
	return -1;

      usb_lld_set_configuration (dev, 0);
    }

  /* Do nothing when current_conf == value */
  return usb_lld_ctrl_ack (dev);
}


static void wait (int count)
{
  int i;

  for (i = 0; i < count; i++)
    asm volatile ("" : : "r" (i) : "memory");
}

#define WAIT 2400000

/* NVIC: Nested Vectored Interrupt Controller.  */
struct NVIC {
  volatile uint32_t ISER[8];
  uint32_t unused1[24];
  volatile uint32_t ICER[8];
  uint32_t unused2[24];
  volatile uint32_t ISPR[8];
  uint32_t unused3[24];
  volatile uint32_t ICPR[8];
  uint32_t unused4[24];
  volatile uint32_t IABR[8];
  uint32_t unused5[56];
  volatile uint32_t IPR[60];
};
static struct NVIC *const NVIC = (struct NVIC *const)0xE000E100;
#define NVIC_ISER(n)	(NVIC->ISER[n >> 5])

static void nvic_enable_intr (uint8_t irq_num)
{
  NVIC_ISER (irq_num) = 1 << (irq_num & 0x1f);
}

#define USB_LP_CAN1_RX0_IRQn	 20
static struct usb_dev dev;

int
main (int argc, char *argv[])
{
  (void)argc; (void)argv;

  set_led (0);

#if defined(STM32F103_OVERRIDE_FLASH_SIZE_KB)
  flash_end = FLASH_START_ADDR + STM32F103_OVERRIDE_FLASH_SIZE_KB*1024;
#else
  flash_end = FLASH_START_ADDR + (*FLASH_SIZE_REG)*1024;
#endif

  /*
   * NVIC interrupt priority was set by Gnuk.
   * USB interrupt is disabled by NVIC setting.
   * We enable the interrupt again by nvic_enable_intr.
   */
  usb_lld_init (&dev, REGNUAL_FEATURE_INIT);
  nvic_enable_intr (USB_LP_CAN1_RX0_IRQn);

  while (1)
    {
      set_led (1);
      wait (WAIT);
      set_led (0);
      wait (WAIT);
    }
}

void
usb_interrupt_handler (void)
{
  uint8_t ep_num;
  int e;

  e = usb_lld_event_handler (&dev);
  ep_num = USB_EVENT_ENDP (e);

  if (ep_num == 0)
    switch (USB_EVENT_ID (e))
      {
      case USB_EVENT_DEVICE_RESET:
	usb_device_reset (&dev);
	break;

      case USB_EVENT_DEVICE_ADDRESSED:
	break;

      case USB_EVENT_GET_DESCRIPTOR:
	if (usb_get_descriptor (&dev) < 0)
	  usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_SET_CONFIGURATION:
	if (usb_set_configuration (&dev) < 0)
	  usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_SET_INTERFACE:
	usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_CTRL_REQUEST:
	/* Device specific device request.  */
	if (usb_setup (&dev) < 0)
	  usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_GET_STATUS_INTERFACE:
	usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_GET_INTERFACE:
	usb_lld_ctrl_error (&dev);
	break;

      case USB_EVENT_SET_FEATURE_DEVICE:
      case USB_EVENT_SET_FEATURE_ENDPOINT:
      case USB_EVENT_CLEAR_FEATURE_DEVICE:
      case USB_EVENT_CLEAR_FEATURE_ENDPOINT:
	usb_lld_ctrl_ack (&dev);
	break;

      case USB_EVENT_CTRL_WRITE_FINISH:
	/* Control WRITE transfer finished.  */
	usb_ctrl_write_finish (&dev);
	break;

      case USB_EVENT_OK:
      case USB_EVENT_DEVICE_SUSPEND:
      default:
	break;
      }
}

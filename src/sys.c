/*
 * sys.c - system services at the first flash ROM blocks
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

#include "config.h"
#include "ch.h"
#include "hal.h"
#include "board.h"
#include "usb_lld.h"

extern uint8_t __flash_start__, __flash_end__;


static void
usb_cable_config (int enable)
{
#if defined(SET_USB_CONDITION)
  if (SET_USB_CONDITION (enable))
    palSetPad (IOPORT_USB, GPIO_USB);
  else
    palClearPad (IOPORT_USB, GPIO_USB);
#else
  (void)enable;
#endif
}

static void
set_led (int on)
{
  if (SET_LED_CONDITION (on))
    palSetPad (IOPORT_LED, GPIO_LED);
  else
    palClearPad (IOPORT_LED, GPIO_LED);
}


#define FLASH_KEY1               0x45670123UL
#define FLASH_KEY2               0xCDEF89ABUL

static void
flash_unlock (void)
{
  FLASH->KEYR = FLASH_KEY1;
  FLASH->KEYR = FLASH_KEY2;
}


static int
flash_wait_for_last_operation (uint32_t timeout)
{
  int status;

  do
    {
      status = FLASH->SR;
      if (--timeout == 0)
	break;
    }
  while ((status & FLASH_SR_BSY) != 0);

  return status & (FLASH_SR_BSY|FLASH_SR_PGERR|FLASH_SR_WRPRTERR);
}

#define FLASH_PROGRAM_TIMEOUT 0x00010000
#define FLASH_ERASE_TIMEOUT   0x01000000

static int
flash_program_halfword (uint32_t addr, uint16_t data)
{
  int status;

  status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);

  port_disable ();
  if (status == 0)
    {
      FLASH->CR |= FLASH_CR_PG;

      *(volatile uint16_t *)addr = data;

      status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);
      FLASH->CR &= ~FLASH_CR_PG;
    }
  port_enable ();

  return status;
}

static int
flash_erase_page (uint32_t addr)
{
  int status;

  status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);

  port_disable ();
  if (status == 0)
    {
      FLASH->CR |= FLASH_CR_PER;
      FLASH->AR = addr;
      FLASH->CR |= FLASH_CR_STRT;

      status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);
      FLASH->CR &= ~FLASH_CR_PER;
    }
  port_enable ();

  return status;
}

static int
flash_check_blank (const uint8_t *p_start, size_t size)
{
  const uint8_t *p;

  for (p = p_start; p < p_start + size; p++)
    if (*p != 0xff)
      return 0;

  return 1;
}

static int
flash_write (uint32_t dst_addr, const uint8_t *src, size_t len)
{
  int status;
  uint32_t flash_start = (uint32_t)&__flash_start__;
  uint32_t flash_end = (uint32_t)&__flash_end__;

  if (dst_addr < flash_start || dst_addr + len > flash_end)
    return 0;

  while (len)
    {
      uint16_t hw = *src++;

      hw |= (*src++ << 8);
      status = flash_program_halfword (dst_addr, hw);
      if (status != 0)
	return 0;		/* error return */

      dst_addr += 2;
      len -= 2;
    }

  return 1;
}

#define OPTION_BYTES_ADDR 0x1ffff800

static int
flash_protect (void)
{
  int status;
  uint32_t option_bytes_value;

  status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);

  port_disable ();
  if (status == 0)
    {
      FLASH->OPTKEYR = FLASH_KEY1;
      FLASH->OPTKEYR = FLASH_KEY2;

      FLASH->CR |= FLASH_CR_OPTER;
      FLASH->CR |= FLASH_CR_STRT;

      status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);
      FLASH->CR &= ~FLASH_CR_OPTER;
    }
  port_enable ();

  if (status != 0)
    return 0;

  option_bytes_value = *(uint32_t *)OPTION_BYTES_ADDR;
  return (option_bytes_value & 0xff) == 0xff ? 1 : 0;
}


static void __attribute__((naked))
flash_erase_all_and_exec (void (*entry)(void))
{
  uint32_t addr = (uint32_t)&__flash_start__;
  uint32_t end = (uint32_t)&__flash_end__;
  int r;

  while (addr < end)
    {
      r = flash_erase_page (addr);
      if (r != 0)
	break;

      addr += FLASH_PAGE_SIZE;
    }

  if (addr >= end)
    (*entry) ();

  for (;;);
}

static void
nvic_enable_vector (uint32_t n, uint32_t prio)
{
  unsigned int sh = (n & 3) << 3;

  NVIC_IPR (n >> 2) = (NVIC_IPR(n >> 2) & ~(0xFF << sh)) | (prio << sh);
  NVIC_ICPR (n >> 5) = 1 << (n & 0x1F);
  NVIC_ISER (n >> 5) = 1 << (n & 0x1F);
}

static void
usb_lld_sys_init (void)
{
  RCC->APB1ENR |= RCC_APB1ENR_USBEN;
  nvic_enable_vector (USB_LP_CAN1_RX0_IRQn,
		      CORTEX_PRIORITY_MASK (STM32_USB_IRQ_PRIORITY));
  /*
   * Note that we also have other IRQ(s):
   * 	USB_HP_CAN1_TX_IRQn (for double-buffered or isochronous)
   * 	USBWakeUp_IRQn (suspend/resume)
   */
  RCC->APB1RSTR = RCC_APB1RSTR_USBRST;
  RCC->APB1RSTR = 0;

  usb_cable_config (1);
}

static void
usb_lld_sys_shutdown (void)
{
  RCC->APB1ENR &= ~RCC_APB1ENR_USBEN;
  usb_cable_config (0);
}

#define SYSRESETREQ 0x04
static void
nvic_system_reset (void)
{
  SCB->AIRCR = (0x05FA0000 | (SCB->AIRCR & 0x70) | SYSRESETREQ);
  asm volatile ("dsb");
}

static void __attribute__ ((naked))
reset (void)
{
  asm volatile ("cpsid	i\n\t"		/* Mask all interrupts. */
		"mov.w	r0, #0xed00\n\t" /* r0 = SCR */
		"movt	r0, #0xe000\n\t"
		"mov	r1, pc\n\t"	 /* r1 = (PC + 0x1000) & ~0x0fff */
		"mov	r2, #0x1000\n\t"
		"add	r1, r1, r2\n\t"
		"sub	r2, r2, #1\n\t"
		"bic	r1, r1, r2\n\t"
		"str	r1, [r0, #8]\n\t"	/* Set SCR->VCR */
		"ldr	r0, [r1], #4\n\t"
		"msr	MSP, r0\n\t"	/* Main (exception handler) stack. */
		"ldr	r0, [r1]\n\t"	/* Reset handler.                  */
		"bx	r0\n"
		: /* no output */ : /* no input */ : "memory");
}

typedef void (*handler)(void);
extern uint8_t __ram_end__;

handler vector[] __attribute__ ((section(".vectors"))) = {
  (handler)&__ram_end__,
  reset,
  (handler)set_led,
  flash_unlock,
  (handler)flash_program_halfword,
  (handler)flash_erase_page,
  (handler)flash_check_blank,
  (handler)flash_write,
  (handler)flash_protect,
  (handler)flash_erase_all_and_exec,
  usb_lld_sys_init,
  usb_lld_sys_shutdown,
  nvic_system_reset,
};

const uint8_t sys_version[8] __attribute__((section(".sys.version"))) = {
  3*2+2,	     /* bLength */
  0x03,		     /* bDescriptorType = USB_STRING_DESCRIPTOR_TYPE*/
  /* sys version: "1.0" */
  '1', 0, '.', 0, '0', 0,
};

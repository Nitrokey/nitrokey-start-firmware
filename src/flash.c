/*
 * flash.c -- Data Objects (DO) and GPG Key handling on Flash ROM
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

/*
 * We assume single DO size is less than 256.
 *
 * NOTE: When we will support "Card holder certificate"
 * (which size is larger than 256), it will not be put into DO pool.
 */

/*
 * Note: Garbage collection and page management with flash erase
 *       is *NOT YET* implemented
 */

#include "config.h"
#include "ch.h"
#include "hal.h"
#include "gnuk.h"

#define FLASH_KEY1               0x45670123UL
#define FLASH_KEY2               0xCDEF89ABUL

enum flash_status
{
  FLASH_BUSY = 1,
  FLASH_ERROR_PG,
  FLASH_ERROR_WRP,
  FLASH_COMPLETE,
  FLASH_TIMEOUT
};

static void
flash_unlock (void)
{
  FLASH->KEYR = FLASH_KEY1;
  FLASH->KEYR = FLASH_KEY2;
}

static int
flash_get_status (void)
{
  int status;

  if ((FLASH->SR & FLASH_SR_BSY) != 0)
    status = FLASH_BUSY;
  else if ((FLASH->SR & FLASH_SR_PGERR) != 0)
    status = FLASH_ERROR_PG;
  else if((FLASH->SR & FLASH_SR_WRPRTERR) != 0 )
    status = FLASH_ERROR_WRP;
  else
    status = FLASH_COMPLETE;

  return status;
}

static int
flash_wait_for_last_operation (uint32_t timeout)
{
  int status;

  do
    if (--timeout == 0)
      return FLASH_TIMEOUT;
    else
      status = flash_get_status ();
  while (status == FLASH_BUSY);

  return status;
}

#define FLASH_PROGRAM_TIMEOUT 0x10000

static int
flash_program_halfword (uint32_t addr, uint16_t data)
{
  int status;

  status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);

  chSysLock ();
  if (status == FLASH_COMPLETE)
    {
      FLASH->CR |= FLASH_CR_PG;

      *(volatile uint16_t *)addr = data;

      status = flash_wait_for_last_operation (FLASH_PROGRAM_TIMEOUT);
      if (status != FLASH_TIMEOUT)
	FLASH->CR &= ~FLASH_CR_PG;
    }
  chSysUnlock ();

  return status;
}

/*
 * Flash memory map
 *
 * _text
 *         .text
 *         .ctors
 *         .dtors
 * _etext
 *         .data
 *         ...
 * _etext + (_edata - _data)
 *
 * 1-KiB align padding
 *
 * 1-KiB DO pool  * 3
 *
 * 3-KiB Key store (512-byte (p, q and N) key-store * 6)
 */

static const uint8_t *do_pool;
static const uint8_t *keystore_pool;

static const uint8_t *last_p;
static const uint8_t *keystore;

const uint8_t const flash_data[4] __attribute__ ((section (".gnuk_data"))) = {
  0xff, 0xff, 0xff, 0xff
};

/* Linker set this symbol */
extern uint8_t _do_pool;

void
flash_init (void)
{
  const uint8_t *p;
  extern uint8_t _keystore_pool;

  do_pool = &_do_pool;
  keystore_pool = &_keystore_pool;

  /* Seek empty keystore */
  p = keystore_pool;
  while (*p != 0xff || *(p+1) != 0xff)
    p += 512;

  keystore = p;

  flash_unlock ();
}

/*
 * DO pool managenent
 *
 * DO pool consists of two part:
 *   2-byte header
 *   contents
 *
 * Format of a DO pool content:
 *    NR:   8-bit tag_number
 *    LEN:  8-bit length
 *    DATA: data * LEN
 *    PAD:  optional byte for 16-bit alignment
 */
#define FLASH_DO_POOL_HEADER_SIZE 2
#define FLASH_DO_POOL_SIZE	  1024*3
#define FLASH_PAGE_SIZE		  1024

const uint8_t *
flash_do_pool (void)
{
  return do_pool + FLASH_DO_POOL_HEADER_SIZE;
}

void
flash_set_do_pool_last (const uint8_t *p)
{
  last_p = p;
}

const uint8_t *
flash_do_write (uint8_t nr, const uint8_t *data, int len)
{
  const uint8_t *p = last_p;
  uint16_t hw;
  uint32_t addr;
  int i;

  if (last_p - do_pool + len + FLASH_DO_POOL_HEADER_SIZE + 2 > FLASH_PAGE_SIZE)
    return NULL;		/* gc/erase/.../ */

  DEBUG_INFO ("flash DO\r\n");

  addr = (uint32_t)last_p;
  hw = nr | (len << 8);
  if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
    return NULL;
  addr += 2;

  for (i = 0; i < len/2; i ++)
    {
      hw = data[i*2] | (data[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	return NULL;
      addr += 2;
    }

  if ((len & 1))
    {
      hw = data[i*2] | 0xff00;
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	return NULL;
      addr += 2;
    }

  last_p = (const uint8_t *)addr;

  DEBUG_INFO ("flash DO...done\r\n");
  return p + 1;
}

static void
flash_warning (const char *msg)
{
  (void)msg;
  DEBUG_INFO ("FLASH: ");
  DEBUG_INFO (msg);
  DEBUG_INFO ("\r\n");
}

void
flash_do_release (const uint8_t *do_data)
{
  uint32_t addr = (uint32_t)do_data - 1;
  uint32_t addr_tag = addr;
  int i;
  int len = do_data[0];

  /* Don't filling zero for data in code (such as ds_count_initial_value) */
  if (do_data < &_do_pool || do_data > &_do_pool + FLASH_DO_POOL_SIZE)
    return;

  addr += 2;

  /* Fill zero for content and pad */
  for (i = 0; i < len/2; i ++)
    {
      if (flash_program_halfword (addr, 0) != FLASH_COMPLETE)
	flash_warning ("fill-zero failure");
      addr += 2;
    }

  if ((len & 1))
    {
      if (flash_program_halfword (addr, 0) != FLASH_COMPLETE)
	flash_warning ("fill-zero pad failure");
      addr += 2;
    }

  /* Fill 0x0000 for "tag_number and length" word */
  if (flash_program_halfword (addr_tag, 0) != FLASH_COMPLETE)
    flash_warning ("fill-zero tag_nr failure");
}

uint8_t *
flash_key_alloc (void)
{
  uint8_t *k = (uint8_t *)keystore;

  keystore += 512;
  return k;
}

int
flash_key_write (uint8_t *key_addr, const uint8_t *key_data,
		 const uint8_t *modulus)
{
  uint16_t hw;
  uint32_t addr;
  int i;

  addr = (uint32_t)key_addr;
  for (i = 0; i < KEY_CONTENT_LEN/2; i ++)
    {
      hw = key_data[i*2] | (key_data[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	return -1;
      addr += 2;
    }

  for (i = 0; i < KEY_CONTENT_LEN/2; i ++)
    {
      hw = modulus[i*2] | (modulus[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	return -1;
      addr += 2;
    }

  return 0;
}

void
flash_key_release (const uint8_t *key_addr)
{
  (void)key_addr;
}

void
flash_clear_halfword (uint32_t addr)
{
  flash_program_halfword (addr, 0);
}


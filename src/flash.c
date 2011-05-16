/*
 * flash.c -- Data Objects (DO) and GPG Key handling on Flash ROM
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

/*
 * We assume single DO size is less than 256.
 *
 * NOTE: "Card holder certificate" (which size is larger than 256) is
 *       not put into data pool, but is implemented by its own flash
 *       page(s).
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

void
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

#define FLASH_PROGRAM_TIMEOUT 0x00010000
#define FLASH_ERASE_TIMEOUT   0x01000000

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

static int
flash_erase_page (uint32_t addr)
{
  int status;

  status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);

  chSysLock ();
  if (status == FLASH_COMPLETE)
    {
      FLASH->CR |= FLASH_CR_PER;
      FLASH->AR = addr; 
      FLASH->CR |= FLASH_CR_STRT;

      status = flash_wait_for_last_operation (FLASH_ERASE_TIMEOUT);
      if (status != FLASH_TIMEOUT)
	FLASH->CR &= ~FLASH_CR_PER;
    }
  chSysUnlock ()

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
 * _bss_start
 *         .bss
 * _end 
 *         <alignment to page>
 * random_bits_start
 *         <one page>
 * ch_certificate_startp
 *         <2048 bytes>
 * _data_pool
 *	   <two pages>
 * _keystore_pool
 *         1.5-KiB Key store (512-byte (p, q and N) key-store * 3)
 */
#define FLASH_DATA_POOL_HEADER_SIZE	2
#if defined(STM32F10X_HD)
#define FLASH_PAGE_SIZE			2048
#else
#define FLASH_PAGE_SIZE			1024
#endif
#define FLASH_DATA_POOL_SIZE		(FLASH_PAGE_SIZE*2)
#define FLASH_KEYSTORE_SIZE		(512*3)

static const uint8_t *data_pool;
extern uint8_t _keystore_pool;

static uint8_t *last_p;
static const uint8_t *keystore;

/* The first halfword is generation for the data page (little endian) */
const uint8_t const flash_data[4] __attribute__ ((section (".gnuk_data"))) = {
  0x01, 0x00, 0xff, 0xff
};

/* Linker set this symbol */
extern uint8_t _data_pool;

const uint8_t *
flash_init (void)
{
  const uint8_t *p;
  uint16_t gen0, gen1;
  uint16_t *gen0_p = (uint16_t *)&_data_pool;
  uint16_t *gen1_p = (uint16_t *)(&_data_pool + FLASH_PAGE_SIZE);

  /* Check data pool generation and choose the page */
  gen0 = *gen0_p;
  gen1 = *gen1_p;
  if (gen0 == 0xffff)
    data_pool = &_data_pool + FLASH_PAGE_SIZE;
  else if (gen1 == 0xffff)
    data_pool = &_data_pool;
  else if (gen1 > gen0)
    data_pool = &_data_pool + FLASH_PAGE_SIZE;
  else
    data_pool = &_data_pool;

  /* Seek empty keystore */
  p = &_keystore_pool;
  while (*p != 0xff || *(p+1) != 0xff)
    p += 512;

  keystore = p;

  return data_pool + FLASH_DATA_POOL_HEADER_SIZE;
}

/*
 * Flash data pool managenent
 *
 * Flash data pool consists of two parts:
 *   2-byte header
 *   contents
 *
 * Flash data pool objects:
 *   Data Object (DO) (of smart card)
 *   Internal objects:
 *     NONE (0x0000)
 *     123-counter
 *     14-bit counter
 *     bool object
 *
 * Format of a Data Object:
 *    NR:   8-bit tag_number
 *    LEN:  8-bit length
 *    DATA: data * LEN
 *    PAD:  optional byte for 16-bit alignment
 */

void
flash_set_data_pool_last (const uint8_t *p)
{
  last_p = (uint8_t *)p;
}

/*
 * We use two pages
 */
static int
flash_copying_gc (void)
{
  uint8_t *src, *dst;
  uint16_t generation;

  if (data_pool == &_data_pool)
    {
      src = &_data_pool;
      dst = &_data_pool + FLASH_PAGE_SIZE;
    }
  else
    {
      src = &_data_pool + FLASH_PAGE_SIZE;
      dst = &_data_pool;
    }

  generation = *(uint16_t *)src;
  data_pool = dst;
  gpg_data_copy (data_pool + FLASH_DATA_POOL_HEADER_SIZE);
  flash_erase_page ((uint32_t)src);
  flash_program_halfword ((uint32_t)dst, generation+1);
  return 0;
}

static int
is_data_pool_full (size_t size)
{
  return last_p + size > data_pool + FLASH_PAGE_SIZE;
}

static uint8_t *
flash_data_pool_allocate (size_t size)
{
  uint8_t *p;

  size = (size + 1) & ~1;	/* allocation unit is 1-halfword (2-byte) */

  if (is_data_pool_full (size))
    if (flash_copying_gc () < 0 || /*still*/ is_data_pool_full (size))
      fatal (FATAL_FLASH);

  p = last_p;
  last_p += size;
  return p;
}

void
flash_do_write_internal (const uint8_t *p, int nr, const uint8_t *data, int len)
{
  uint16_t hw;
  uint32_t addr;
  int i;

  addr = (uint32_t)p;
  hw = nr | (len << 8);
  if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
    flash_warning ("DO WRITE ERROR");
  addr += 2;

  for (i = 0; i < len/2; i++)
    {
      hw = data[i*2] | (data[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	flash_warning ("DO WRITE ERROR");
      addr += 2;
    }

  if ((len & 1))
    {
      hw = data[i*2] | 0xff00;
      if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	flash_warning ("DO WRITE ERROR");
      addr += 2;
    }
}

const uint8_t *
flash_do_write (uint8_t nr, const uint8_t *data, int len)
{
  const uint8_t *p;

  DEBUG_INFO ("flash DO\r\n");

  p = flash_data_pool_allocate (2 + len);
  if (p == NULL)
    {
      DEBUG_INFO ("flash data pool allocation failure.\r\n");
      return NULL;
    }

  flash_do_write_internal (p, nr, data, len);
  DEBUG_INFO ("flash DO...done\r\n");
  return p + 1;
}

void
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
  if (do_data < &_data_pool || do_data > &_data_pool + FLASH_DATA_POOL_SIZE)
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

  if ((k - &_keystore_pool) >= FLASH_KEYSTORE_SIZE)
    return NULL;

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
flash_keystore_release (void)
{
  flash_erase_page ((uint32_t)&_keystore_pool);
#if FLASH_KEYSTORE_SIZE > FLASH_PAGE_SIZE
  flash_erase_page ((uint32_t)&_keystore_pool + FLASH_PAGE_SIZE);
#endif
  keystore = &_keystore_pool;
}

void
flash_clear_halfword (uint32_t addr)
{
  flash_program_halfword (addr, 0);
}


void
flash_put_data_internal (const uint8_t *p, uint16_t hw)
{
  flash_program_halfword ((uint32_t)p, hw);
}

void
flash_put_data (uint16_t hw)
{
  uint8_t *p;

  p = flash_data_pool_allocate (2);
  if (p == NULL)
    {
      DEBUG_INFO ("data allocation failure.\r\n");
    }

  flash_program_halfword ((uint32_t)p, hw);
}


void
flash_bool_clear (const uint8_t **addr_p)
{
  const uint8_t *p;

  if ((p = *addr_p) == NULL)
    return;

  flash_program_halfword ((uint32_t)p, 0);
  *addr_p = NULL;
}

void
flash_bool_write_internal (const uint8_t *p, int nr)
{
  flash_program_halfword ((uint32_t)p, nr);
}

const uint8_t *
flash_bool_write (uint8_t nr)
{
  uint8_t *p;
  uint16_t hw = nr;

  p = flash_data_pool_allocate (2);
  if (p == NULL)
    {
      DEBUG_INFO ("bool allocation failure.\r\n");
      return NULL;
    }

  flash_program_halfword ((uint32_t)p, hw);
  return p;
}


int
flash_cnt123_get_value (const uint8_t *p)
{
  if (p == NULL)
    return 0;
  else
    {
      uint8_t v = *p;

      /*
       * After erase, a halfword in flash memory becomes 0xffff.
       * The halfword can be programmed to any value.
       * Then, the halfword can be programmed to zero.
       *
       * Thus, we can represent value 1, 2, and 3.
       */
      if (v == 0xff)
	return 1;
      else if (v == 0x00)
	return 3;
      else
	return 2;
    }
}

void
flash_cnt123_write_internal (const uint8_t *p, int which, int v)
{
  uint16_t hw;

  hw = NR_COUNTER_123 | (which << 8);
  flash_program_halfword ((uint32_t)p, hw);

  if (v == 1)
    return;
  else if (v == 2)
    flash_program_halfword ((uint32_t)p+2, 0xc3c3);
  else				/* v == 3 */
    flash_program_halfword ((uint32_t)p+2, 0);
}

void
flash_cnt123_increment (uint8_t which, const uint8_t **addr_p)
{
  const uint8_t *p;
  uint16_t hw;

  if ((p = *addr_p) == NULL)
    {
      p = flash_data_pool_allocate (4);
      if (p == NULL)
	{
	  DEBUG_INFO ("cnt123 allocation failure.\r\n");
	  return;
	}
      hw = NR_COUNTER_123 | (which << 8);
      flash_program_halfword ((uint32_t)p, hw);
      *addr_p = p + 2;
    }
  else
    {
      uint8_t v = *p;

      if (v == 0)
	return;

      if (v == 0xff)
	hw = 0xc3c3;
      else
	hw = 0;

      flash_program_halfword ((uint32_t)p, hw);
    }
}

void
flash_cnt123_clear (const uint8_t **addr_p)
{
  const uint8_t *p;

  if ((p = *addr_p) == NULL)
    return;

  flash_program_halfword ((uint32_t)p, 0);
  p -= 2;
  flash_program_halfword ((uint32_t)p, 0);
  *addr_p = NULL;
}


static int
flash_check_blank (const uint8_t *page, int size)
{
  const uint8_t *p;

  for (p = page; p < page + size; p++)
    if (*p != 0xff)
      return 0;

  return 1;
}


#define FLASH_CH_CERTIFICATE_SIZE 2048
int
flash_erase_binary (uint8_t file_id)
{
  const uint8_t *p;

  if (file_id == FILEID_CH_CERTIFICATE)
    {
      p = &ch_certificate_start;
      if (flash_check_blank (p, FLASH_CH_CERTIFICATE_SIZE) == 0)
	{
	  flash_erase_page ((uint32_t)p);
#if FLASH_CH_CERTIFICATE_SIZE > FLASH_PAGE_SIZE
	  flash_erase_page ((uint32_t)p + FLASH_PAGE_SIZE);
#endif
	}

      return 0;
    }
  else if (file_id == FILEID_RANDOM)
    {
      p = &random_bits_start;

      if (flash_check_blank (p, FLASH_PAGE_SIZE) == 0)
	flash_erase_page ((uint32_t)p);

      return 0;
    }
  else
    return -1;
}


int
flash_write_binary (uint8_t file_id, const uint8_t *data,
		    uint16_t len, uint16_t offset)
{
  uint16_t maxsize;
  const uint8_t *p;

  if (file_id == FILEID_CH_CERTIFICATE)
    {
      maxsize = FLASH_CH_CERTIFICATE_SIZE;
      p = &ch_certificate_start;
    }
  else if (file_id == FILEID_RANDOM)
    {
      maxsize = FLASH_PAGE_SIZE;
      p = &random_bits_start;
    }
  else if (file_id == FILEID_SERIAL_NO)
    {
      maxsize = 6;
      p = &openpgpcard_aid[8];
    }
  else
    return -1;

  if (offset + len > maxsize || (offset&1) || (len&1))
    return -1;
  else
    {
      uint16_t hw;
      uint32_t addr;
      int i;

      addr = (uint32_t)p + offset;
      for (i = 0; i < len/2; i++)
	{
	  hw = data[i*2] | (data[i*2+1]<<8);
	  if (flash_program_halfword (addr, hw) != FLASH_COMPLETE)
	    flash_warning ("DO WRITE ERROR");
	  addr += 2;
	}

      return 0;
    }
}

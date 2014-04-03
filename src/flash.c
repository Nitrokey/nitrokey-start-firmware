/*
 * flash.c -- Data Objects (DO) and GPG Key handling on Flash ROM
 *
 * Copyright (C) 2010, 2011, 2012, 2013
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
 * We assume single DO size is less than 256.
 *
 * NOTE: "Card holder certificate" (which size is larger than 256) is
 *       not put into data pool, but is implemented by its own flash
 *       page(s).
 */

#include <stdint.h>
#include <string.h>

#include "config.h"

#include "board.h"
#include "sys.h"
#include "gnuk.h"

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
 * ch_certificate_startp
 *         <2048 bytes>
 * _data_pool
 *	   <two pages>
 * _keystore_pool
 *         three flash pages for keystore (single: 512-byte (p, q and N))
 */
#define KEY_SIZE	512	/* P, Q and N */

#define FLASH_DATA_POOL_HEADER_SIZE	2
#define FLASH_DATA_POOL_SIZE		(FLASH_PAGE_SIZE*2)

static const uint8_t *data_pool;
extern uint8_t _keystore_pool;

static uint8_t *last_p;

/* The first halfword is generation for the data page (little endian) */
const uint8_t const flash_data[4] __attribute__ ((section (".gnuk_data"))) = {
  0x01, 0x00, 0xff, 0xff
};

/* Linker set this symbol */
extern uint8_t _data_pool;

static int key_available_at (uint8_t *k)
{
  int i;

  for (i = 0; i < KEY_SIZE; i++)
    if (k[i])
      break;
  if (i == KEY_SIZE)	/* It's ZERO.  Released key.  */
    return 0;

  for (i = 0; i < KEY_SIZE; i++)
    if (k[i] != 0xff)
      break;
  if (i == KEY_SIZE)	/* It's FULL.  Unused key.  */
    return 0;

  return 1;
}

const uint8_t *
flash_init (void)
{
  uint16_t gen0, gen1;
  uint16_t *gen0_p = (uint16_t *)&_data_pool;
  uint16_t *gen1_p = (uint16_t *)(&_data_pool + FLASH_PAGE_SIZE);
  uint8_t *p;
  int i; 

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

  /* For each key, find its address.  */
  p = &_keystore_pool;
  for (i = 0; i < 3; i++)
    {
      uint8_t *k;

      kd[i].key_addr = NULL;
      for (k = p; k < p + FLASH_PAGE_SIZE; k += KEY_SIZE)
	if (key_available_at (k))
	  {
	    kd[i].key_addr = k;
	    break;
	  }

      p += FLASH_PAGE_SIZE;
    }

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
  if (flash_program_halfword (addr, hw) != 0)
    flash_warning ("DO WRITE ERROR");
  addr += 2;

  for (i = 0; i < len/2; i++)
    {
      hw = data[i*2] | (data[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != 0)
	flash_warning ("DO WRITE ERROR");
      addr += 2;
    }

  if ((len & 1))
    {
      hw = data[i*2] | 0xff00;
      if (flash_program_halfword (addr, hw) != 0)
	flash_warning ("DO WRITE ERROR");
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
      if (flash_program_halfword (addr, 0) != 0)
	flash_warning ("fill-zero failure");
      addr += 2;
    }

  if ((len & 1))
    {
      if (flash_program_halfword (addr, 0) != 0)
	flash_warning ("fill-zero pad failure");
    }

  /* Fill 0x0000 for "tag_number and length" word */
  if (flash_program_halfword (addr_tag, 0) != 0)
    flash_warning ("fill-zero tag_nr failure");
}


uint8_t *
flash_key_alloc (enum kind_of_key kk)
{
  uint8_t *k0, *k;
  int i; 

  /* There is a page for each KK.  */
  k0 = &_keystore_pool + (FLASH_PAGE_SIZE * kk);

  /* Seek free space in the page.  */
  for (k = k0; k < k0 + FLASH_PAGE_SIZE; k += KEY_SIZE)
    {
      const uint32_t *p = (const uint32_t *)k;

      for (i = 0; i < KEY_SIZE/4; i++)
	if (p[i] != 0xffffffff)
	  break;

      if (i == KEY_SIZE/4)	/* Yes, it's empty.  */
	return k;
    }

  /* Should not happen as we have enough free space all time, but just
     in case.  */
  return NULL;
}

int
flash_key_write (uint8_t *key_addr, const uint8_t *key_data,
		 const uint8_t *pubkey, int pubkey_len)
{
  uint16_t hw;
  uint32_t addr;
  int i;

  addr = (uint32_t)key_addr;
  for (i = 0; i < KEY_CONTENT_LEN/2; i ++)
    {
      hw = key_data[i*2] | (key_data[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != 0)
	return -1;
      addr += 2;
    }

  for (i = 0; i < pubkey_len/2; i ++)
    {
      hw = pubkey[i*2] | (pubkey[i*2+1]<<8);
      if (flash_program_halfword (addr, hw) != 0)
	return -1;
      addr += 2;
    }

  return 0;
}

static int
flash_check_all_other_keys_released (const uint8_t *key_addr)
{
  uint32_t start = (uint32_t)key_addr & ~(FLASH_PAGE_SIZE - 1);
  const uint32_t *p = (const uint32_t *)start;

  while (p < (const uint32_t *)(start + FLASH_PAGE_SIZE))
    if (p == (const uint32_t *)key_addr)
      p += KEY_SIZE/4;
    else
      if (*p)
	return 0;
      else
	p++;

  return 1;
}

static void
flash_key_fill_zero_as_released (uint8_t *key_addr)
{
  int i;
  uint32_t addr = (uint32_t)key_addr;

  for (i = 0; i < KEY_SIZE/2; i++)
    flash_program_halfword (addr + i*2, 0);
}

void
flash_key_release (uint8_t *key_addr)
{
  if (flash_check_all_other_keys_released (key_addr))
    flash_erase_page (((uint32_t)key_addr & ~(FLASH_PAGE_SIZE - 1)));
  else
    flash_key_fill_zero_as_released (key_addr);
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


#if defined(CERTDO_SUPPORT)
int
flash_erase_binary (uint8_t file_id)
{
  if (file_id == FILEID_CH_CERTIFICATE)
    {
      const uint8_t *p = &ch_certificate_start;
      if (flash_check_blank (p, FLASH_CH_CERTIFICATE_SIZE) == 0)
	{
	  flash_erase_page ((uint32_t)p);
#if FLASH_CH_CERTIFICATE_SIZE > FLASH_PAGE_SIZE
	  flash_erase_page ((uint32_t)p + FLASH_PAGE_SIZE);
#endif
	}

      return 0;
    }

  return -1;
}
#endif


int
flash_write_binary (uint8_t file_id, const uint8_t *data,
		    uint16_t len, uint16_t offset)
{
  uint16_t maxsize;
  const uint8_t *p;

  if (file_id == FILEID_SERIAL_NO)
    {
      maxsize = 6;
      p = &openpgpcard_aid[8];
    }
  else if (file_id >= FILEID_UPDATE_KEY_0 && file_id <= FILEID_UPDATE_KEY_3)
    {
      maxsize = KEY_CONTENT_LEN;
      p = gpg_get_firmware_update_key (file_id - FILEID_UPDATE_KEY_0);
      if (len == 0 && offset == 0)
	{ /* This means removal of update key.  */
	  if (flash_program_halfword ((uint32_t)p, 0) != 0)
	    flash_warning ("DO WRITE ERROR");
	  return 0;
	}
    }
#if defined(CERTDO_SUPPORT)
  else if (file_id == FILEID_CH_CERTIFICATE)
    {
      maxsize = FLASH_CH_CERTIFICATE_SIZE;
      p = &ch_certificate_start;
    }
#endif
  else
    return -1;

  if (offset + len > maxsize || (offset&1) || (len&1))
    return -1;
  else
    {
      uint16_t hw;
      uint32_t addr;
      int i;

      if (flash_check_blank (p + offset, len)  == 0)
	return -1;

      addr = (uint32_t)p + offset;
      for (i = 0; i < len/2; i++)
	{
	  hw = data[i*2] | (data[i*2+1]<<8);
	  if (flash_program_halfword (addr, hw) != 0)
	    flash_warning ("DO WRITE ERROR");
	  addr += 2;
	}

      return 0;
    }
}

/*
 * pin-dnd.c -- PIN input support (Drag and Drop with File Manager)
 *
 * Copyright (C) 2011 Free Software Initiative of Japan
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
#include "board.h"
#include "gnuk.h"
#include "usb-msc.h"

struct folder {
  uint8_t parent;
  uint8_t children[7];
};

static struct folder folders[8];
static const struct folder folder_ini = { 0, { 1, 2, 3, 4, 5, 6, 7 } };


uint8_t pin_input_buffer[MAX_PIN_CHARS];
uint8_t pin_input_len;

static Thread *pin_thread;

/*
 * Let user input PIN string.
 * Return length of the string.
 * The string itself is in PIN_INPUT_BUFFER.
 */
int
pinpad_getline (int msg_code, systime_t timeout)
{
  msg_t msg;

  (void)msg_code;
  (void)timeout;

  DEBUG_INFO (">>>\r\n");

  pin_input_len = 0;

  msc_media_insert_change (1);

  memset (folders, 0, sizeof folders);
  memcpy (folders, &folder_ini, sizeof folder_ini);

  while (1)
    {
      chSysLock ();
      pin_thread = chThdSelf ();
      chSchGoSleepS (THD_STATE_SUSPENDED);
      msg = chThdSelf ()->p_u.rdymsg;
      chSysUnlock ();

      led_blink (LED_ONESHOT);
      if (msg != 0)
	break;
    }

  msc_media_insert_change (0);

  if (msg == 1)
    return pin_input_len;
  else
    return -1;			/* cancel */
}

static void pinpad_input (void)
{
  chSysLock ();
  pin_thread->p_u.rdymsg = 0;
  chSchReadyI (pin_thread);
  chSysUnlock ();
}

static void pinpad_finish_entry (int cancel)
{
  chSysLock ();
  if (cancel)
    pin_thread->p_u.rdymsg = 2;
  else
    pin_thread->p_u.rdymsg = 1;
  chSchReadyI (pin_thread);
  chSysUnlock ();
}

#define TOTAL_SECTOR 68

/*

blk=0: master boot record sector
blk=1: fat0
blk=2: fat1
blk=3: root directory
blk=4: fat cluster #2
...
blk=4+63: fat cluster #2+63
*/

static const uint8_t d0_0_sector[] = {
  0xeb, 0x3c,			       /* Jump instruction */
  0x90, /* NOP */

  0x6d, 0x6b, 0x64, 0x6f, 0x73, 0x66, 0x73, 0x20, /* "mkdosfs " */

  0x00, 0x02,			/* Bytes per sector: 512 */

  0x01,				/* sectors per cluster: 1 */
  0x01, 0x00,			/* reserved sector count: 1 */
  0x02, 			/* Number of FATs: 2 */
  0x10, 0x00,			/* Max. root directory entries: 16 (1 sector) */
  TOTAL_SECTOR, 0x00,		/* total sectors: 68 */
  0xf8,				/* media descriptor: fixed disk */
  0x01, 0x00,			/* sectors per FAT: 1 */
  0x04, 0x00, 			/* sectors per track: 4 */
  0x01, 0x00, 			/* number of heads: 1 */
  0x00, 0x00, 0x00, 0x00,	/* hidden sectors: 0 */
  0x00, 0x00, 0x00, 0x00,	/* total sectors (long) */
  0x00, 			/* drive number */
  0x00,				/* reserved */
  0x29, 			/* extended boot signature */
  0xbf, 0x86, 0x75, 0xea, /* Volume ID (serial number) (Little endian) */

  /* Volume label: DNDpinentry */
  'D', 'n', 'D', 'p', 'i', 'n', 'e', 'n', 't', 'r', 'y',

  0x46, 0x41, 0x54, 0x31, 0x32, 0x20, 0x20, 0x20, /* FAT12 */

  0x0e,				/*    push cs */
  0x1f,				/*    pop ds */
  0xbe, 0x5b, 0x7c,		/*    mov si, offset message_txt */
  0xac,				/* 1: lodsb */
  0x22, 0xc0,			/*    and al, al */
  0x74, 0x0b,			/*    jz 2f */
  0x56,				/*    push si */
  0xb4, 0x0e,			/*    mov ah, 0eh */
  0xbb, 0x07, 0x00,		/*    mov bx, 0007h */
  0xcd, 0x10,			/*    int 10h ; output char color=white */
  0x5e,				/*    pop si */
  0xeb, 0xf0,			/*    jmp 1b */
  0x32, 0xe4,			/* 2: xor ah, ah */
  0xcd, 0x16,			/*    int 16h; key input */
  0xcd, 0x19,			/*    int 19h; load OS */
  0xeb, 0xfe,			/* 3: jmp 3b */

  /* "This is not a bootable disk... \r\n" */
  0x54, 0x68, 0x69, 0x73, 0x20,
  0x69, 0x73, 0x20, 0x6e, 0x6f, 0x74, 0x20, 0x61,
  0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c,
  0x65, 0x20, 0x64, 0x69, 0x73, 0x6b, 0x2e, 0x20,
  0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20,
  0x69, 0x6e, 0x73, 0x65, 0x72, 0x74, 0x20, 0x61,
  0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c,
  0x65, 0x20, 0x66, 0x6c, 0x6f, 0x70, 0x70, 0x79,
  0x20, 0x61, 0x6e, 0x64, 0x0d, 0x0a, 0x70, 0x72,
  0x65, 0x73, 0x73, 0x20, 0x61, 0x6e, 0x79, 0x20,
  0x6b, 0x65, 0x79, 0x20, 0x74, 0x6f, 0x20, 0x74,
  0x72, 0x79, 0x20, 0x61, 0x67, 0x61, 0x69, 0x6e,
  0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d, 0x0a, 0x00,
};

static const uint8_t d0_fat0_sector[] = {
  0xf8, 0xff, 0xff,	/* Media descriptor: fixed disk */ /* EOC */
  0xff, 0xff, 0xff,	/* cluster 2: used */ /* cluster 3: used */
  0xff, 0xff, 0xff,	/* cluster 4: used */ /* cluster 5: used */
  0xff, 0xff, 0xff,	/* cluster 6: used */ /* cluster 7: used */
  0xff, 0x0f, 0x00,	/* cluster 8: used */ /* cluster 9: free */
};

static uint8_t the_sector[512];

#define FOLDER_INDEX_TO_CLUSTER_NO(i) (i+1)
#define CLUSTER_NO_TO_FOLDER_INDEX(n) (n-1)
#define FOLDER_INDEX_TO_LBA(i) (i+3)
#define LBA_TO_FOLDER_INDEX(lba) (lba-3)
#define FOLDER_INDEX_TO_DIRCHAR(i) ('A'+i-1)
#define DIRCHAR_TO_FOLDER_INDEX(c) (c - 'A' + 1)

static uint8_t *fill_file_entry (uint8_t *p, const uint8_t *filename,
				 uint16_t cluster_no)
{
  memcpy (p, filename, 8+3);
  p += 11;
  *p++ = 0x10;			/* directory */
  *p++ = 0x00;			/* reserved */
  memcpy (p, "\x64\x3b\xa7\x61\x3f", 5); /* create time */
  p += 5;
  memcpy (p, "\x61\x3f", 2);	/* last access */
  p += 2;
  *p++ = 0x00;  *p++ = 0x00;	/* ea-index */
  memcpy (p, "\x3b\xa7\x61\x3f", 4); /* last modified */
  p += 4;
  memcpy (p, &cluster_no, 2);  	/* cluster # */
  p += 2;
  *p++ = 0x00;  *p++ = 0x00;  *p++ = 0x00;  *p++ = 0x00; /* file size */
  return p;
}

static void build_directory_sector (uint8_t *p, uint8_t index)
{
  uint16_t cluster_no = FOLDER_INDEX_TO_CLUSTER_NO (index);
  int i;
  uint8_t filename[11] = { 0x2e, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
			   0x20, 0x20, 0x20 };
  uint8_t child;

  memset (p, 0, 512);

  if (index != 0)
    {
      p = fill_file_entry (p, filename, cluster_no);
      filename[1] = 0x2e;
      p = fill_file_entry (p, filename, 0);
      filename[1] = 0x20;
    }

  for (i = 0; i < 7; i++)
    if ((child = folders[index].children[i]) != 0)
      {
	filename[0] = FOLDER_INDEX_TO_DIRCHAR (child);
	p = fill_file_entry (p, filename, FOLDER_INDEX_TO_CLUSTER_NO (child));
      }
    else
      break;
}

int
msc_scsi_read (uint32_t lba, const uint8_t **sector_p)
{
  if (!media_available)
    return SCSI_ERROR_NOT_READY;

  if (lba >= TOTAL_SECTOR)
    return SCSI_ERROR_ILLEAGAL_REQUEST;

  switch (lba)
    {
    case 0:
      *sector_p = the_sector;
      memcpy (the_sector, d0_0_sector, sizeof d0_0_sector);
      memset (the_sector + sizeof d0_0_sector, 0, 512 - sizeof d0_0_sector);
      the_sector[510] = 0x55;
      the_sector[511] = 0xaa;
      return 0;
    case 1:
    case 2:
      *sector_p = the_sector;
      memcpy (the_sector, d0_fat0_sector, sizeof d0_fat0_sector);
      memset (the_sector + sizeof d0_fat0_sector, 0,
	      512 - sizeof d0_fat0_sector);
      return 0;
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
    case 9:
    case 10:
      *sector_p = the_sector;
      build_directory_sector (the_sector, LBA_TO_FOLDER_INDEX (lba));
      return 0;
    default:
      *sector_p = the_sector;
      memset (the_sector, 0, 512);
      return 0;
    }
}


static void parse_directory_sector (const uint8_t *p, uint8_t index)
{
  int i;
  uint8_t child;
  int input = 0;
  int num_children = 0;

  if (index != 0)
    {
      uint16_t cluster_no;
      uint8_t dest_index;

      p += 32;		/* skip "." */

      /* ".." */
      cluster_no = p[26] | (p[27] << 8);
      dest_index = CLUSTER_NO_TO_FOLDER_INDEX (cluster_no);

      if (dest_index < 1 || dest_index > 7)
	; /* it can be 255 : root_dir */
      else
	if (pin_input_len < MAX_PIN_CHARS - 2)
	  {
	    pin_input_buffer[pin_input_len++]
	      = FOLDER_INDEX_TO_DIRCHAR (index);
	    pin_input_buffer[pin_input_len++]
	      = FOLDER_INDEX_TO_DIRCHAR (dest_index);
	    input = 1;
	  }

      p += 32;
    }

  for (i = 0; i < 7; i++)
    {
      if (*p >= 'A' && *p <= 'G')
	{
	  child = DIRCHAR_TO_FOLDER_INDEX (*p);
	  folders[index].children[i] = child;
	  num_children++;
	}
      else
	folders[index].children[i] = 0;
      p += 32;
    }

  if (index == 0 && num_children == 1)
    pinpad_finish_entry (0);
  else if (input)
    pinpad_input ();
}

int
msc_scsi_write (uint32_t lba, const uint8_t *buf, size_t size)
{
  (void)size;

  if (!media_available)
    return SCSI_ERROR_NOT_READY;

  if (lba >= TOTAL_SECTOR)
    return SCSI_ERROR_ILLEAGAL_REQUEST;

  if (lba == 1)
    return 0;			/* updating FAT, just ignore */

  if (lba <= 2 || lba >= 11)
    return SCSI_ERROR_DATA_PROTECT;
  else
    {
      uint8_t index = LBA_TO_FOLDER_INDEX (lba);

      parse_directory_sector (buf, index);
      return 0;
    }
}

void
msc_scsi_stop (uint8_t code)
{
  (void)code;
  pinpad_finish_entry (1);
}

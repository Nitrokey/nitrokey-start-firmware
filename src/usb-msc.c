/*
 * usb-msc.c -- USB Mass Storage Class protocol handling
 *
 * Copyright (C) 2011, 2012, 2013 Free Software Initiative of Japan
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

#include <stdint.h>
#include <string.h>
#include <chopstx.h>

#include "config.h"
#include "gnuk.h"
#include "usb_lld.h"
#include "usb-msc.h"

extern uint8_t __process5_stack_base__, __process5_stack_size__;
const uint32_t __stackaddr_msc = (uint32_t)&__process5_stack_base__;
const size_t __stacksize_msc = (size_t)&__process5_stack_size__;
#define PRIO_MSC 3

static chopstx_mutex_t a_pinpad_mutex;
static chopstx_cond_t a_pinpad_cond;

static chopstx_mutex_t a_msc_mutex;
static chopstx_cond_t a_msc_cond;

#define RDY_OK    0
#define RDY_RESET 1
static uint8_t msg;

chopstx_mutex_t *pinpad_mutex = &a_pinpad_mutex;
chopstx_cond_t *pinpad_cond = &a_pinpad_cond;

static chopstx_mutex_t *msc_mutex = &a_msc_mutex;
static chopstx_cond_t *msc_cond = &a_msc_cond;


struct usb_endp_in {
  const uint8_t *txbuf;	     /* Pointer to the transmission buffer. */
  size_t txsize;	     /* Transmit transfer size remained. */
  size_t txcnt;		     /* Transmitted bytes so far. */
};

struct usb_endp_out {
  uint8_t *rxbuf;		/* Pointer to the receive buffer. */
  size_t rxsize;		/* Requested receive transfer size. */
  size_t rxcnt;			/* Received bytes so far.  */
};

static struct usb_endp_in ep6_in;
static struct usb_endp_out ep6_out;

#define ENDP_MAX_SIZE 64

static uint8_t msc_state;


static void usb_start_transmit (const uint8_t *p, size_t n)
{
  size_t pkt_len = n > ENDP_MAX_SIZE ? ENDP_MAX_SIZE : n;

  ep6_in.txbuf = p;
  ep6_in.txsize = n;
  ep6_in.txcnt = 0;

  usb_lld_write (ENDP6, (uint8_t *)ep6_in.txbuf, pkt_len);
}

/* "Data Transmitted" callback */
void
EP6_IN_Callback (void)
{
  size_t n;

  chopstx_mutex_lock (msc_mutex);

  n = (size_t)usb_lld_tx_data_len (ENDP6);
  ep6_in.txbuf += n;
  ep6_in.txcnt += n;
  ep6_in.txsize -= n;

  if (ep6_in.txsize > 0)	/* More data to be sent */
    {
      if (ep6_in.txsize > ENDP_MAX_SIZE)
	n = ENDP_MAX_SIZE;
      else
	n = ep6_in.txsize;
      usb_lld_write (ENDP6, (uint8_t *)ep6_in.txbuf, n);
    }
  else
    /* Transmit has been completed, notify the waiting thread */
    switch (msc_state)
      {
      case MSC_SENDING_CSW:
      case MSC_DATA_IN:
	msg = RDY_OK;
	chopstx_cond_signal (msc_cond);
	break;
      default:
	break;
      }

  chopstx_mutex_unlock (msc_mutex);
}


static void usb_start_receive (uint8_t *p, size_t n)
{
  ep6_out.rxbuf = p;
  ep6_out.rxsize = n;
  ep6_out.rxcnt = 0;
  usb_lld_rx_enable (ENDP6);
}

/* "Data Received" call back */
void
EP6_OUT_Callback (void)
{
  size_t n;
  int err = 0;

  chopstx_mutex_lock (msc_mutex);

  n =  (size_t)usb_lld_rx_data_len (ENDP6);
  if (n > ep6_out.rxsize)
    {				/* buffer overflow */
      err = 1;
      n = ep6_out.rxsize;
    }

  usb_lld_rxcpy (ep6_out.rxbuf, ENDP6, 0, n);
  ep6_out.rxbuf += n;
  ep6_out.rxcnt += n;
  ep6_out.rxsize -= n;

  if (n == ENDP_MAX_SIZE && ep6_out.rxsize != 0)
    /* More data to be received */
    usb_lld_rx_enable (ENDP6);
  else
    /* Receiving has been completed, notify the waiting thread */
    switch (msc_state)
      {
      case MSC_IDLE:
      case MSC_DATA_OUT:
	msg = err ? RDY_RESET : RDY_OK;
	chopstx_cond_signal (msc_cond);
	break;
      default:
	break;
      }

  chopstx_mutex_unlock (msc_mutex);
}

static const uint8_t scsi_inquiry_data_00[] = { 0, 0, 0, 0, 0 };

static const uint8_t scsi_inquiry_data[] = {
  0x00,				/* Direct Access Device.      */
  0x80,				/* RMB = 1: Removable Medium. */
  0x05,				/* Version: SPC-3.            */
  0x02,				/* Response format: SPC-3.    */
  36 - 4,			/* Additional Length.         */
  0x00,
  0x00,
  0x00,
				/* Vendor Identification */
  'F', 'S', 'I', 'J', ' ', ' ', ' ', ' ',
				/* Product Identification */
  'V', 'i', 'r', 't', 'u', 'a', 'l', ' ',
  'D', 'i', 's', 'k', ' ', ' ', ' ', ' ',
				/* Product Revision Level */
  '1', '.', '0', ' '
};

static uint8_t scsi_sense_data_desc[] = {
  0x72,			  /* Response Code: descriptor, current */
  0x02,			  /* Sense Key */
  0x3a,			  /* ASC (additional sense code) */
  0x00,			  /* ASCQ (additional sense code qualifier) */
  0x00, 0x00, 0x00,
  0x00,			  /* Additional Sense Length */
};

static uint8_t scsi_sense_data_fixed[] = {
  0x70,			  /* Response Code: fixed, current */
  0x00,
  0x02,			  /* Sense Key */
  0x00, 0x00, 0x00, 0x00,
  0x0a,			  /* Additional Sense Length */
  0x00, 0x00, 0x00, 0x00,
  0x3a,			  /* ASC (additional sense code) */
  0x00,			  /* ASCQ (additional sense code qualifier) */
  0x00,
  0x00, 0x00, 0x00,
};

static void set_scsi_sense_data(uint8_t sense_key, uint8_t asc)
{
  scsi_sense_data_desc[1] = scsi_sense_data_fixed[2] = sense_key;
  scsi_sense_data_desc[2] = scsi_sense_data_fixed[12] = asc;
}


static uint8_t buf[512];

static uint8_t contingent_allegiance;
static uint8_t keep_contingent_allegiance;

uint8_t media_available;

void
msc_media_insert_change (int available)
{
  contingent_allegiance = 1;
  media_available = available;
  if (available)
    {
      set_scsi_sense_data (0x06, 0x28); /* UNIT_ATTENTION */
      keep_contingent_allegiance = 0;
    }
  else
    {
      set_scsi_sense_data (0x02, 0x3a); /* NOT_READY */
      keep_contingent_allegiance = 1;
    }
}


static uint8_t scsi_read_format_capacities (uint32_t *nblocks,
					    uint32_t *secsize)
{
  *nblocks = 68;
  *secsize = 512;
  if (media_available)
    return 2; /* Formatted Media.*/
  else
    return 3; /* No Media.*/
}

static struct CBW CBW;

static struct CSW CSW;


/* called with holding the lock.  */
static int msc_recv_data (void)
{
  msc_state = MSC_DATA_OUT;
  usb_start_receive (buf, 512);
  chopstx_cond_wait (msc_cond, msc_mutex);
  return 0;
}

/* called with holding the lock.  */
static void msc_send_data (const uint8_t *p, size_t n)
{
  msc_state = MSC_DATA_IN;
  usb_start_transmit (p, n);
  chopstx_cond_wait (msc_cond, msc_mutex);
  CSW.dCSWDataResidue -= (uint32_t)n;
}

/* called with holding the lock.  */
static void msc_send_result (const uint8_t *p, size_t n)
{
  if (p != NULL)
    {
      if (n > CBW.dCBWDataTransferLength)
	n = CBW.dCBWDataTransferLength;

      CSW.dCSWDataResidue = CBW.dCBWDataTransferLength;
      msc_send_data (p, n);
      CSW.bCSWStatus = MSC_CSW_STATUS_PASSED;
    }

  CSW.dCSWSignature = MSC_CSW_SIGNATURE;

  msc_state = MSC_SENDING_CSW;
  usb_start_transmit ((uint8_t *)&CSW, sizeof CSW);
  chopstx_cond_wait (msc_cond, msc_mutex);
}


void
msc_handle_command (void)
{
  size_t n;
  uint32_t nblocks, secsize;
  uint32_t lba;
  int r;

  chopstx_mutex_lock (msc_mutex);
  msc_state = MSC_IDLE;
  usb_start_receive ((uint8_t *)&CBW, sizeof CBW);
  chopstx_cond_wait (msc_cond, msc_mutex);

  if (msg != RDY_OK)
    {
      /* Error occured, ignore the request and go into error state */
      msc_state = MSC_ERROR;
      usb_lld_stall_rx (ENDP6);
      goto done; 
    }

  n = ep6_out.rxcnt;

  if ((n != sizeof (struct CBW)) || (CBW.dCBWSignature != MSC_CBW_SIGNATURE))
    {
      msc_state = MSC_ERROR;
      usb_lld_stall_rx (ENDP6);
      goto done;
    }

  CSW.dCSWTag = CBW.dCBWTag;
  switch (CBW.CBWCB[0]) {
  case SCSI_REQUEST_SENSE:
    if (CBW.CBWCB[1] & 0x01) /* DESC */
      msc_send_result ((uint8_t *)&scsi_sense_data_desc,
		       sizeof scsi_sense_data_desc);
    else
      msc_send_result ((uint8_t *)&scsi_sense_data_fixed,
		       sizeof scsi_sense_data_fixed);
    /* After the error is reported, clear it, if it's .  */
    if (!keep_contingent_allegiance)
      {
	contingent_allegiance = 0;
	set_scsi_sense_data (0x00, 0x00);
      }
    goto done;
  case SCSI_INQUIRY:
    if (CBW.CBWCB[1] & 0x01) /* EVPD */
      /* assume page 00 */
      msc_send_result ((uint8_t *)&scsi_inquiry_data_00,
		       sizeof scsi_inquiry_data_00);
    else
      msc_send_result ((uint8_t *)&scsi_inquiry_data,
		       sizeof scsi_inquiry_data);
    goto done;
  case SCSI_READ_FORMAT_CAPACITIES:
    buf[8]  = scsi_read_format_capacities (&nblocks, &secsize);
    buf[0]  = buf[1] = buf[2] = 0;
    buf[3]  = 8;
    buf[4]  = (uint8_t)(nblocks >> 24);
    buf[5]  = (uint8_t)(nblocks >> 16);
    buf[6]  = (uint8_t)(nblocks >> 8);
    buf[7]  = (uint8_t)(nblocks >> 0);
    buf[9]  = (uint8_t)(secsize >> 16);
    buf[10] = (uint8_t)(secsize >> 8);
    buf[11] = (uint8_t)(secsize >> 0);
    msc_send_result (buf, 12);
    goto done;
  case SCSI_START_STOP_UNIT:
    if (CBW.CBWCB[4] == 0x00 /* stop */
	|| CBW.CBWCB[4] == 0x02 /* eject */ || CBW.CBWCB[4] == 0x03 /* close */)
      {
	msc_scsi_stop (CBW.CBWCB[4]);
	set_scsi_sense_data (0x05, 0x24); /* ILLEGAL_REQUEST */
	contingent_allegiance = 1;
	keep_contingent_allegiance = 1;
      }
    /* CBW.CBWCB[4] == 0x01 *//* start */
    goto success;
  case SCSI_TEST_UNIT_READY:
    if (contingent_allegiance)
      {
	CSW.bCSWStatus = MSC_CSW_STATUS_FAILED;
	CSW.dCSWDataResidue = 0;
	msc_send_result (NULL, 0);
	goto done;
      }
    /* fall through */
  success:
  case SCSI_SYNCHRONIZE_CACHE:
  case SCSI_VERIFY10:
  case SCSI_ALLOW_MEDIUM_REMOVAL:
    CSW.bCSWStatus = MSC_CSW_STATUS_PASSED;
    CSW.dCSWDataResidue = CBW.dCBWDataTransferLength;
    msc_send_result (NULL, 0);
    goto done;
  case SCSI_MODE_SENSE6:
    buf[0] = 0x03;
    buf[1] = buf[2] = buf[3] = 0;
    msc_send_result (buf, 4);
    goto done;
  case SCSI_READ_CAPACITY10:
    scsi_read_format_capacities (&nblocks, &secsize);
    buf[0]  = (uint8_t)((nblocks - 1) >> 24);
    buf[1]  = (uint8_t)((nblocks - 1) >> 16);
    buf[2]  = (uint8_t)((nblocks - 1) >> 8);
    buf[3]  = (uint8_t)((nblocks - 1) >> 0);
    buf[4]  = (uint8_t)(secsize >> 24);
    buf[5]  = (uint8_t)(secsize >> 16);
    buf[6] = (uint8_t)(secsize >> 8);
    buf[7] = (uint8_t)(secsize >> 0);
    msc_send_result (buf, 8);
    goto done;
  case SCSI_READ10:
  case SCSI_WRITE10:
    break;
  default:
    if (CBW.dCBWDataTransferLength == 0)
      {
	CSW.bCSWStatus = MSC_CSW_STATUS_FAILED;
	CSW.dCSWDataResidue = 0;
	msc_send_result (NULL, 0);
	goto done;
      }
    else
      {
	msc_state = MSC_ERROR;
	usb_lld_stall_tx (ENDP6);
	usb_lld_stall_rx (ENDP6);
	goto done;
      }
  }

  lba = (CBW.CBWCB[2] << 24) | (CBW.CBWCB[3] << 16)
      | (CBW.CBWCB[4] <<  8) | CBW.CBWCB[5];

  /* Transfer direction.*/
  if (CBW.bmCBWFlags & 0x80)
    {
      /* IN, Device to Host.*/
      msc_state = MSC_DATA_IN;
      if (CBW.CBWCB[0] == SCSI_READ10)
	{
	  const uint8_t *p;

	  CSW.dCSWDataResidue = 0;
	  while (1)
	    {
	      if (CBW.CBWCB[7] == 0 && CBW.CBWCB[8] == 0)
		{
		  CSW.bCSWStatus = MSC_CSW_STATUS_PASSED;
		  break;
		}

	      if ((r = msc_scsi_read (lba, &p)) == 0)
		{
		  msc_send_data (p, 512);
		  if (++CBW.CBWCB[5] == 0)
		    if (++CBW.CBWCB[4] == 0)
		      if (++CBW.CBWCB[3] == 0)
			++CBW.CBWCB[2];
		  if (CBW.CBWCB[8]-- == 0)
		    CBW.CBWCB[7]--;
		  CSW.dCSWDataResidue += 512;
		}
	      else
		{
		  CSW.bCSWStatus = MSC_CSW_STATUS_FAILED;
		  contingent_allegiance = 1;
		  if (r == SCSI_ERROR_NOT_READY)
		    set_scsi_sense_data (SCSI_ERROR_NOT_READY, 0x3a);
		  else
		    set_scsi_sense_data (r, 0x00);
		  break;
		}
	    }

	  msc_send_result (NULL, 0);
	}
    }
  else
    {
      /* OUT, Host to Device.*/
      if (CBW.CBWCB[0] == SCSI_WRITE10)
	{
	  CSW.dCSWDataResidue = CBW.dCBWDataTransferLength;

	  while (1)
	    {
	      if (CBW.CBWCB[8] == 0 && CBW.CBWCB[7] == 0)
		{
		  CSW.bCSWStatus = MSC_CSW_STATUS_PASSED;
		  break;
		}

	      msc_recv_data ();
	      if (msg != RDY_OK)
		/* ignore erroneous packet, ang go next.  */
		continue;

	      if ((r = msc_scsi_write (lba, buf, 512)) == 0)
		{
		  if (++CBW.CBWCB[5] == 0)
		    if (++CBW.CBWCB[4] == 0)
		      if (++CBW.CBWCB[3] == 0)
			++CBW.CBWCB[2];
		  if (CBW.CBWCB[8]-- == 0)
		    CBW.CBWCB[7]--;
		  CSW.dCSWDataResidue -= 512;
		}
	      else
		{
		  CSW.bCSWStatus = MSC_CSW_STATUS_FAILED;
		  contingent_allegiance = 1;
		  if (r == SCSI_ERROR_NOT_READY)
		    set_scsi_sense_data (SCSI_ERROR_NOT_READY, 0x3a);
		  else
		    set_scsi_sense_data (r, 0x00);
		  break;
		}
	    }

	  msc_send_result (NULL, 0);
	}
    }

 done:
  chopstx_mutex_unlock (msc_mutex);
}


static void *
msc_main (void *arg)
{
  (void)arg;

  chopstx_mutex_init (msc_mutex);
  chopstx_cond_init (msc_cond);

  chopstx_mutex_init (pinpad_mutex);
  chopstx_cond_init (pinpad_cond);

  /* Initially, it starts with no media */
  msc_media_insert_change (0);
  while (1)
    msc_handle_command ();

  return NULL;
}


void
msc_init (void)
{
  chopstx_create (PRIO_MSC, __stackaddr_msc, __stacksize_msc, msc_main, NULL);
}

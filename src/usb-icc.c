/*
 * usb-icc.c -- USB CCID/ICCD protocol handling
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

#include "config.h"
#include "ch.h"
#include "hal.h"
#include "gnuk.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_istr.h"

#define ICC_SET_PARAMS		0x61 /* non-ICCD command  */
#define ICC_POWER_ON		0x62
#define ICC_POWER_OFF		0x63
#define ICC_SLOT_STATUS		0x65 /* non-ICCD command */
#define ICC_GET_PARAMS		0x6C /* non-ICCD command */
#define ICC_XFR_BLOCK		0x6F
#define ICC_DATA_BLOCK_RET	0x80
#define ICC_SLOT_STATUS_RET	0x81 /* non-ICCD result */
#define ICC_PARAMS_RET		0x82 /* non-ICCD result */

#define ICC_MSG_SEQ_OFFSET	6
#define ICC_MSG_STATUS_OFFSET	7
#define ICC_MSG_ERROR_OFFSET	8
#define ICC_MSG_CHAIN_OFFSET	9
#define ICC_MSG_DATA_OFFSET	10
#define ICC_MSG_HEADER_SIZE	ICC_MSG_DATA_OFFSET
#define ICC_MAX_MSG_DATA_SIZE	(USB_BUF_SIZE - ICC_MSG_HEADER_SIZE)

#define ICC_STATUS_RUN		0x00
#define ICC_STATUS_PRESENT	0x01
#define ICC_STATUS_NOTPRESENT	0x02
#define ICC_CMD_STATUS_OK	0x00
#define ICC_CMD_STATUS_ERROR	0x40
#define ICC_CMD_STATUS_TIMEEXT	0x80

#define ICC_ERROR_XFR_OVERRUN	0xFC

/*
 * Since command-byte is at offset 0,
 * error with offset 0 means "command not supported".
 */
#define ICC_OFFSET_CMD_NOT_SUPPORTED 0
#define ICC_OFFSET_PARAM 8

struct icc_header {
  uint8_t msg_type;
  int32_t data_len;
  uint8_t slot;
  uint8_t seq;
  uint8_t rsvd;
  uint16_t param;
} __attribute__((packed));

static struct icc_header *icc_header;
static uint8_t icc_seq;

static uint8_t *icc_data;
static int icc_data_size;

static uint8_t icc_rcv_data[USB_BUF_SIZE];
static uint8_t icc_tx_data[USB_BUF_SIZE];

static int icc_tx_size;

static int
icc_tx_ready (void)
{
  if (icc_tx_size == -1)
    return 1;
  else
    return 0;
}

Thread *icc_thread;

#define EV_RX_DATA_READY (eventmask_t)1  /* USB Rx data available  */

/*
 * Tx done
 */
void
EP1_IN_Callback (void)
{
  if (icc_tx_size == USB_BUF_SIZE)
    {
      icc_tx_size = 0;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
    }
  else
    icc_tx_size = -1;
}

/*
 * Upon arrival of Bulk-OUT packet, 
 * we setup the variables (icc_header, icc_data, and icc_data_size, icc_seq)
 * and notify icc_thread
 *  (modify header's byte order to host order if needed)
 */
void
EP2_OUT_Callback (void)
{
  int len;

#ifdef HOST_BIG_ENDIAN
#error "Here, you need to write code to correct byte order."
#else
  /* nothing to do */
#endif

  len = USB_SIL_Read (EP2_OUT, icc_rcv_data);

  icc_header = (struct icc_header *)icc_rcv_data;
  icc_data = &icc_rcv_data[ICC_MSG_DATA_OFFSET];
  icc_data_size = len - ICC_MSG_HEADER_SIZE;
  icc_seq = icc_header->seq;

  if (icc_data_size < 0)
    /* just ignore short invalid packet, enable Rx again */
    SetEPRxValid (ENDP2);
  else
    /* Notify icc_thread */
    chEvtSignalI (icc_thread, EV_RX_DATA_READY);

  /*
   * what if (icc_data_size != icc_header->data_len)???
   */
}

enum icc_state
{
  ICC_STATE_START,		/* Initial */
  ICC_STATE_WAIT,		/* Waiting APDU */
				/* Busy1, Busy2, Busy3, Busy5 */
  ICC_STATE_EXECUTE,		/* Busy4 */
  ICC_STATE_RECEIVE,		/* APDU Received Partially */
  ICC_STATE_SEND,		/* APDU Sent Partially */
};

static enum icc_state icc_state;

/*
 * ATR (Answer To Reset) string
 *
 * TS = 0x3B: Direct conversion
 * T0 = 0x94: TA1 and TD1 follow, 4 historical bytes
 * TA1 = 0x11: FI=1, DI=1
 * TD1 = 0x81: TD2 follows, T=1
 * TD2 = 0x31: TA3 and TB3 follow, T=1
 * TA3 = 0xFE: IFSC = 254 bytes
 * TB3 = 0x55: BWI = 5, CWI = 5   (BWT timeout 3.2 sec)
 * Historical bytes: "FSIJ"
 * XOR check
 *
 */
static const char ATR[] = {
  0x3B, 0x94, 0x11, 0x81, 0x31, 0xFE, 0x55,
 'F', 'S', 'I', 'J',
 (0x94^0x11^0x81^0x31^0xFE^0x55^'F'^'S'^'I'^'J')
};

/*
 * Send back error
 */
void
icc_error (int offset)
{
  icc_tx_data[0] = ICC_SLOT_STATUS_RET; /* Any value should be OK */
  icc_tx_data[1] = 0x00;
  icc_tx_data[2] = 0x00;
  icc_tx_data[3] = 0x00;
  icc_tx_data[4] = 0x00;
  icc_tx_data[5] = 0x00;	/* Slot */
  icc_tx_data[ICC_MSG_SEQ_OFFSET] = icc_seq;
  if (icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_tx_data[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_tx_data[ICC_MSG_STATUS_OFFSET] = 0;
  icc_tx_data[ICC_MSG_STATUS_OFFSET] |= ICC_CMD_STATUS_ERROR; /* Failed */
  icc_tx_data[ICC_MSG_ERROR_OFFSET] = offset;
  icc_tx_data[ICC_MSG_CHAIN_OFFSET] = 0x00;

  if (!icc_tx_ready ())
    {
      DEBUG_INFO ("ERR0D\r\n");
    }
  else
    {
      icc_tx_size = ICC_MSG_HEADER_SIZE;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
    }
}

/* Send back ATR (Answer To Reset) */
enum icc_state
icc_power_on (void)
{
  int size_atr;

  size_atr = sizeof (ATR);

  icc_tx_data[0] = ICC_DATA_BLOCK_RET;
  icc_tx_data[1] = size_atr;
  icc_tx_data[2] = 0x00;
  icc_tx_data[3] = 0x00;
  icc_tx_data[4] = 0x00;
  icc_tx_data[5] = 0x00;	/* Slot */
  icc_tx_data[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_tx_data[ICC_MSG_STATUS_OFFSET] = 0x00;
  icc_tx_data[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_tx_data[ICC_MSG_CHAIN_OFFSET] = 0x00;
  memcpy (&icc_tx_data[ICC_MSG_DATA_OFFSET], ATR, size_atr);

  if (!icc_tx_ready ())
    {
      DEBUG_INFO ("ERR0B\r\n");
    }
  else
    {
      icc_tx_size = ICC_MSG_HEADER_SIZE + size_atr;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
      DEBUG_INFO ("ON\r\n");
    }

  return ICC_STATE_WAIT;
}

static void
icc_send_status (void)
{
  icc_tx_data[0] = ICC_SLOT_STATUS_RET;
  icc_tx_data[1] = 0x00;
  icc_tx_data[2] = 0x00;
  icc_tx_data[3] = 0x00;
  icc_tx_data[4] = 0x00;
  icc_tx_data[5] = 0x00;	/* Slot */
  icc_tx_data[ICC_MSG_SEQ_OFFSET] = icc_seq;
  if (icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_tx_data[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_tx_data[ICC_MSG_STATUS_OFFSET] = 0;
  icc_tx_data[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_tx_data[ICC_MSG_CHAIN_OFFSET] = 0x00;

  if (!icc_tx_ready ())
    {
      DEBUG_INFO ("ERR0C\r\n");
    }
  else
    {
      icc_tx_size = ICC_MSG_HEADER_SIZE;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
    }
#ifdef DEBUG_MORE
  DEBUG_INFO ("St\r\n");
#endif
}

enum icc_state
icc_power_off (void)
{
  icc_send_status ();
  DEBUG_INFO ("OFF\r\n");
  return ICC_STATE_START;
}

uint8_t cmd_APDU[MAX_CMD_APDU_SIZE];
uint8_t res_APDU[MAX_RES_APDU_SIZE];
int cmd_APDU_size;
int res_APDU_size;

static uint8_t *p_cmd;
static uint8_t *p_res;

static void
icc_send_data_block (uint8_t status, uint8_t error, uint8_t chain,
		     uint8_t *data, int len)
{
  icc_tx_data[0] = ICC_DATA_BLOCK_RET;
  icc_tx_data[1] = len & 0xFF;
  icc_tx_data[2] = (len >> 8)& 0xFF;
  icc_tx_data[3] = (len >> 16)& 0xFF;
  icc_tx_data[4] = (len >> 24)& 0xFF;
  icc_tx_data[5] = 0x00;	/* Slot */
  icc_tx_data[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_tx_data[ICC_MSG_STATUS_OFFSET] = status;
  icc_tx_data[ICC_MSG_ERROR_OFFSET] = error;
  icc_tx_data[ICC_MSG_CHAIN_OFFSET] = chain;
  if (len)
    memcpy (&icc_tx_data[ICC_MSG_DATA_OFFSET], data, len);

  if (!icc_tx_ready ())
    {				/* not ready to send */
      DEBUG_INFO ("ERR09\r\n");
    }
  else
    {
      icc_tx_size = ICC_MSG_HEADER_SIZE + len;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
#ifdef DEBUG_MORE
      DEBUG_INFO ("DATA\r\n");
#endif
    }
}


static void
icc_send_params (void)
{
  icc_tx_data[0] = ICC_PARAMS_RET;
  icc_tx_data[1] = 0x07;	/* Length = 0x00000007 */
  icc_tx_data[2] = 0;
  icc_tx_data[3] = 0;
  icc_tx_data[4] = 0;
  icc_tx_data[5] = 0x00;	/* Slot */
  icc_tx_data[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_tx_data[ICC_MSG_STATUS_OFFSET] = 0;
  icc_tx_data[ICC_MSG_ERROR_OFFSET] = 0;
  icc_tx_data[ICC_MSG_CHAIN_OFFSET] = 0x01; /* ProtocolNum: T=1 */
  icc_tx_data[ICC_MSG_DATA_OFFSET] = 0x11;   /* bmFindexDindex */
  icc_tx_data[ICC_MSG_DATA_OFFSET+1] = 0x11; /* bmTCCKST1 */
  icc_tx_data[ICC_MSG_DATA_OFFSET+2] = 0xFE; /* bGuardTimeT1 */
  icc_tx_data[ICC_MSG_DATA_OFFSET+3] = 0x55; /* bmWaitingIntegersT1 */
  icc_tx_data[ICC_MSG_DATA_OFFSET+4] = 0x03; /* bClockStop */
  icc_tx_data[ICC_MSG_DATA_OFFSET+5] = 0xFE; /* bIFSC */
  icc_tx_data[ICC_MSG_DATA_OFFSET+6] = 0; /* bNadValue */

  if (!icc_tx_ready ())
    {				/* not ready to send */
      DEBUG_INFO ("ERR09\r\n");
    }
  else
    {
      icc_tx_size = ICC_MSG_HEADER_SIZE + icc_header->data_len;
      USB_SIL_Write (EP1_IN, icc_tx_data, icc_tx_size);
      SetEPTxValid (ENDP1);
#ifdef DEBUG_MORE
      DEBUG_INFO ("DATA\r\n");
#endif
    }
}


static enum icc_state
icc_handle_data (void)
{
  enum icc_state next_state = icc_state;

  switch (icc_state)
    {
    case ICC_STATE_START:
      if (icc_header->msg_type == ICC_POWER_ON)
	next_state = icc_power_on ();
      else if (icc_header->msg_type == ICC_POWER_OFF)
	next_state = icc_power_off ();
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else
	{
	  DEBUG_INFO ("ERR01\r\n");
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_WAIT:
      if (icc_header->msg_type == ICC_POWER_ON)
	/* Not in the spec., but pcscd/libccid */
	next_state = icc_power_on ();
      else if (icc_header->msg_type == ICC_POWER_OFF)
	next_state = icc_power_off ();
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else if (icc_header->msg_type == ICC_XFR_BLOCK)
	{
	  if (icc_header->param == 0)
	    {			/* Give this message to GPG thread */
	      p_cmd = cmd_APDU;
	      memcpy (p_cmd, icc_data, icc_data_size);
	      cmd_APDU_size = icc_data_size;
	      chEvtSignal (gpg_thread, (eventmask_t)1);
	      next_state = ICC_STATE_EXECUTE;
	      chEvtSignal (blinker_thread, EV_LED_ON);
	    }
	  else if (icc_header->param == 1)
	    {
	      p_cmd = cmd_APDU;
	      memcpy (p_cmd, icc_data, icc_data_size);
	      p_cmd += icc_data_size;
	      cmd_APDU_size = icc_data_size;
	      icc_send_data_block (0, 0, 0x10, NULL, 0);
	      next_state = ICC_STATE_RECEIVE;
	    }
	  else
	    {
	      DEBUG_INFO ("ERR02\r\n");
	      icc_error (ICC_OFFSET_PARAM);
	    }
	}
      else if (icc_header->msg_type == ICC_SET_PARAMS
	       || icc_header->msg_type == ICC_GET_PARAMS)
	icc_send_params ();
      else
	{
	  DEBUG_INFO ("ERR03\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_EXECUTE:
      if (icc_header->msg_type == ICC_POWER_OFF)
	{
	  /* XXX: Kill GPG thread */
	  next_state = icc_power_off ();
	}
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else
	{
	  DEBUG_INFO ("ERR04\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_RECEIVE:
      if (icc_header->msg_type == ICC_POWER_OFF)
	next_state = icc_power_off ();
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else if (icc_header->msg_type == ICC_XFR_BLOCK)
	{
	  if (cmd_APDU_size + icc_data_size <= MAX_CMD_APDU_SIZE)
	    {
	      memcpy (p_cmd, icc_data, icc_data_size);
	      p_cmd += icc_data_size;
	      cmd_APDU_size += icc_data_size;

	      if (icc_header->param == 2) /* Got final block */
		{			/* Give this message to GPG thread */
		  next_state = ICC_STATE_EXECUTE;
		  chEvtSignal (blinker_thread, EV_LED_ON);
		  cmd_APDU_size = p_cmd - cmd_APDU;
		  chEvtSignal (gpg_thread, (eventmask_t)1);
		}
	      else if (icc_header->param == 3)
		icc_send_data_block (0, 0, 0x10, NULL, 0);
	      else
		{
		  DEBUG_INFO ("ERR08\r\n");
		  icc_error (ICC_OFFSET_PARAM);
		}
	    }
	  else			/* Overrun */
	    {
	      icc_send_data_block (ICC_CMD_STATUS_ERROR, ICC_ERROR_XFR_OVERRUN,
				   0, NULL, 0);
	      next_state = ICC_STATE_WAIT;
	    }
	}
      else
	{
	  DEBUG_INFO ("ERR05\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	  next_state = ICC_STATE_WAIT;
	}
      break;
    case ICC_STATE_SEND:
      if (icc_header->msg_type == ICC_POWER_OFF)
	next_state = icc_power_off ();
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else if (icc_header->msg_type == ICC_XFR_BLOCK)
	{
	  if (icc_header->param == 0x10)
	    {
	      if (res_APDU_size <= ICC_MAX_MSG_DATA_SIZE)
		{
		  icc_send_data_block (0, 0, 0x02, p_res, res_APDU_size);
		  next_state = ICC_STATE_WAIT;
		}
	      else
		{
		  icc_send_data_block (0, 0, 0x03,
				       p_res, ICC_MAX_MSG_DATA_SIZE);
		  p_res += ICC_MAX_MSG_DATA_SIZE;
		  res_APDU_size -= ICC_MAX_MSG_DATA_SIZE;
		}
	    }
	  else
	    {
	      DEBUG_INFO ("ERR0A\r\n");
	      DEBUG_BYTE (icc_header->param >> 8);
	      DEBUG_BYTE (icc_header->param & 0xff);
	      icc_error (ICC_OFFSET_PARAM);
	      next_state = ICC_STATE_WAIT;
	    }
	}
      else
	{
	  DEBUG_INFO ("ERR06\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	  next_state = ICC_STATE_WAIT;
	}
      break;
    }

  SetEPRxValid (ENDP2);
  return next_state;
}

static enum icc_state
icc_handle_timeout (void)
{
  enum icc_state next_state = icc_state;

  switch (icc_state)
    {
    case ICC_STATE_EXECUTE:
      icc_send_data_block (ICC_CMD_STATUS_TIMEEXT, 0, 0, NULL, 0);
      break;
    case ICC_STATE_RECEIVE:
    case ICC_STATE_SEND:
    case ICC_STATE_START:
    case ICC_STATE_WAIT:
      break;
    }

  return next_state;
}

#define USB_ICC_TIMEOUT MS2ST(1950)

msg_t
USBthread (void *arg)
{
  (void)arg;

  icc_thread = chThdSelf ();
  chEvtClear (ALL_EVENTS);

  icc_state = ICC_STATE_START;
  icc_tx_size = -1;

  while (1)
    {
      eventmask_t m;

      m = chEvtWaitOneTimeout (ALL_EVENTS, USB_ICC_TIMEOUT);

      if (m == EV_RX_DATA_READY)
	icc_state = icc_handle_data ();
      else if (m == EV_EXEC_FINISHED)
	{
	  if (icc_state == ICC_STATE_EXECUTE)
	    {
	      chEvtSignal (blinker_thread, EV_LED_OFF);

	      if (res_APDU_size <= ICC_MAX_MSG_DATA_SIZE)
		{
		  icc_send_data_block (0, 0, 0, res_APDU, res_APDU_size);
		  icc_state = ICC_STATE_WAIT;
		}
	      else
		{
		  p_res = res_APDU;
		  icc_send_data_block (0, 0, 0x01,
				       p_res, ICC_MAX_MSG_DATA_SIZE);
		  p_res += ICC_MAX_MSG_DATA_SIZE;
		  res_APDU_size -= ICC_MAX_MSG_DATA_SIZE;
		  icc_state = ICC_STATE_SEND;
		}
	    }
	  else
	    {			/* XXX: error */
	      DEBUG_INFO ("ERR07\r\n");
	    }
	}
      else			/* Timeout */
	icc_state = icc_handle_timeout ();
    }

  return 0;
}

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

extern void *memmove(void *dest, const void *src, size_t n);

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
#define ICC_MSG_DATA_OFFSET	10	/* == ICC_MSG_HEADER_SIZE */
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

static int icc_data_size;

/*
 * USB-ICC communication could be considered "half duplex".
 *
 * While the device is sending something, there is no possibility for
 * the device to receive anything.
 *
 * While the device is receiving something, there is no possibility
 * for the device to send anything.
 * 
 * Thus, the buffer can be shared for RX and TX.
 */

/*
 * Buffer of USB communication: for both of RX and TX
 *
 * The buffer will be filled by multiple RX transactions (Bulk-OUT)
 * or will be used for multiple TX transactions (Bulk-IN)
 */
uint8_t icc_buffer[USB_BUF_SIZE];
uint8_t icc_seq;

/*
 * Pointer to ICC_BUFFER
 */
static uint8_t *icc_next_p;

/*
 * Chain pointer: This implementation support two packets in chain (not more)
 */
static uint8_t *icc_chain_p;

/*
 * Whole size of TX transfer (Bulk-IN transactions)
 */
static int icc_tx_size;

Thread *icc_thread;

#define EV_RX_DATA_READY (eventmask_t)1  /* USB Rx data available  */
/* EV_EXEC_FINISHED == 2 */
#define EV_TX_FINISHED (eventmask_t)4  /* USB Tx finished  */

/*
 * Tx done callback
 */
void
EP1_IN_Callback (void)
{
  if (icc_next_p == NULL)
    /* The sequence of Bulk-IN transactions finished */
    chEvtSignalI (icc_thread, EV_TX_FINISHED);
  else if (icc_next_p == &icc_buffer[icc_tx_size])
    /* It was multiple of USB_LL_BUF_SIZE */
    {
      /* Send the last 0-DATA transcation of Bulk-IN in the transactions */
      icc_next_p = NULL;
      USB_SIL_Write (EP1_IN, icc_buffer, 0);
      SetEPTxValid (ENDP1);
    }
  else
    {
      int tx_size = USB_LL_BUF_SIZE;
      uint8_t *p = icc_next_p;

      icc_next_p += USB_LL_BUF_SIZE;
      if (icc_next_p > &icc_buffer[icc_tx_size])
	{
	  icc_next_p = NULL;
	  tx_size = &icc_buffer[icc_tx_size] - p;
	}

      USB_SIL_Write (EP1_IN, p, tx_size);
      SetEPTxValid (ENDP1);
    }
}

static void
icc_prepare_receive (int chain)
{
  if (chain)
    icc_next_p = icc_chain_p;
  else
    icc_next_p = icc_buffer;

  SetEPRxValid (ENDP2);
}

/*
 * Rx ready callback
 */
void
EP2_OUT_Callback (void)
{
  int len;

  len = USB_SIL_Read (EP2_OUT, icc_next_p);

  if (len == USB_LL_BUF_SIZE) /* The sequence of transactions continues */
    {
      icc_next_p += USB_LL_BUF_SIZE;
      SetEPRxValid (ENDP2);
      if ((icc_next_p - icc_buffer) >= USB_BUF_SIZE)
	/* No room to receive any more */
	{
	  DEBUG_INFO ("ERR0F\r\n");
	  icc_next_p -= USB_LL_BUF_SIZE; /* Just for not overrun the buffer */
	  /*
	   * Receive until the end of the sequence
	   * (and discard the whole block)
	   */
	}
    }
  else 				/* Finished */
    {
      struct icc_header *icc_header;
      int data_len;

      icc_next_p += len;
      if (icc_chain_p)
	{
	  icc_header = (struct icc_header *)icc_chain_p;
	  icc_data_size = (icc_next_p - icc_chain_p) - ICC_MSG_HEADER_SIZE;
	}
      else
	{
	  icc_header = (struct icc_header *)icc_buffer;
	  icc_data_size = (icc_next_p - icc_buffer) - ICC_MSG_HEADER_SIZE;
	}

      /* NOTE: We're little endian, nothing to convert */
      data_len = icc_header->data_len;
      icc_seq = icc_header->seq;

      if (icc_data_size != data_len)
	{
	  DEBUG_INFO ("ERR0E\r\n");
	  /* Ignore the whole block */
	  icc_chain_p = NULL;
	  icc_prepare_receive (0);
	}
      else
	/* Notify icc_thread */
	chEvtSignalI (icc_thread, EV_RX_DATA_READY);
    }
}

enum icc_state
{
  ICC_STATE_START,		/* Initial */
  ICC_STATE_WAIT,		/* Waiting APDU */
				/* Busy1, Busy2, Busy3, Busy5 */
  ICC_STATE_EXECUTE,		/* Busy4 */
  ICC_STATE_RECEIVE, /* APDU Received Partially */

  ICC_STATE_SEND,    /* APDU Sent Partially */  /* Not used */
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
  uint8_t *icc_reply;

  if (icc_chain_p)
    icc_reply = icc_chain_p;
  else
    icc_reply = icc_buffer;

  icc_reply[0] = ICC_SLOT_STATUS_RET; /* Any value should be OK */
  icc_reply[1] = 0x00;
  icc_reply[2] = 0x00;
  icc_reply[3] = 0x00;
  icc_reply[4] = 0x00;
  icc_reply[5] = 0x00;	/* Slot */
  icc_reply[ICC_MSG_SEQ_OFFSET] = icc_seq;
  if (icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 0;
  icc_reply[ICC_MSG_STATUS_OFFSET] |= ICC_CMD_STATUS_ERROR; /* Failed */
  icc_reply[ICC_MSG_ERROR_OFFSET] = offset;
  icc_reply[ICC_MSG_CHAIN_OFFSET] = 0x00;

  icc_next_p = NULL;	/* This is a single transaction Bulk-IN */
  icc_tx_size = ICC_MSG_HEADER_SIZE;
  USB_SIL_Write (EP1_IN, icc_reply, icc_tx_size);
  SetEPTxValid (ENDP1);
}

/* Send back ATR (Answer To Reset) */
enum icc_state
icc_power_on (void)
{
  int size_atr;

  size_atr = sizeof (ATR);
  icc_buffer[0] = ICC_DATA_BLOCK_RET;
  icc_buffer[1] = size_atr;
  icc_buffer[2] = 0x00;
  icc_buffer[3] = 0x00;
  icc_buffer[4] = 0x00;
  icc_buffer[5] = 0x00;	/* Slot */
  icc_buffer[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_buffer[ICC_MSG_STATUS_OFFSET] = 0x00;
  icc_buffer[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_buffer[ICC_MSG_CHAIN_OFFSET] = 0x00;
  memcpy (&icc_buffer[ICC_MSG_DATA_OFFSET], ATR, size_atr);

  icc_next_p = NULL;	/* This is a single transaction Bulk-IN */
  icc_tx_size = ICC_MSG_HEADER_SIZE + size_atr;
  USB_SIL_Write (EP1_IN, icc_buffer, icc_tx_size);
  SetEPTxValid (ENDP1);
  DEBUG_INFO ("ON\r\n");

  return ICC_STATE_WAIT;
}

static void
icc_send_status (void)
{
  uint8_t *icc_reply;

  if (icc_chain_p)
    icc_reply = icc_chain_p;
  else
    icc_reply = icc_buffer;

  icc_reply[0] = ICC_SLOT_STATUS_RET;
  icc_reply[1] = 0x00;
  icc_reply[2] = 0x00;
  icc_reply[3] = 0x00;
  icc_reply[4] = 0x00;
  icc_reply[5] = 0x00;	/* Slot */
  icc_reply[ICC_MSG_SEQ_OFFSET] = icc_seq;
  if (icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 0;
  icc_reply[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_reply[ICC_MSG_CHAIN_OFFSET] = 0x00;

  icc_next_p = NULL;	/* This is a single transaction Bulk-IN */
  icc_tx_size = ICC_MSG_HEADER_SIZE;
  USB_SIL_Write (EP1_IN, icc_reply, icc_tx_size);
  SetEPTxValid (ENDP1);

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

int res_APDU_size;

static void
icc_send_data_block_filling_header (int len)
{
  int tx_size = USB_LL_BUF_SIZE;
  uint8_t *p = icc_buffer;

  icc_buffer[0] = ICC_DATA_BLOCK_RET;
  icc_buffer[1] = len & 0xFF;
  icc_buffer[2] = (len >> 8)& 0xFF;
  icc_buffer[3] = (len >> 16)& 0xFF;
  icc_buffer[4] = (len >> 24)& 0xFF;
  icc_buffer[5] = 0x00;	/* Slot */
  icc_buffer[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_buffer[ICC_MSG_STATUS_OFFSET] = 0;
  icc_buffer[ICC_MSG_ERROR_OFFSET] = 0;
  icc_buffer[ICC_MSG_CHAIN_OFFSET] = 0;

  icc_tx_size = ICC_MSG_HEADER_SIZE + len;
  icc_next_p = icc_buffer + USB_LL_BUF_SIZE;
  if (icc_next_p > &icc_buffer[icc_tx_size])
    {
      icc_next_p = NULL;
      tx_size = &icc_buffer[icc_tx_size] - p;
    }

  USB_SIL_Write (EP1_IN, p, tx_size);
  SetEPTxValid (ENDP1);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}

static void
icc_send_data_block (uint8_t status, uint8_t chain)
{
  uint8_t *icc_reply;

  if (icc_chain_p)
    icc_reply = icc_chain_p;
  else
    icc_reply = icc_buffer;

  icc_reply[0] = ICC_DATA_BLOCK_RET;
  icc_reply[1] = 0x00;
  icc_reply[2] = 0x00;
  icc_reply[3] = 0x00;
  icc_reply[4] = 0x00;
  icc_reply[5] = 0x00;	/* Slot */
  icc_reply[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_reply[ICC_MSG_STATUS_OFFSET] = status;
  icc_reply[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_reply[ICC_MSG_CHAIN_OFFSET] = chain;

  icc_next_p = NULL;	/* This is a single transaction Bulk-IN */
  icc_tx_size = ICC_MSG_HEADER_SIZE;
  USB_SIL_Write (EP1_IN, icc_reply, icc_tx_size);
  SetEPTxValid (ENDP1);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}


static void
icc_send_params (void)
{
  icc_buffer[0] = ICC_PARAMS_RET;
  icc_buffer[1] = 0x07;	/* Length = 0x00000007 */
  icc_buffer[2] = 0;
  icc_buffer[3] = 0;
  icc_buffer[4] = 0;
  icc_buffer[5] = 0x00;	/* Slot */
  icc_buffer[ICC_MSG_SEQ_OFFSET] = icc_seq;
  icc_buffer[ICC_MSG_STATUS_OFFSET] = 0;
  icc_buffer[ICC_MSG_ERROR_OFFSET] = 0;
  icc_buffer[ICC_MSG_CHAIN_OFFSET] = 0x01;  /* ProtocolNum: T=1 */
  icc_buffer[ICC_MSG_DATA_OFFSET] = 0x11;   /* bmFindexDindex */
  icc_buffer[ICC_MSG_DATA_OFFSET+1] = 0x11; /* bmTCCKST1 */
  icc_buffer[ICC_MSG_DATA_OFFSET+2] = 0xFE; /* bGuardTimeT1 */
  icc_buffer[ICC_MSG_DATA_OFFSET+3] = 0x55; /* bmWaitingIntegersT1 */
  icc_buffer[ICC_MSG_DATA_OFFSET+4] = 0x03; /* bClockStop */
  icc_buffer[ICC_MSG_DATA_OFFSET+5] = 0xFE; /* bIFSC */
  icc_buffer[ICC_MSG_DATA_OFFSET+6] = 0;    /* bNadValue */

  icc_next_p = NULL;	/* This is a single transaction Bulk-IN */
  icc_tx_size = ICC_MSG_HEADER_SIZE + 7;
  USB_SIL_Write (EP1_IN, icc_buffer, icc_tx_size);
  SetEPTxValid (ENDP1);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}

static enum icc_state
icc_handle_data (void)
{
  enum icc_state next_state = icc_state;
  struct icc_header *icc_header;

  if (icc_chain_p)
    icc_header = (struct icc_header *)icc_chain_p;
  else
    icc_header = (struct icc_header *)icc_buffer;

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
	      chEvtSignal (gpg_thread, (eventmask_t)1);
	      next_state = ICC_STATE_EXECUTE;
	      chEvtSignal (blinker_thread, EV_LED_ON);
	    }
	  else if (icc_header->param == 1)
	    {
	      icc_chain_p = icc_next_p;
	      icc_send_data_block (0, 0x10);
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
    case ICC_STATE_RECEIVE:
      if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else if (icc_header->msg_type == ICC_XFR_BLOCK)
	{
	  if (icc_header->param == 2) /* Got the final block */
	    {			/* Give this message to GPG thread */
	      int len = icc_next_p - icc_chain_p - ICC_MSG_HEADER_SIZE;

	      memmove (icc_chain_p, icc_chain_p + ICC_MSG_HEADER_SIZE, len);
	      icc_next_p -= ICC_MSG_HEADER_SIZE;
	      icc_data_size = icc_next_p - icc_buffer - ICC_MSG_HEADER_SIZE;
	      icc_chain_p = NULL;
	      next_state = ICC_STATE_EXECUTE;
	      chEvtSignal (blinker_thread, EV_LED_ON);
	      chEvtSignal (gpg_thread, (eventmask_t)1);
	    }
	  else			/* icc_header->param == 3 is not supported. */
	    {
	      DEBUG_INFO ("ERR08\r\n");
	      icc_error (ICC_OFFSET_PARAM);
	    }
	}
      else
	{
	  DEBUG_INFO ("ERR05\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_chain_p = NULL;
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	  next_state = ICC_STATE_WAIT;
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
    default:
      next_state = ICC_STATE_START;
      DEBUG_INFO ("ERR10\r\n");
      break;
    }

  return next_state;
}

static enum icc_state
icc_handle_timeout (void)
{
  enum icc_state next_state = icc_state;

  switch (icc_state)
    {
    case ICC_STATE_EXECUTE:
      icc_send_data_block (ICC_CMD_STATUS_TIMEEXT, 0);
      break;
    default:
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

  icc_prepare_receive (0);
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

	      icc_send_data_block_filling_header (res_APDU_size);
	      icc_state = ICC_STATE_WAIT;
	    }
	  else
	    {			/* XXX: error */
	      DEBUG_INFO ("ERR07\r\n");
	    }
	}
      else if (m == EV_TX_FINISHED)
	{
	  if (icc_state == ICC_STATE_START || icc_state == ICC_STATE_WAIT)
	    icc_prepare_receive (0);
	  else if (icc_state == ICC_STATE_RECEIVE)
	    icc_prepare_receive (1);
	}
      else			/* Timeout */
	icc_state = icc_handle_timeout ();
    }

  return 0;
}

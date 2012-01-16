/*
 * usb-icc.c -- USB CCID/ICCD protocol handling
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

#include "config.h"
#include "ch.h"
#include "hal.h"
#include "gnuk.h"
#include "usb_lib.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_istr.h"
#include "usb_lld.h"

#define ICC_SET_PARAMS		0x61 /* non-ICCD command  */
#define ICC_POWER_ON		0x62
#define ICC_POWER_OFF		0x63
#define ICC_SLOT_STATUS		0x65 /* non-ICCD command */
#define ICC_SECURE		0x69 /* non-ICCD command */
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

int icc_data_size;

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

static Thread *icc_thread;

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
      usb_lld_write (ENDP1, icc_buffer, 0);
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

      usb_lld_write (ENDP1, p, tx_size);
    }
}

static void
icc_prepare_receive (int chain)
{
  if (chain)
    icc_next_p = icc_chain_p;
  else
    icc_next_p = icc_buffer;

  usb_lld_rx_enable (ENDP2);
}

/*
 * Rx ready callback
 */
void
EP2_OUT_Callback (void)
{
  int len;
  struct icc_header *icc_header;
  int data_len_so_far;
  int data_len;

  len = usb_lld_get_data_len (ENDP2);
  usb_lld_rxcpy (icc_next_p, ENDP2, 0, len);
  if (len == 0)
    {		    /* Just ignore Zero Length Packet (ZLP), if any */
      usb_lld_rx_enable (ENDP2);
      return;
    }

  icc_next_p += len;

  if (icc_chain_p)
    icc_header = (struct icc_header *)icc_chain_p;
  else
    icc_header = (struct icc_header *)icc_buffer;

  data_len = icc_header->data_len; /* NOTE: We're little endian */
  data_len_so_far = (icc_next_p - (uint8_t *)icc_header) - ICC_MSG_HEADER_SIZE;

  if (len == USB_LL_BUF_SIZE
      && data_len != data_len_so_far)
    /* The sequence of transactions continues */
    {
      usb_lld_rx_enable (ENDP2);
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

      /*
       * NOTE: It is possible a transaction may stall when the size of
       * BULK_OUT transaction it's bigger than USB_BUF_SIZE and stops
       * with just USB_LL_BUF_SIZE packet.  Device will remain waiting
       * another packet.
       */
    }
  else 				/* Finished */
    {
      icc_data_size = data_len_so_far;
      icc_seq = icc_header->seq; /* NOTE: We're little endian */

      if (icc_data_size != data_len)
	{
	  DEBUG_INFO ("ERR0E\r\n");
	  /* Ignore the whole block */
	  icc_chain_p = NULL;
	  icc_prepare_receive (0);
	}
      else
	/* Notify myself */
	chEvtSignalI (icc_thread, EV_RX_DATA_READY);
    }
}

volatile enum icc_state icc_state;

/*
 * ATR (Answer To Reset) string
 *
 * TS = 0x3b: Direct conversion
 * T0 = 0xda: TA1, TC1 and TD1 follow, 10 historical bytes
 * TA1 = 0x11: FI=1, DI=1
 * TC1 = 0xff
 * TD1 = 0x81: TD2 follows, T=1
 * TD2 = 0xb1: TA3, TB3 and TD3 follow, T=1
 * TA3 = 0xFE: IFSC = 254 bytes
 * TB3 = 0x55: BWI = 5, CWI = 5   (BWT timeout 3.2 sec)
 * TD3 = 0x1f: TA4 follows, T=15
 * TA4 = 0x03: 5V or 3.3V
 * Historical bytes: to be explained...
 * XOR check
 *
 * Minimum: 0x3b, 0x8a, 0x80, 0x01, + historical bytes, xor check
 *
 */
static const char ATR[] = {
  0x3b, 0xda, 0x11, 0xff, 0x81, 0xb1, 0xfe, 0x55, 0x1f, 0x03,
  0x00,
	0x31, 0x84, /* full DF name, GET DATA, MF */
        0x73,
              0x80,		/* DF full name */
  	      0x01,		/* 1-byte */
  	      0x40,		/* Extended Lc and extended Le */
  	0x00,
        0x90, 0x00,
 (0xda^0x11^0xff^0x81^0xb1^0xfe^0x55^0x1f^0x03
  ^0x00^0x31^0x84^0x73^0x80^0x01^0x40^0x00^0x90^0x00)
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
  usb_lld_write (ENDP1, icc_reply, icc_tx_size);
}

static Thread *gpg_thread;
static WORKING_AREA(waGPGthread, 128*16);
extern msg_t GPGthread (void *arg);


/* Send back ATR (Answer To Reset) */
enum icc_state
icc_power_on (void)
{
  int size_atr;

  if (gpg_thread == NULL)
    gpg_thread = chThdCreateStatic (waGPGthread, sizeof(waGPGthread),
				    NORMALPRIO, GPGthread, (void *)icc_thread);

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
  usb_lld_write (ENDP1, icc_buffer, icc_tx_size);
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
  usb_lld_write (ENDP1, icc_reply, icc_tx_size);

#ifdef DEBUG_MORE
  DEBUG_INFO ("St\r\n");
#endif
}

enum icc_state
icc_power_off (void)
{
  icc_data_size = 0;

  if (gpg_thread)
    {
      chThdTerminate (gpg_thread);
      chEvtSignal (gpg_thread, EV_NOP);
      chThdWait (gpg_thread);
      gpg_thread = NULL;
    }

  icc_state = ICC_STATE_START;	/* This status change should be here */
  icc_send_status ();
  DEBUG_INFO ("OFF\r\n");
  return ICC_STATE_START;
}

int res_APDU_size;
const uint8_t *res_APDU_pointer;

static void
icc_send_data_block (int len, uint8_t status, uint8_t chain)
{
  int tx_size = USB_LL_BUF_SIZE;
  uint8_t *p;

  if (icc_chain_p)
    p = icc_chain_p;
  else
    p = icc_buffer;

  p[0] = ICC_DATA_BLOCK_RET;
  p[1] = len & 0xFF;
  p[2] = (len >> 8)& 0xFF;
  p[3] = (len >> 16)& 0xFF;
  p[4] = (len >> 24)& 0xFF;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = icc_seq;
  p[ICC_MSG_STATUS_OFFSET] = status;
  p[ICC_MSG_ERROR_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET] = chain;

  icc_tx_size = ICC_MSG_HEADER_SIZE + len;
  if (icc_tx_size < USB_LL_BUF_SIZE)
    {
      icc_next_p = NULL;
      tx_size = icc_tx_size;
    }
  else
    icc_next_p = p + USB_LL_BUF_SIZE;

  usb_lld_write (ENDP1, p, tx_size);
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
  usb_lld_write (ENDP1, icc_buffer, icc_tx_size);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}

/* Supporting smaller buffer of libccid (<= 1.3.11) */
#define ICC_RESPONSE_MSG_DATA_SIZE 262

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
	      chEvtSignal (gpg_thread, EV_CMD_AVAILABLE);
	      next_state = ICC_STATE_EXECUTE;
	    }
	  else if (icc_header->param == 1)
	    {
	      icc_chain_p = icc_next_p;
	      icc_send_data_block (0, 0, 0x10);
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
      else if (icc_header->msg_type == ICC_SECURE)
	{
	  if (icc_buffer[10] == 0x00) /* PIN verification */
	    {
	      cmd_APDU[0] = icc_buffer[25];
	      cmd_APDU[1] = icc_buffer[26];
	      cmd_APDU[2] = icc_buffer[27];
	      cmd_APDU[3] = icc_buffer[28];
	      icc_data_size = 4;
	      cmd_APDU[4] = 0; /* bConfirmPIN */
	      cmd_APDU[5] = icc_buffer[17]; /* bEntryValidationCondition */
	      cmd_APDU[6] = icc_buffer[18]; /* bNumberMessage */
	      cmd_APDU[7] = icc_buffer[19]; /* wLangId L */
	      cmd_APDU[8] = icc_buffer[20]; /* wLangId H */
	      cmd_APDU[9] = icc_buffer[21]; /* bMsgIndex, bMsgIndex1 */
	      cmd_APDU[10] = 0; /* bMsgIndex2 */
	      cmd_APDU[11] = 0; /* bMsgIndex3 */
	      chEvtSignal (gpg_thread, EV_VERIFY_CMD_AVAILABLE);
	      next_state = ICC_STATE_EXECUTE;
	    }
	  else if (icc_buffer[10] == 0x01) /* PIN Modification */
	    {
	      uint8_t num_msgs = icc_buffer[21];

	      if (num_msgs == 0x00)
		num_msgs = 1;
	      else if (num_msgs == 0xff)
		num_msgs = 3;
	      cmd_APDU[0] = icc_buffer[27 + num_msgs];
	      cmd_APDU[1] = icc_buffer[28 + num_msgs];
	      cmd_APDU[2] = icc_buffer[29 + num_msgs];
	      cmd_APDU[3] = icc_buffer[30 + num_msgs];
	      icc_data_size = 4;
	      cmd_APDU[4] = icc_buffer[19]; /* bConfirmPIN */
	      cmd_APDU[5] = icc_buffer[20]; /* bEntryValidationCondition */
	      cmd_APDU[6] = icc_buffer[21]; /* bNumberMessage */
	      cmd_APDU[7] = icc_buffer[22]; /* wLangId L */
	      cmd_APDU[8] = icc_buffer[23]; /* wLangId H */
	      cmd_APDU[9] = icc_buffer[24]; /* bMsgIndex, bMsgIndex1 */
	      cmd_APDU[10] = cmd_APDU[11] = 0;
	      if (num_msgs >= 2)
		cmd_APDU[10] = icc_buffer[25]; /* bMsgIndex2 */
	      if (num_msgs == 3)
		cmd_APDU[11] = icc_buffer[26]; /* bMsgIndex3 */
	      chEvtSignal (gpg_thread, EV_MODIFY_CMD_AVAILABLE);
	      next_state = ICC_STATE_EXECUTE;
	    }
	  else
	    icc_error (ICC_MSG_DATA_OFFSET);
	}
      else
	{
	  DEBUG_INFO ("ERR03\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_RECEIVE:
      if (icc_header->msg_type == ICC_POWER_OFF)
	{
	  icc_chain_p = NULL;
	  next_state = icc_power_off ();
	}
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
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
	      chEvtSignal (gpg_thread, EV_CMD_AVAILABLE);
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
	next_state = icc_power_off ();
      else if (icc_header->msg_type == ICC_SLOT_STATUS)
	icc_send_status ();
      else
	{
	  DEBUG_INFO ("ERR04\r\n");
	  DEBUG_BYTE (icc_header->msg_type);
	  icc_error (ICC_OFFSET_CMD_NOT_SUPPORTED);
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
	      if (res_APDU_pointer != NULL)
		{
		  memcpy (res_APDU, res_APDU_pointer,
			  ICC_RESPONSE_MSG_DATA_SIZE);
		  res_APDU_pointer += ICC_RESPONSE_MSG_DATA_SIZE;
		}
	      else
		memmove (res_APDU, res_APDU+ICC_RESPONSE_MSG_DATA_SIZE,
			 res_APDU_size);

	      if (res_APDU_size <= ICC_RESPONSE_MSG_DATA_SIZE)
		{
		  icc_send_data_block (res_APDU_size, 0, 0x02);
		  next_state = ICC_STATE_WAIT;
		}
	      else
		{
		  icc_send_data_block (ICC_RESPONSE_MSG_DATA_SIZE, 0, 0x03);
		  res_APDU_size -= ICC_RESPONSE_MSG_DATA_SIZE;
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
      icc_send_data_block (0, ICC_CMD_STATUS_TIMEEXT, 0);
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
	if (icc_state == ICC_STATE_EXECUTE)
	  {
	    if (res_APDU_pointer != NULL)
	      {
		memcpy (res_APDU, res_APDU_pointer, ICC_RESPONSE_MSG_DATA_SIZE);
		res_APDU_pointer += ICC_RESPONSE_MSG_DATA_SIZE;
	      }

	    if (res_APDU_size <= ICC_RESPONSE_MSG_DATA_SIZE)
	      {
		icc_send_data_block (res_APDU_size, 0, 0);
		icc_state = ICC_STATE_WAIT;
	      }
	    else
	      {
		icc_send_data_block (ICC_RESPONSE_MSG_DATA_SIZE, 0, 0x01);
		res_APDU_size -= ICC_RESPONSE_MSG_DATA_SIZE;
		icc_state = ICC_STATE_SEND;
	      }
	  }
	else
	  {			/* XXX: error */
	    DEBUG_INFO ("ERR07\r\n");
	  }
      else if (m == EV_TX_FINISHED)
	{
	  if (icc_state == ICC_STATE_START || icc_state == ICC_STATE_WAIT
	      || icc_state == ICC_STATE_SEND)
	    icc_prepare_receive (0);
	  else if (icc_state == ICC_STATE_RECEIVE)
	    icc_prepare_receive (1);
	}
      else			/* Timeout */
	icc_state = icc_handle_timeout ();
    }

  return 0;
}

/*
 * usb-icc.c -- USB CCID/ICCD protocol handling
 *
 * Copyright (C) 2010, 2011, 2012 Free Software Initiative of Japan
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
#include "usb_lld.h"

/*
 * USB buffer size of USB-ICC driver
 */
#if MAX_RES_APDU_DATA_SIZE > MAX_CMD_APDU_DATA_SIZE
#define USB_BUF_SIZE (MAX_RES_APDU_DATA_SIZE+5)
#else
#define USB_BUF_SIZE (MAX_CMD_APDU_DATA_SIZE+5)
#endif

struct apdu apdu;

/*
 * There are three layers in USB CCID implementation
 *
 *   +-------------------+
 *   | Application Layer |
 *   +-------------------+
 *      ^ command APDU |
 *      |              v response APDU
 *   +-------------------+
 *   |    CCID Layer     |
 *   +-------------------+
 *    ^ CCID PC_to_RDR  | CCID RDR_to_PC
 *    | Message         v Message
 *   +-------------------+
 *   |    USB Layer      |
 *   +-------------------+
 *    ^ USB             | USB
 *    | Bulk-OUT Packet v Bulk-IN Packet
 *
 */

/*
 * USB layer data structures
 */

struct ep_in {
  uint8_t ep_num;
  uint8_t tx_done;
  void (*notify) (struct ep_in *epi);
  const uint8_t *buf;
  size_t cnt;
  size_t buf_len;
  void *priv;
  void (*next_buf) (struct ep_in *epi, size_t len);
};

static void epi_init (struct ep_in *epi, int ep_num,
		      void (*notify) (struct ep_in *epi), void *priv)
{
  epi->ep_num = ep_num;
  epi->tx_done = 0;
  epi->notify = notify;
  epi->buf = NULL;
  epi->cnt = 0;
  epi->buf_len = 0;
  epi->priv = priv;
  epi->next_buf = NULL;
}

struct ep_out {
  uint8_t ep_num;
  uint8_t err;
  void (*notify) (struct ep_out *epo);
  uint8_t *buf;
  size_t cnt;
  size_t buf_len;
  void *priv;
  void (*next_buf) (struct ep_out *epo, size_t len);
  int  (*end_rx) (struct ep_out *epo, size_t orig_len);
};

static struct ep_out endpoint_out;
static struct ep_in endpoint_in;

static void epo_init (struct ep_out *epo, int ep_num,
		      void (*notify) (struct ep_out *epo), void *priv)
{
  epo->ep_num = ep_num;
  epo->err = 0;
  epo->notify = notify;
  epo->buf = NULL;
  epo->cnt = 0;
  epo->buf_len = 0;
  epo->priv = priv;
  epo->next_buf = NULL;
  epo->end_rx = NULL;
}

/*
 * CCID Layer
 */

/*
 * Buffer of USB communication: for both of RX and TX
 *
 * The buffer will be filled by multiple RX packets (Bulk-OUT)
 * or will be used for multiple TX packets (Bulk-IN)
 */
static uint8_t icc_buffer[USB_BUF_SIZE];

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
#define ICC_MAX_MSG_DATA_SIZE	USB_BUF_SIZE

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
#define ICC_OFFSET_DATA_LEN 1
#define ICC_OFFSET_PARAM 8

struct icc_header {
  uint8_t msg_type;
  uint32_t data_len;
  uint8_t slot;
  uint8_t seq;
  uint8_t rsvd;
  uint16_t param;
} __attribute__((packed));


enum icc_state *icc_state_p;

/* Data structure handled by CCID layer */
struct ccid {
  enum icc_state icc_state;
  uint8_t state;
  uint8_t *p;
  size_t len;

  uint8_t err;

  struct icc_header icc_header;

  uint8_t sw1sw2[2];
  uint8_t chained_cls_ins_p1_p2[4];

  Thread *icc_thread;
  Thread *application;

  /* lower layer */
  struct ep_out *epo;
  struct ep_in *epi;

  /* upper layer */
  struct apdu *a;
};

/*
 * APDU_STATE_WAIT_COMMAND           +---------+
 *        |           |              |         |
 *        |           v              v         |
 *        |      APDU_STATE_COMMAND_CHAINING --+
 *        |                 |
 *        v                 v
 *      APDU_STATE_COMMAND_RECEIVED
 *                  |
 *                  v
 *          ===================
 *          | Process COMMAND |
 *          ===================
 *                  |
 *                  v
 *            +-----+----------+                  +---------+
 *            |                |                  |         |
 *            v                v                  v         |
 * APDU_STATE_RESULT <---- APDU_STATE_RESULT_GET_RESPONSE --+
 *         |
 *         |
 *         v
 * APDU_STATE_WAIT_COMMAND
 */

#define APDU_STATE_WAIT_COMMAND        0
#define APDU_STATE_COMMAND_CHAINING    1
#define APDU_STATE_COMMAND_RECEIVED    2
#define APDU_STATE_RESULT              3
#define APDU_STATE_RESULT_GET_RESPONSE 4

static void ccid_reset (struct ccid *c)
{
  c->err = 0;
  c->state = APDU_STATE_WAIT_COMMAND;
  c->p = c->a->cmd_apdu_data;
  c->len = MAX_CMD_APDU_DATA_SIZE;
  c->a->cmd_apdu_data_len = 0;
  c->a->expected_res_size = 0;
}

static void ccid_init (struct ccid *c, struct ep_in *epi, struct ep_out *epo,
		       struct apdu *a, Thread *t)
{
  icc_state_p = &c->icc_state;

  c->icc_state = ICC_STATE_START;
  c->state = APDU_STATE_WAIT_COMMAND;
  /*
   * Note: a is not yet initialized yet, we can't use c->a->cmd_apdu_data here.
   */
  c->p = &icc_buffer[5];
  c->len = MAX_CMD_APDU_DATA_SIZE;
  c->err = 0;
  memset (&c->icc_header, 0, sizeof (struct icc_header));
  c->sw1sw2[0] = 0x90;
  c->sw1sw2[1] = 0x00;
  c->icc_thread = t;
  c->application = NULL;
  c->epi = epi;
  c->epo = epo;
  c->a = a;
}

/*
 * Application layer
 */

/*
 * USB-CCID communication could be considered "half duplex".
 *
 * While the device is sending something, there is no possibility for
 * the device to receive anything.
 *
 * While the device is receiving something, there is no possibility
 * for the device to send anything.
 *
 * Thus, the buffer can be shared for RX and TX.
 *
 * Exception: When we support ABORT of CCID, it is possible to receive
 * ABORT Class Specific Request to control pipe while we are
 * receiving/sending something at OUT/IN endpoint.
 *
 */

#define CMD_APDU_HEAD_SIZE 5

static void apdu_init (struct apdu *a)
{
  a->seq = 0;			/* will be set by lower layer */
  a->cmd_apdu_head = &icc_buffer[0];
  a->cmd_apdu_data = &icc_buffer[5];
  a->cmd_apdu_data_len = 0;	/* will be set by lower layer */
  a->expected_res_size = 0;	/* will be set by lower layer */

  a->sw = 0x9000;		     /* will be set by upper layer */
  a->res_apdu_data = &icc_buffer[5]; /* will be set by upper layer */
  a->res_apdu_data_len = 0;	     /* will be set by upper layer */
}

#define EV_RX_DATA_READY (eventmask_t)1  /* USB Rx data available  */
/* EV_EXEC_FINISHED == 2 */
#define EV_TX_FINISHED (eventmask_t)4  /* USB Tx finished  */


static void notify_tx (struct ep_in *epi)
{
  struct ccid *c = (struct ccid *)epi->priv;

  /* The sequence of Bulk-IN transactions finished */
  chEvtSignalI (c->icc_thread, EV_TX_FINISHED);
}

static void no_buf (struct ep_in *epi, size_t len)
{
  (void)len;
  epi->buf = NULL;
  epi->cnt = 0;
  epi->buf_len = 0;
}

static void set_sw1sw2 (struct ep_in *epi)
{
  struct ccid *c = (struct ccid *)epi->priv;

  if (c->a->expected_res_size >= c->len)
    {
      c->sw1sw2[0] = 0x90;
      c->sw1sw2[1] = 0x00;
    }
  else
    {
      c->sw1sw2[0] = 0x61;
      if (c->len >= 256)
	c->sw1sw2[1] = 0;
      else
	c->sw1sw2[1] = (uint8_t)c->len;
    }
}

static void get_sw1sw2 (struct ep_in *epi, size_t len)
{
  struct ccid *c = (struct ccid *)epi->priv;

  (void)len;
  epi->buf = c->sw1sw2;
  epi->cnt = 0;
  epi->buf_len = 2;
  epi->next_buf = no_buf;
}


/*
 * Tx done callback
 */
void
EP1_IN_Callback (void)
{
  struct ep_in *epi = &endpoint_in;

  if (epi->buf == NULL)
    if (epi->tx_done)
      epi->notify (epi);
    else
      {
	epi->tx_done = 1;
	usb_lld_tx_enable (epi->ep_num, 0); /* send ZLP */
      }
  else
    {
      int tx_size = 0;
      size_t remain = USB_LL_BUF_SIZE;
      int offset = 0;

      while (epi->buf)
	if (epi->buf_len < remain)
	  {
	    usb_lld_txcpy (epi->buf, epi->ep_num, offset, epi->buf_len);
	    offset += epi->buf_len;
	    remain -= epi->buf_len;
	    tx_size += epi->buf_len;
	    epi->next_buf (epi, remain); /* Update epi->buf, cnt, buf_len */
	  }
	else
	  {
	    usb_lld_txcpy (epi->buf, epi->ep_num, offset, remain);
	    epi->buf += remain;
	    epi->cnt += remain;
	    epi->buf_len -= remain;
	    tx_size += remain;
	    break;
	  }

      if (tx_size < USB_LL_BUF_SIZE)
	epi->tx_done = 1;
      usb_lld_tx_enable (epi->ep_num, tx_size);
    }
}


static void notify_icc (struct ep_out *epo)
{
  struct ccid *c = (struct ccid *)epo->priv;

  c->err = epo->err;
  chEvtSignalI (c->icc_thread, EV_RX_DATA_READY);
}

static int end_icc_rx (struct ep_out *epo, size_t orig_len)
{
  (void)orig_len;
  if (epo->cnt < sizeof (struct icc_header))
    /* short packet, just ignore */
    return 1;

  /* icc message with no abdata */
  return 0;
}

static int end_abdata (struct ep_out *epo, size_t orig_len)
{
  struct ccid *c = (struct ccid *)epo->priv;
  size_t len = epo->cnt;

  if (orig_len == USB_LL_BUF_SIZE && len < c->icc_header.data_len)
    /* more packet comes */
    return 1;

  if (len != c->icc_header.data_len)
    epo->err = 1;

  return 0;
}

static int end_cmd_apdu_head (struct ep_out *epo, size_t orig_len)
{
  struct ccid *c = (struct ccid *)epo->priv;

  (void)orig_len;

  if (epo->cnt < 4 || epo->cnt != c->icc_header.data_len)
    {
      epo->err = 1;
      return 0;
    }

  if ((c->state == APDU_STATE_COMMAND_CHAINING)
      && (c->chained_cls_ins_p1_p2[0] != (c->a->cmd_apdu_head[0] & ~0x10)
	  || c->chained_cls_ins_p1_p2[1] != c->a->cmd_apdu_head[1]
	  || c->chained_cls_ins_p1_p2[2] != c->a->cmd_apdu_head[2]
	  || c->chained_cls_ins_p1_p2[3] != c->a->cmd_apdu_head[3]))
    /*
     * Handling exceptional request.
     *
     * Host stops sending command APDU using command chaining,
     * and start another command APDU.
     *
     * Discard old one, and start handling new one.
     */
    {
      c->state = APDU_STATE_WAIT_COMMAND;
      c->p = c->a->cmd_apdu_data;
      c->len = MAX_CMD_APDU_DATA_SIZE;
    }

  if (epo->cnt == 4)
    {
      /* No Lc and Le */
      c->a->cmd_apdu_data_len = 0;
      c->a->expected_res_size = 0;
    }
  else if (epo->cnt == 5)
    {
      /* No Lc but Le */
      c->a->cmd_apdu_data_len = 0;
      c->a->expected_res_size = c->a->cmd_apdu_head[4];
      if (c->a->expected_res_size == 0)
	c->a->expected_res_size = 256;
      c->a->cmd_apdu_head[4] = 0;
    }

  return 0;
}


static int end_nomore_data (struct ep_out *epo, size_t orig_len)
{
  (void)epo;
  if (orig_len == USB_LL_BUF_SIZE)
    return 1;
  else
    return 0;
}


static int end_cmd_apdu_data (struct ep_out *epo, size_t orig_len)
{
  struct ccid *c = (struct ccid *)epo->priv;
  size_t len = epo->cnt;

  if (orig_len == USB_LL_BUF_SIZE
      && CMD_APDU_HEAD_SIZE + len < c->icc_header.data_len)
    /* more packet comes */
    return 1;

  if (CMD_APDU_HEAD_SIZE + len != c->icc_header.data_len)
    goto error;

  if (len == c->a->cmd_apdu_head[4])
    /* No Le field*/
    c->a->expected_res_size = 0;
  else if (len == (size_t)c->a->cmd_apdu_head[4] + 1)
    {
      /* it has Le field*/
      c->a->expected_res_size = epo->buf[-1];
      len--;
    }
  else
    {
    error:
      epo->err = 1;
      return 0;
    }

  c->a->cmd_apdu_data_len += len;
  return 0;
}


static void nomore_data (struct ep_out *epo, size_t len)
{
  (void)len;
  epo->err = 1;
  epo->end_rx = end_nomore_data;
  epo->buf = NULL;
  epo->buf_len = 0;
  epo->cnt = 0;
  epo->next_buf = nomore_data;
}

#define INS_GET_RESPONSE 0xc0

static void icc_cmd_apdu_data (struct ep_out *epo, size_t len)
{
  struct ccid *c = (struct ccid *)epo->priv;

  (void)len;
  if (c->state == APDU_STATE_RESULT_GET_RESPONSE
      && c->a->cmd_apdu_head[1] != INS_GET_RESPONSE)
    {
      /*
       * Handling exceptional request.
       *
       * Host didn't finish receiving the whole response APDU by GET RESPONSE,
       * but initiates another command.
       */

      c->state = APDU_STATE_WAIT_COMMAND;
      c->p = c->a->cmd_apdu_data;
      c->len = MAX_CMD_APDU_DATA_SIZE;
    }
  else if (c->state == APDU_STATE_COMMAND_CHAINING)
    {
      if (c->chained_cls_ins_p1_p2[0] != (c->a->cmd_apdu_head[0] & ~0x10)
	  || c->chained_cls_ins_p1_p2[1] != c->a->cmd_apdu_head[1]
	  || c->chained_cls_ins_p1_p2[2] != c->a->cmd_apdu_head[2]
	  || c->chained_cls_ins_p1_p2[3] != c->a->cmd_apdu_head[3])
	/*
	 * Handling exceptional request.
	 *
	 * Host stops sending command APDU using command chaining,
	 * and start another command APDU.
	 *
	 * Discard old one, and start handling new one.
	 */
	{
	  c->state = APDU_STATE_WAIT_COMMAND;
	  c->p = c->a->cmd_apdu_data;
	  c->len = MAX_CMD_APDU_DATA_SIZE;
	  c->a->cmd_apdu_data_len = 0;
	}
    }

  epo->end_rx = end_cmd_apdu_data;
  epo->buf = c->p;
  epo->buf_len = c->len;
  epo->cnt = 0;
  epo->next_buf = nomore_data;
}

static void icc_abdata (struct ep_out *epo, size_t len)
{
  struct ccid *c = (struct ccid *)epo->priv;

  (void)len;
  c->a->seq = c->icc_header.seq;
  if (c->icc_header.msg_type == ICC_XFR_BLOCK)
    {
      c->a->seq = c->icc_header.seq;
      epo->end_rx = end_cmd_apdu_head;
      epo->buf = c->a->cmd_apdu_head;
      epo->buf_len = 5;
      epo->cnt = 0;
      epo->next_buf = icc_cmd_apdu_data;
    }
  else
    {
      epo->end_rx = end_abdata;
      epo->buf = c->p;
      epo->buf_len = c->len;
      epo->cnt = 0;
      epo->next_buf = nomore_data;
    }
}


static void
icc_prepare_receive (struct ccid *c)
{
  DEBUG_INFO ("Rx ready\r\n");

  c->epo->err = 0;
  c->epo->buf = (uint8_t *)&c->icc_header;
  c->epo->buf_len = sizeof (struct icc_header);
  c->epo->cnt = 0;
  c->epo->next_buf = icc_abdata;
  c->epo->end_rx = end_icc_rx;
  usb_lld_rx_enable (c->epo->ep_num);
}

/*
 * Rx ready callback
 */

void
EP1_OUT_Callback (void)
{
  struct ep_out *epo = &endpoint_out;
  size_t len = usb_lld_rx_data_len (epo->ep_num);
  int offset = 0;
  int cont;
  size_t orig_len = len;

  while (epo->err == 0)
    if (len == 0)
      break;
    else if (len <= epo->buf_len)
      {
	usb_lld_rxcpy (epo->buf, epo->ep_num, offset, len);
	epo->buf += len;
	epo->cnt += len;
	epo->buf_len -= len;
	break;
      }
    else /* len > buf_len */
      {
	usb_lld_rxcpy (epo->buf, epo->ep_num, offset, epo->buf_len);
	len -= epo->buf_len;
	offset += epo->buf_len;
	epo->next_buf (epo, len); /* Update epo->buf, cnt, buf_len */
      }

  /*
   * ORIG_LEN to distingush ZLP and the end of transaction
   *  (ORIG_LEN != USB_LL_BUF_SIZE)
   */
  cont = epo->end_rx (epo, orig_len);

  if (cont)
    usb_lld_rx_enable (epo->ep_num);
  else
    epo->notify (epo);
}


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
static const uint8_t ATR[] = {
  0x3b, 0xda, 0x11, 0xff, 0x81, 0xb1, 0xfe, 0x55, 0x1f, 0x03,
  0x00,
	0x31, 0x84, /* full DF name, GET DATA, MF */
        0x73,
              0x80, /* DF full name */
  	      0x01, /* 1-byte */
  	      0x80, /* Command chaining, No extended Lc and extended Le */
  	0x00,
        0x90, 0x00,
 (0xda^0x11^0xff^0x81^0xb1^0xfe^0x55^0x1f^0x03
  ^0x00^0x31^0x84^0x73^0x80^0x01^0x80^0x00^0x90^0x00)
};

/*
 * Send back error
 */
static void icc_error (struct ccid *c, int offset)
{
  uint8_t icc_reply[ICC_MSG_HEADER_SIZE];

  icc_reply[0] = ICC_SLOT_STATUS_RET; /* Any value should be OK */
  icc_reply[1] = 0x00;
  icc_reply[2] = 0x00;
  icc_reply[3] = 0x00;
  icc_reply[4] = 0x00;
  icc_reply[5] = 0x00;	/* Slot */
  icc_reply[ICC_MSG_SEQ_OFFSET] = c->icc_header.seq;
  if (c->icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 0;
  icc_reply[ICC_MSG_STATUS_OFFSET] |= ICC_CMD_STATUS_ERROR; /* Failed */
  icc_reply[ICC_MSG_ERROR_OFFSET] = offset;
  icc_reply[ICC_MSG_CHAIN_OFFSET] = 0x00;

  /* This is a single packet Bulk-IN transaction */
  c->epi->buf = NULL;
  c->epi->tx_done = 1;
  usb_lld_write (c->epi->ep_num, icc_reply, ICC_MSG_HEADER_SIZE);
}

static WORKING_AREA(waGPGthread, 128*16);
extern msg_t GPGthread (void *arg);


/* Send back ATR (Answer To Reset) */
enum icc_state
icc_power_on (struct ccid *c)
{
  size_t size_atr = sizeof (ATR);
  uint8_t p[ICC_MSG_HEADER_SIZE];

  if (c->application == NULL)
    c->application = chThdCreateStatic (waGPGthread, sizeof(waGPGthread),
					NORMALPRIO, GPGthread,
					(void *)c->icc_thread);

  p[0] = ICC_DATA_BLOCK_RET;
  p[1] = size_atr;
  p[2] = 0x00;
  p[3] = 0x00;
  p[4] = 0x00;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = c->icc_header.seq;
  p[ICC_MSG_STATUS_OFFSET] = 0x00;
  p[ICC_MSG_ERROR_OFFSET] = 0x00;
  p[ICC_MSG_CHAIN_OFFSET] = 0x00;

  usb_lld_txcpy (p, c->epi->ep_num, 0, ICC_MSG_HEADER_SIZE);
  usb_lld_txcpy (ATR, c->epi->ep_num, ICC_MSG_HEADER_SIZE, size_atr);

  /* This is a single packet Bulk-IN transaction */
  c->epi->buf = NULL;
  c->epi->tx_done = 1;
  usb_lld_tx_enable (c->epi->ep_num, ICC_MSG_HEADER_SIZE + size_atr);
  DEBUG_INFO ("ON\r\n");

  return ICC_STATE_WAIT;
}

static void
icc_send_status (struct ccid *c)
{
  uint8_t icc_reply[ICC_MSG_HEADER_SIZE];

  icc_reply[0] = ICC_SLOT_STATUS_RET;
  icc_reply[1] = 0x00;
  icc_reply[2] = 0x00;
  icc_reply[3] = 0x00;
  icc_reply[4] = 0x00;
  icc_reply[5] = 0x00;	/* Slot */
  icc_reply[ICC_MSG_SEQ_OFFSET] = c->icc_header.seq;
  if (c->icc_state == ICC_STATE_START)
    /* 1: ICC present but not activated 2: No ICC present */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 1;
  else
    /* An ICC is present and active */
    icc_reply[ICC_MSG_STATUS_OFFSET] = 0;
  icc_reply[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_reply[ICC_MSG_CHAIN_OFFSET] = 0x00;

  /* This is a single packet Bulk-IN transaction */
  c->epi->buf = NULL;
  c->epi->tx_done = 1;
  usb_lld_write (c->epi->ep_num, icc_reply, ICC_MSG_HEADER_SIZE);

#ifdef DEBUG_MORE
  DEBUG_INFO ("St\r\n");
#endif
}

enum icc_state
icc_power_off (struct ccid *c)
{
  if (c->application)
    {
      chThdTerminate (c->application);
      chEvtSignal (c->application, EV_NOP);
      chThdWait (c->application);
      c->application = NULL;
    }

  c->icc_state = ICC_STATE_START; /* This status change should be here */
  icc_send_status (c);
  DEBUG_INFO ("OFF\r\n");
  return ICC_STATE_START;
}

static void
icc_send_data_block_0x9000 (struct ccid *c)
{
  uint8_t p[ICC_MSG_HEADER_SIZE+2];
  size_t len = 2;

  p[0] = ICC_DATA_BLOCK_RET;
  p[1] = len & 0xFF;
  p[2] = (len >> 8)& 0xFF;
  p[3] = (len >> 16)& 0xFF;
  p[4] = (len >> 24)& 0xFF;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = c->a->seq;
  p[ICC_MSG_STATUS_OFFSET] = 0;
  p[ICC_MSG_ERROR_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET+1] = 0x90;
  p[ICC_MSG_CHAIN_OFFSET+2] = 0x00;

  usb_lld_txcpy (p, c->epi->ep_num, 0, ICC_MSG_HEADER_SIZE + len);
  c->epi->buf = NULL;
  c->epi->tx_done = 1;

  usb_lld_tx_enable (c->epi->ep_num, ICC_MSG_HEADER_SIZE + len);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}

static void
icc_send_data_block (struct ccid *c, uint8_t status)
{
  int tx_size = USB_LL_BUF_SIZE;
  uint8_t p[ICC_MSG_HEADER_SIZE];
  size_t len;

  if (status == 0)
    len = c->a->res_apdu_data_len + 2;
  else
    len = 0;

  p[0] = ICC_DATA_BLOCK_RET;
  p[1] = len & 0xFF;
  p[2] = (len >> 8)& 0xFF;
  p[3] = (len >> 16)& 0xFF;
  p[4] = (len >> 24)& 0xFF;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = c->a->seq;
  p[ICC_MSG_STATUS_OFFSET] = status;
  p[ICC_MSG_ERROR_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET] = 0;

  usb_lld_txcpy (p, c->epi->ep_num, 0, ICC_MSG_HEADER_SIZE);
  if (len == 0)
    {
      usb_lld_tx_enable (c->epi->ep_num, ICC_MSG_HEADER_SIZE);
      return;
    }

  if (ICC_MSG_HEADER_SIZE + len <= USB_LL_BUF_SIZE)
    {
      usb_lld_txcpy (c->a->res_apdu_data, c->epi->ep_num,
		     ICC_MSG_HEADER_SIZE, c->a->res_apdu_data_len);
      usb_lld_txcpy (c->sw1sw2, c->epi->ep_num,
		     ICC_MSG_HEADER_SIZE + c->a->res_apdu_data_len, 2);
      c->epi->buf = NULL;
      if (ICC_MSG_HEADER_SIZE + len < USB_LL_BUF_SIZE)
	c->epi->tx_done = 1;
      tx_size = ICC_MSG_HEADER_SIZE + len;
    }
  else if (ICC_MSG_HEADER_SIZE + len - 1 == USB_LL_BUF_SIZE)
    {
      usb_lld_txcpy (c->a->res_apdu_data, c->epi->ep_num,
		     ICC_MSG_HEADER_SIZE, c->a->res_apdu_data_len);
      usb_lld_txcpy (c->sw1sw2, c->epi->ep_num,
		     ICC_MSG_HEADER_SIZE + c->a->res_apdu_data_len, 1);
      c->epi->buf = &c->sw1sw2[1];
      c->epi->cnt = 1;
      c->epi->buf_len = 1;
      c->epi->next_buf = no_buf;
    }
  else if (ICC_MSG_HEADER_SIZE + len - 2 == USB_LL_BUF_SIZE)
    {
      usb_lld_txcpy (c->a->res_apdu_data, c->epi->ep_num,
		     ICC_MSG_HEADER_SIZE, c->a->res_apdu_data_len);
      c->epi->buf = &c->sw1sw2[0];
      c->epi->cnt = 0;
      c->epi->buf_len = 2;
      c->epi->next_buf = no_buf;
    }
  else
    {
      usb_lld_txcpy (c->a->res_apdu_data, c->epi->ep_num, ICC_MSG_HEADER_SIZE,
		     USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE);
      c->epi->buf = c->a->res_apdu_data + USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE;
      c->epi->cnt = USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE;
      c->epi->buf_len = c->a->res_apdu_data_len
	- (USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE);
      c->epi->next_buf = get_sw1sw2;
    }

  usb_lld_tx_enable (c->epi->ep_num, tx_size);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}

static void
icc_send_data_block_gr (struct ccid *c, size_t chunk_len)
{
  int tx_size = USB_LL_BUF_SIZE;
  uint8_t p[ICC_MSG_HEADER_SIZE];
  size_t len = chunk_len + 2;

  p[0] = ICC_DATA_BLOCK_RET;
  p[1] = len & 0xFF;
  p[2] = (len >> 8)& 0xFF;
  p[3] = (len >> 16)& 0xFF;
  p[4] = (len >> 24)& 0xFF;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = c->a->seq;
  p[ICC_MSG_STATUS_OFFSET] = 0;
  p[ICC_MSG_ERROR_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET] = 0;

  usb_lld_txcpy (p, c->epi->ep_num, 0, ICC_MSG_HEADER_SIZE);

  set_sw1sw2 (c->epi);

  if (chunk_len <= USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE)
    {
      int size_for_sw;

      if (chunk_len <= USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE - 2)
	size_for_sw = 2;
      else if (chunk_len == USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE - 1)
	size_for_sw = 1;
      else
	size_for_sw = 0;

      usb_lld_txcpy (c->p, c->epi->ep_num, ICC_MSG_HEADER_SIZE, chunk_len);
      if (size_for_sw)
	usb_lld_txcpy (c->sw1sw2, c->epi->ep_num,
		       ICC_MSG_HEADER_SIZE + chunk_len, size_for_sw);
      tx_size = ICC_MSG_HEADER_SIZE + chunk_len + size_for_sw;
      if (size_for_sw == 2)
	{
	  c->epi->buf = NULL;
	  if (tx_size < USB_LL_BUF_SIZE)
	    c->epi->tx_done = 1;
	  /* Don't set epi->tx_done = 1, when it requires ZLP */
	}
      else
	{
	  c->epi->buf = c->sw1sw2 + size_for_sw;
	  c->epi->cnt = size_for_sw;
	  c->epi->buf_len = 2 - size_for_sw;
	  c->epi->next_buf = no_buf;
	}
    }
  else
    {
      usb_lld_txcpy (c->p, c->epi->ep_num, ICC_MSG_HEADER_SIZE,
		     USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE);
      c->epi->buf = c->p + USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE;
      c->epi->cnt = 0;
      c->epi->buf_len = chunk_len - (USB_LL_BUF_SIZE - ICC_MSG_HEADER_SIZE);
      c->epi->next_buf = get_sw1sw2;
    }

  c->p += chunk_len;
  c->len -= chunk_len;
  usb_lld_tx_enable (c->epi->ep_num, tx_size);
#ifdef DEBUG_MORE
  DEBUG_INFO ("DATA\r\n");
#endif
}


static void
icc_send_params (struct ccid *c)
{
  uint8_t p[ICC_MSG_HEADER_SIZE];
  const uint8_t params[] =  {
    0x11,   /* bmFindexDindex */
    0x11, /* bmTCCKST1 */
    0xFE, /* bGuardTimeT1 */
    0x55, /* bmWaitingIntegersT1 */
    0x03, /* bClockStop */
    0xFE, /* bIFSC */
    0    /* bNadValue */
  };

  p[0] = ICC_PARAMS_RET;
  p[1] = 0x07;	/* Length = 0x00000007 */
  p[2] = 0;
  p[3] = 0;
  p[4] = 0;
  p[5] = 0x00;	/* Slot */
  p[ICC_MSG_SEQ_OFFSET] = c->icc_header.seq;
  p[ICC_MSG_STATUS_OFFSET] = 0;
  p[ICC_MSG_ERROR_OFFSET] = 0;
  p[ICC_MSG_CHAIN_OFFSET] = 0x01;  /* ProtocolNum: T=1 */

  usb_lld_txcpy (p, c->epi->ep_num, 0, ICC_MSG_HEADER_SIZE);
  usb_lld_txcpy (params, c->epi->ep_num, ICC_MSG_HEADER_SIZE, sizeof params);

  /* This is a single packet Bulk-IN transaction */
  c->epi->buf = NULL;
  c->epi->tx_done = 1;
  usb_lld_tx_enable (c->epi->ep_num, ICC_MSG_HEADER_SIZE + sizeof params);
#ifdef DEBUG_MORE
  DEBUG_INFO ("PARAMS\r\n");
#endif
}


static enum icc_state
icc_handle_data (struct ccid *c)
{
  enum icc_state next_state = c->icc_state;

  if (c->err != 0)
    {
      ccid_reset (c);
      icc_error (c, ICC_OFFSET_DATA_LEN);
      return next_state;
    }

  switch (c->icc_state)
    {
    case ICC_STATE_START:
      if (c->icc_header.msg_type == ICC_POWER_ON)
	{
	  ccid_reset (c);
	  next_state = icc_power_on (c);
	}
      else if (c->icc_header.msg_type == ICC_POWER_OFF)
	{
	  ccid_reset (c);
	  next_state = icc_power_off (c);
	}
      else if (c->icc_header.msg_type == ICC_SLOT_STATUS)
	icc_send_status (c);
      else
	{
	  DEBUG_INFO ("ERR01\r\n");
	  icc_error (c, ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_WAIT:
      if (c->icc_header.msg_type == ICC_POWER_ON)
	{
	  /* Not in the spec., but pcscd/libccid */
	  ccid_reset (c);
	  next_state = icc_power_on (c);
	}
      else if (c->icc_header.msg_type == ICC_POWER_OFF)
	{
	  ccid_reset (c);
	  next_state = icc_power_off (c);
	}
      else if (c->icc_header.msg_type == ICC_SLOT_STATUS)
	icc_send_status (c);
      else if (c->icc_header.msg_type == ICC_XFR_BLOCK)
	{
	  if (c->icc_header.param == 0)
	    {
	      if ((c->a->cmd_apdu_head[0] & 0x10) == 0)
		{
		  if (c->state == APDU_STATE_COMMAND_CHAINING)
		    {		/* command chaining finished */
		      c->p += c->a->cmd_apdu_head[4];
		      c->a->cmd_apdu_head[4] = 0;
		      DEBUG_INFO ("CMD chaning finished.\r\n");
		    }

		  if (c->a->cmd_apdu_head[1] == INS_GET_RESPONSE
		      && c->state == APDU_STATE_RESULT_GET_RESPONSE)
		    {
		      size_t len = c->a->expected_res_size;

		      if (c->len <= c->a->expected_res_size)
			len = c->len;

		      icc_send_data_block_gr (c, len);
		      if (c->len == 0)
			c->state = APDU_STATE_RESULT;
		      c->icc_state = ICC_STATE_WAIT;
		      DEBUG_INFO ("GET Response.\r\n");
		    }
		  else
		    {		  /* Give this message to GPG thread */
		      c->state = APDU_STATE_COMMAND_RECEIVED;

		      c->a->sw = 0x9000;
		      c->a->res_apdu_data_len = 0;
		      c->a->res_apdu_data = &icc_buffer[5];

		      chEvtSignal (c->application, EV_CMD_AVAILABLE);
		      next_state = ICC_STATE_EXECUTE;
		    }
		}
	      else
		{
		  if (c->state == APDU_STATE_WAIT_COMMAND)
		    {		/* command chaining is started */
		      c->a->cmd_apdu_head[0] &= ~0x10;
		      memcpy (c->chained_cls_ins_p1_p2, c->a->cmd_apdu_head, 4);
		      c->state = APDU_STATE_COMMAND_CHAINING;
		    }

		  c->p += c->a->cmd_apdu_head[4];
		  c->len -= c->a->cmd_apdu_head[4];
		  icc_send_data_block_0x9000 (c);
		  DEBUG_INFO ("CMD chaning...\r\n");
		}
	    }
	  else
	    {		     /* ICC block chaining is not supported. */
	      DEBUG_INFO ("ERR02\r\n");
	      icc_error (c, ICC_OFFSET_PARAM);
	    }
	}
      else if (c->icc_header.msg_type == ICC_SET_PARAMS
	       || c->icc_header.msg_type == ICC_GET_PARAMS)
	icc_send_params (c);
      else if (c->icc_header.msg_type == ICC_SECURE)
	{
	  if (c->p != c->a->cmd_apdu_data)
	    {
	      /* SECURE received in the middle of command chaining */
	      ccid_reset (c);
	      icc_error (c, ICC_OFFSET_DATA_LEN);
	      return next_state;
	    }

	  if (c->p[10-10] == 0x00) /* PIN verification */
	    {
	      c->a->cmd_apdu_head[0] = c->p[25-10];
	      c->a->cmd_apdu_head[1] = c->p[26-10];
	      c->a->cmd_apdu_head[2] = c->p[27-10];
	      c->a->cmd_apdu_head[3] = c->p[28-10];
	      /**/
	      c->a->cmd_apdu_data[0] = 0; /* bConfirmPIN */
	      c->a->cmd_apdu_data[1] = c->p[17-10]; /* bEntryValidationCondition */
	      c->a->cmd_apdu_data[2] = c->p[18-10]; /* bNumberMessage */
	      c->a->cmd_apdu_data[3] = c->p[19-10]; /* wLangId L */
	      c->a->cmd_apdu_data[4] = c->p[20-10]; /* wLangId H */
	      c->a->cmd_apdu_data[5] = c->p[21-10]; /* bMsgIndex */

	      c->a->cmd_apdu_data_len = 6;
	      c->a->expected_res_size = 0;

	      c->a->sw = 0x9000;
	      c->a->res_apdu_data_len = 0;
	      c->a->res_apdu_data = &c->p[5];

	      chEvtSignal (c->application, EV_VERIFY_CMD_AVAILABLE);
	      next_state = ICC_STATE_EXECUTE;
	    }
	  else if (c->p[10-10] == 0x01) /* PIN Modification */
	    {
	      uint8_t num_msgs = c->p[21-10];

	      if (num_msgs == 0x00)
		num_msgs = 1;
	      else if (num_msgs == 0xff)
		num_msgs = 3;
	      c->a->cmd_apdu_head[0] = c->p[27 + num_msgs-10];
	      c->a->cmd_apdu_head[1] = c->p[28 + num_msgs-10];
	      c->a->cmd_apdu_head[2] = c->p[29 + num_msgs-10];
	      c->a->cmd_apdu_head[3] = c->p[30 + num_msgs-10];
	      /**/
	      c->a->cmd_apdu_data[0] = c->p[19-10]; /* bConfirmPIN */
	      c->a->cmd_apdu_data[1] = c->p[20-10]; /* bEntryValidationCondition */
	      c->a->cmd_apdu_data[2] = c->p[21-10]; /* bNumberMessage */
	      c->a->cmd_apdu_data[3] = c->p[22-10]; /* wLangId L */
	      c->a->cmd_apdu_data[4] = c->p[23-10]; /* wLangId H */
	      c->a->cmd_apdu_data[5] = c->p[24-10]; /* bMsgIndex, bMsgIndex1 */
	      if (num_msgs >= 2)
		c->a->cmd_apdu_data[6] = c->p[25-10]; /* bMsgIndex2 */
	      if (num_msgs == 3)
		c->a->cmd_apdu_data[7] = c->p[26-10]; /* bMsgIndex3 */

	      c->a->cmd_apdu_data_len = 5 + num_msgs;
	      c->a->expected_res_size = 0;

	      c->a->sw = 0x9000;
	      c->a->res_apdu_data_len = 0;
	      c->a->res_apdu_data = &icc_buffer[5];

	      chEvtSignal (c->application, EV_MODIFY_CMD_AVAILABLE);
	      next_state = ICC_STATE_EXECUTE;
	    }
	  else
	    icc_error (c, ICC_MSG_DATA_OFFSET);
	}
      else
	{
	  DEBUG_INFO ("ERR03\r\n");
	  DEBUG_BYTE (c->icc_header.msg_type);
	  icc_error (c, ICC_OFFSET_CMD_NOT_SUPPORTED);
	}
      break;
    case ICC_STATE_EXECUTE:
      if (c->icc_header.msg_type == ICC_POWER_OFF)
	next_state = icc_power_off (c);
      else if (c->icc_header.msg_type == ICC_SLOT_STATUS)
	icc_send_status (c);
      else
	{
	  DEBUG_INFO ("ERR04\r\n");
	  DEBUG_BYTE (c->icc_header.msg_type);
	  icc_error (c, ICC_OFFSET_CMD_NOT_SUPPORTED);
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
icc_handle_timeout (struct ccid *c)
{
  enum icc_state next_state = c->icc_state;

  switch (c->icc_state)
    {
    case ICC_STATE_EXECUTE:
      icc_send_data_block (c, ICC_CMD_STATUS_TIMEEXT);
      break;
    default:
      break;
    }

  return next_state;
}

#define USB_ICC_TIMEOUT MS2ST(1950)


static struct ccid ccid;


msg_t
USBthread (void *arg)
{
  struct ep_in *epi = &endpoint_in;
  struct ep_out *epo = &endpoint_out;
  struct ccid *c = &ccid;
  struct apdu *a = &apdu;

  (void)arg;

  epi_init (epi, ENDP1, notify_tx, c);
  epo_init (epo, ENDP1, notify_icc, c);
  ccid_init (c, epi, epo, a, chThdSelf ());
  apdu_init (a);

  chEvtClear (ALL_EVENTS);

  icc_prepare_receive (c);
  while (1)
    {
      eventmask_t m;

      m = chEvtWaitOneTimeout (ALL_EVENTS, USB_ICC_TIMEOUT);

      if (m == EV_RX_DATA_READY)
	c->icc_state = icc_handle_data (c);
      else if (m == EV_EXEC_FINISHED)
	if (c->icc_state == ICC_STATE_EXECUTE)
	  {
	    c->a->cmd_apdu_data_len = 0;
	    c->sw1sw2[0] = c->a->sw >> 8;
	    c->sw1sw2[1] = c->a->sw & 0xff;

	    if (c->a->res_apdu_data_len <= c->a->expected_res_size)
	      {
		c->state = APDU_STATE_RESULT;
		icc_send_data_block (c, 0);
		c->icc_state = ICC_STATE_WAIT;
	      }
	    else
	      {
		c->state = APDU_STATE_RESULT_GET_RESPONSE;
		c->p = c->a->res_apdu_data;
		c->len = c->a->res_apdu_data_len;
		icc_send_data_block_gr (c, c->a->expected_res_size);
		c->icc_state = ICC_STATE_WAIT;
	      }
	  }
	else
	  {
	    DEBUG_INFO ("ERR07\r\n");
	  }
      else if (m == EV_TX_FINISHED)
	{
	  if (c->state == APDU_STATE_RESULT)
	    {
	      c->state = APDU_STATE_WAIT_COMMAND;
	      c->p = c->a->cmd_apdu_data;
	      c->len = MAX_CMD_APDU_DATA_SIZE;
	      c->err = 0;
	      c->a->cmd_apdu_data_len = 0;
	      c->a->expected_res_size = 0;
	    }

	  if (c->state == APDU_STATE_WAIT_COMMAND
	      || c->state == APDU_STATE_COMMAND_CHAINING
	      || c->state == APDU_STATE_RESULT_GET_RESPONSE)
	    icc_prepare_receive (c);
	}
      else			/* Timeout */
	c->icc_state = icc_handle_timeout (c);
    }

  return 0;
}

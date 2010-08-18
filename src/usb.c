/*
 * usb.c -- 
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

#include "ch.h"
#include "hal.h"

#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_mem.h"
#include "hw_config.h"
#include "usb_istr.h"

Mutex icc_in_mutex;

static uint8_t icc_buffer_out[64];
static uint8_t icc_buffer_in[64];

static __IO uint32_t icc_count_out = 0;
static uint32_t icc_count_in = 0;

void
EP4_IN_Callback(void)
{
  icc_count_in = 0;
}

void
EP5_OUT_Callback(void)
{
  /* Get the received data buffer and update the counter */
  icc_count_out = USB_SIL_Read (EP5_OUT, icc_buffer_out);
}

#define ICC_POWER_ON	0x62
#define ICC_POWER_OFF	0x63
#define ICC_SLOT_STATUS	0x65
#define XFR_BLOCK	0x6F

#define ICC_MSG_SEQ_OFFSET 6
#define ICC_MSG_STATUS_OFFSET 7
#define ICC_MSG_ERROR_OFFSET 8

#if 0
0 bMessageType  0x62
1 dwLength      0x00000000
5 bSlot         0x00   FIXED
6 bSeq          0x00-FF
7 bReserved     0x01   FIXED
8 abRFU         0x0000
-->
0  bMessageType    0x80 Indicates RDR_to_PC_DataBlock
1  dwLength             Size of bytes for the ATR
5  bSlot           0x00 FIXED
6  bSeq            	Sequence number for the corresponding command.
7  bStatus              USB-ICC Status register as defined in Table 6.1-8
8  bError               USB-ICC Error register as defined in Table 6.1-9
9  bChainParameter 0x00 Indicates that this message contains the complete ATR.
10 abData               ATR


0 bMessageType 0x63
1 dwLength     0x00000000       Message-specific data length
5 bSlot        0x00             FIXED
6 bSeq         0x00-FF          Sequence number for command.
7 abRFU        0x000000
-->
0 bMessageType  81h         Indicates RDR_to_PC_SlotStatus
1 dwLength      0x00000000  Message-specific data length
5 bSlot         0x00        FIXED
6 bSeq          	     Sequence number for the corresponding command.
7 bStatus                   USB-ICC Status register as defined in Table 6.1-8
8 bError                    USB-ICC Error register as defined in Table 6.1-9
9 bReserved     0x00        FIXED


0  bMessageType     0x6F    Indicates PC_to_RDR_XfrBlock
1  dwLength                 Size of abData field of this message
5  bSlot            0x00    FIXED
6  bSeq             0x00-FF Sequence number for command.
7  bReserved        0x00    FIXED
8  wLevelParameter
                           0x0000
                           the command APDU begins and ends with this command

                           0x0001
                           the command APDU begins with this command, and
                           continue in the next PC_to_RDR_XfrBlock

                           0x0002
                           this abData field continues a command APDU and
                           ends the command APDU

                           0x0003
                           the abData field continues a command APDU and
                           another block is to follow

                           0x0010
                           empty abData field, continuation of response APDU
                           is expected in the next RDR_to_PC_DataBlock.

10 abData                  Data block sent to the USB-ICC
-->
0  bMessageType   0x80      Indicates RDR_to_PC_DataBlock
1  dwLength                 Size of abData field of this message
5  bSlot          0x00      FIXED
6  bSeq                     Sequence number for the corresponding command.
                  
7  bStatus                 USB-ICC Status register as defined in Table 6.1-8
8  bError                  USB-ICC Error register as defined in Table 6.1-9
9  bChainParameter
                           Indicates if the response is complete, to be
                           continued or if the command APDU can continue
                           0x00: The response APDU begins and ends in this command
                           0x01: The response APDU begins with this command and is to continue
                           0x02: This abData field continues the response
                                 APDU and ends the response APDU
                           0x03: This abData field continues the response
                                 APDU and another block is to follow
                           0x10: Empty abData field, continuation of the
                                 command APDU is expected in next PC_to_RDR_XfrBlock command
10 abData


/* status code and error code */
0            bmIccStatus          1         0, 1, 2 0= The USB-ICC is present and activated.
                                  (2 bits)          1= The USB-ICC is present but not activated
(2 bits)                                            2= The USB-ICC is virtually not present
                                                    3= RFU
                                  (4 bits)          RFU
(6 bits)     bmCommandStatus      (2 bits)  0, 1, 2 0= Processed without error.
                                                    1= Failed, error condition given by bError.
                                                    2= Time extension is requested
                                                    3= RFU
1            bError               1                 Error codes


/* error code */
ICC_MUTE     0xFE                         The applications of the USB-ICC did not respond
                                          or the ATR could not be sent by the USB-ICC.
XFR_OVERRUN  0xFC                         The USB-ICC detected a buffer overflow when
                                          receiving a data block.
HW_ERROR     0xFB                         The USB-ICC detected a hardware error.

            (0xC0 to 0x81)              User defined

	    0xE0, 0xEF, 0xF0,		These values shall not be used by the USB-ICC 
            0xF2..0xF8, 0xFD

            all others                  Reserved for future use
            (0x80 and those filling the gaps)

extern const uchar *icc_power_on (void);
extern byte icc_get_status (void);


PC_to_RDR_IccPowerOff
RDR_to_PC_SlotStatus

PC_to_RDR_IccPowerOn
RDR_to_PC_DataBlock             

PC_to_RDR_XfrBlock
RDR_to_PC_DataBlock
#endif

/* Direct conversion, T=1, "FSIJ" */
static const char ATR[] = { '\x3B', '\x84', '\x01', 'F', 'S', 'I', 'J' };

void
icc_power_on (char *buf, int len)
{
  int i, size_atr;

  size_atr = sizeof (ATR);

  chMtxLock (&icc_in_mutex);
  icc_buffer_in[0] = 0x80;
  icc_buffer_in[1] = size_atr;
		/* not including '\0' at the end */
  icc_buffer_in[2] = 0x00;
  icc_buffer_in[3] = 0x00;
  icc_buffer_in[4] = 0x00;
  icc_buffer_in[5] = 0x00;	/* Slot */
  icc_buffer_in[ICC_MSG_SEQ_OFFSET] = buf[ICC_MSG_SEQ_OFFSET];
  icc_buffer_in[ICC_MSG_STATUS_OFFSET] = 0x00;
  icc_buffer_in[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_buffer_in[9] = 0x00;
  for (i = 0; i < size_atr; i++)
    icc_buffer_in[i+10] = ATR[i];

  icc_count_in = 10 + size_atr;

  USB_SIL_Write (EP4_IN, icc_buffer_in, icc_count_in);
  SetEPTxValid (ENDP4);
  chMtxUnlock ();

  _write ("ON\r\n", 4);
}

void
icc_power_off (char *buf, int len)
{
  chMtxLock (&icc_in_mutex);

  icc_buffer_in[0] = 0x81;
  icc_buffer_in[1] = 0x00;
  icc_buffer_in[2] = 0x00;
  icc_buffer_in[3] = 0x00;
  icc_buffer_in[4] = 0x00;
  icc_buffer_in[5] = 0x00;	/* Slot */
  icc_buffer_in[ICC_MSG_SEQ_OFFSET] = buf[ICC_MSG_SEQ_OFFSET];
  icc_buffer_in[ICC_MSG_STATUS_OFFSET] = 0x00;
  icc_buffer_in[ICC_MSG_ERROR_OFFSET] = 0x00;
  icc_buffer_in[9] = 0x00;

  icc_count_in = 10;
  USB_SIL_Write (EP4_IN, icc_buffer_in, icc_count_in);
  SetEPTxValid (ENDP4);
  chMtxUnlock ();

  _write ("OFF\r\n", 5);
}

msg_t
USBThread (void *arg)
{
  char b[3];

  chMtxInit (&icc_in_mutex);

  while (TRUE)
    {
      while (icc_count_out == 0)
	chThdSleepMilliseconds (1);

      b[0] = icc_buffer_out[0];
      b[1] = '\r';
      b[2] = '\n';

      _write (b, 3);
      if (icc_buffer_out[0] == ICC_POWER_ON)
	{
	  /* Send back ATR (Answer To Reset) */
	  icc_power_on (icc_buffer_out, icc_count_out);
	}
      else if (icc_buffer_out[0] == ICC_POWER_OFF
	       || icc_buffer_out[0] == ICC_SLOT_STATUS)
	{
	  /* Kill ICC thread(s) and send back slot status */
	  icc_power_off (icc_buffer_out, icc_count_out);
	}
      else if (icc_buffer_out[0] == XFR_BLOCK)
	{
	  /* Give this message to ICC thread */
	}
      else
	{
	}

      icc_count_out = 0;
      SetEPRxValid (ENDP5);
    }

  return 0;
}

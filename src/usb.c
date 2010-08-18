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

#define ICC_POWER_ON	0x62
0 bMessageType  0x62
1 dwLength      0x00000000
5 bSlot         0x00   FIXED
6 bSeq          0x00-FF
7 bReserved     0x01   FIXED
8 abRFU         0x0000

0  bMessageType    0x80 Indicates RDR_to_PC_DataBlock
1  dwLength             Size of bytes for the ATR
5  bSlot           0x00 FIXED
6  bSeq            	Sequence number for the corresponding command.
7  bStatus              USB-ICC Status register as defined in Table 6.1-8
8  bError               USB-ICC Error register as defined in Table 6.1-9
9  bChainParameter 0x00 Indicates that this message contains the complete ATR.
10 abData               ATR



#define ICC_POWER_OFF	0x63
 0 bMessageType 0x63             Indicates PC_to_RDR_IccPowerOn
 1 dwLength     0x00000000       Message-specific data length
 5 bSlot        0x00             FIXED
 6 bSeq         0x00-FF          Sequence number for command.
 7 abRFU        0x000000

 0 bMessageType  81h         Indicates RDR_to_PC_SlotStatus
 1 dwLength      0x00000000  Message-specific data length
 5 bSlot         0x00        FIXED
 6 bSeq          	     Sequence number for the corresponding command.
 7 bStatus                   USB-ICC Status register as defined in Table 6.1-8
 8 bError                    USB-ICC Error register as defined in Table 6.1-9
 9 bReserved     0x00        FIXED


#define XFR_BLOCK	0x6F
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

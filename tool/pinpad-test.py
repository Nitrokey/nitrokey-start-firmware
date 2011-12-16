#! /usr/bin/python

"""
pinpad-test.py - a tool to test pinpad support by card reader.

Copyright (C) 2011 Free Software Initiative of Japan
Author: NIIBE Yutaka <gniibe@fsij.org>

This file is a part of Gnuk, a GnuPG USB Token implementation.

Gnuk is free software: you can redistribute it and/or modify it
under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Gnuk is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public
License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
"""

import sys

# Assume only single CCID device is attached to computer with card

from smartcard.CardType import AnyCardType
from smartcard.CardRequest import CardRequest
from smartcard.util import toHexString

CM_IOCTL_GET_FEATURE_REQUEST = (0x42000000 + 3400)
FEATURE_VERIFY_PIN_DIRECT    = 0x06
FEATURE_MODIFY_PIN_DIRECT    = 0x07

class Card(object):
    def __init__(self, add_a_byte):
        cardtype = AnyCardType()
        cardrequest = CardRequest(timeout=1, cardType=cardtype)
        cardservice = cardrequest.waitforcard()
        self.connection = cardservice.connection
        self.verify_ioctl = -1
        self.modify_ioctl = -1
        self.another_byte = add_a_byte

    def get_features(self):
        p = self.connection.control(CM_IOCTL_GET_FEATURE_REQUEST, [])
        i = 0
        while i < len(p):
            code = p[i]
            l = p[i+1]
            i = i + 2
            if l == 4:
                ioctl = (p[i] << 24) | (p[i+1] << 16) | (p[i+2] << 8) | p[i+3]
                i = i + l
            else:
                i = i + l
                continue
            if code == FEATURE_VERIFY_PIN_DIRECT:
                self.verify_ioctl = ioctl
            elif code == FEATURE_MODIFY_PIN_DIRECT:
                self.modify_ioctl = ioctl
        if self.verify_ioctl == -1:
            raise ValueError, "Not supported"

    def cmd_select_openpgp(self):
        apdu = [0x00, 0xa4, 0x04, 0x00, 6, 0xd2, 0x76, 0x00, 0x01, 0x24, 0x01 ]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, "cmd_select_openpgp"

    def possibly_add_dummy_byte(self):
        if self.another_byte:
            return [ 0 ]
        else:
            return []

    def cmd_verify_pinpad(self, who):
        apdu = [0x00, 0x20, 0x00, 0x80+who ]
        pin_verify = [ 0x00,    # bTimeOut
                       0x00,    # bTimeOut2
                       0x82,    # bmFormatString: Byte, pos=0, left, ASCII.
                       0x00,    # bmPINBlockString
                       0x00,    # bmPINLengthFormat
                       15,      # wPINMaxExtraDigit Low  (PINmax)
                       1,       # wPINMaxExtraDigit High (PINmin) 
                       0x02,    # bEntryValidationCondition
                       0x01,    # bNumberMessage
                       0x00,    # wLangId Low
                       0x00,    # wLangId High
                       0x00,    # bMsgIndex
                       0x00,    # bTeoPrologue[0]
                       0x00,    # bTeoPrologue[1]
                       0x00     # bTeoPrologue[2]
                       ]
        pin_verify += [ len(apdu), 0, 0, 0 ] + apdu + self.possibly_add_dummy_byte()
        data = self.connection.control(self.verify_ioctl,pin_verify)
        sw1 = data[0]
        sw2 = data[1]
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("cmd_verify_pinpad %02x %02x" % (sw1, sw2))

    def send_modify_pinpad(self, adpu, command):
        if self.modify_ioctl == -1:
            raise ValueError, "Not supported"
        pin_modify = [ 0x00, # bTimerOut
                       0x00, # bTimerOut2
                       0x82, # bmFormatString: Byte, pos=0, left, ASCII.
                       0x00, # bmPINBlockString
                       0x00, # bmPINLengthFormat
                       0x00, # bInsertionOffsetOld
                       0x00, # bInsertionOffsetNew
                       15,      # wPINMaxExtraDigit Low  (PINmax)
                       1,       # wPINMaxExtraDigit High (PINmin) 
                       0x03,    # bConfirmPIN: old PIN and new PIN twice
                       0x02,    # bEntryValidationCondition
                       0x03,    # bNumberMessage
                       0x00,    # wLangId Low
                       0x00,    # wLangId High
                       0x00,    # bMsgIndex1
                       0x01,    # bMsgIndex2
                       0x02,    # bMsgIndex3
                       0x00,    # bTeoPrologue[0]
                       0x00,    # bTeoPrologue[1]
                       0x00     # bTeoPrologue[2]
                       ]
        pin_modify += [ len(apdu), 0, 0, 0 ] + apdu + self.possibly_add_dummy_byte()
        data = self.connection.control(self.modify_ioctl,pin_modify)
        sw1 = data[0]
        sw2 = data[1]
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%s %02x %02x" % (command, sw1, sw2))

    def cmd_reset_retry_counter_pinpad(self, who):
        apdu = [0x00, 0x2c, 0x02, 0x80+who ]
        self.send_modify_pinpad(apdu, "cmd_reset_retry_counter_pinpad")

    def cmd_change_reference_data(self, who):
        apdu = [0x00, 0x24, 0x00, 0x80+who ]
        self.send_modify_pinpad(apdu, "cmd_change_reference_data")


def main(who, method, add_a_byte):
    card = Card(add_a_byte)

    card.connection.connect()
    print "Reader/Token:", card.connection.getReader()
    print "ATR:", toHexString( card.connection.getATR() )

    card.get_features()
    card.cmd_select_openpgp()
    if method == "verify"
        card.cmd_verify_pinpad(who)
    elif method == "change"
        card.cmd_change_reference_data(self, who):
    elif method == "unblock"
        card.cmd_reset_retry_counter_pinpad(who)
    else:
        raise ValueError, method
    card.connection.disconnect()

    print "OK."
    return 0

BY_ADMIN = 3
BY_USER = 1

if __name__ == '__main__':
    who = BY_USER
    method = "verify"
    add_a_byte = False
    while len(sys.argv) >= 2:
        option = sys.argv[1]
        sys.argv.pop(1)
        if option == '--admin':
            who = BY_ADMIN
        elif option == '--unblock':
            who = BY_ADMIN
            method = "unblock"
        elif option == '--change':
            method = "change"
        elif option == '--add':
            add_a_byte = True
        else:
            raise ValueError, option
    main(who, method, add_a_byte)

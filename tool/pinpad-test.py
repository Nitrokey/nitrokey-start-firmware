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

BY_ADMIN = 3
BY_USER = 1
PIN_MAX_DEFAULT = 15            # max of VASCO DIGIPASS 920

def confirm_pin_setting(single_step):
    if single_step:
        return 0x01    # bConfirmPIN: new PIN twice
    else:
        return 0x03    # bConfirmPIN: old PIN and new PIN twice

class Card(object):
    def __init__(self, add_a_byte, pinmax):
        cardtype = AnyCardType()
        cardrequest = CardRequest(timeout=1, cardType=cardtype)
        cardservice = cardrequest.waitforcard()
        self.connection = cardservice.connection
        self.verify_ioctl = -1
        self.modify_ioctl = -1
        self.another_byte = add_a_byte
        self.pinmax = pinmax

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
                       self.pinmax, # wPINMaxExtraDigit Low  (PINmax)
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

    def send_modify_pinpad(self, adpu, single_step, command):
        if self.modify_ioctl == -1:
            raise ValueError, "Not supported"
        pin_modify = [ 0x00, # bTimerOut
                       0x00, # bTimerOut2
                       0x82, # bmFormatString: Byte, pos=0, left, ASCII.
                       0x00, # bmPINBlockString
                       0x00, # bmPINLengthFormat
                       0x00, # bInsertionOffsetOld
                       0x00, # bInsertionOffsetNew
                       self.pinmax, # wPINMaxExtraDigit Low  (PINmax)
                       1,       # wPINMaxExtraDigit High (PINmin) 
                       confirm_pin_setting(single_step),
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

    # Note: CCID specification doesn't permit this (only 0x20 and 0x24)
    def cmd_reset_retry_counter_pinpad(self, who):
        if who == BY_ADMIN:
            apdu = [0x00, 0x2c, 0x02, 0x81] # BY_ADMIN
        else:
            apdu = [0x00, 0x2c, 0x00, 0x81] # BY_USER with resetcode
        self.send_modify_pinpad(apdu, False, "cmd_reset_retry_counter_pinpad")

    # Note: CCID specification doesn't permit this (only 0x20 and 0x24)
    def cmd_put_resetcode_pinpad(self):
        apdu = [0x00, 0xda, 0x00, 0xd3]
        self.send_modify_pinpad(apdu, True, "cmd_put_resetcode_pinpad")

    def cmd_change_reference_data_pinpad(self, who, is_exchange):
        if is_exchange:
            apdu = [0x00, 0x24, 1, 0x80+who]
        else:
            apdu = [0x00, 0x24, 0x00, 0x80+who]
        self.send_modify_pinpad(apdu, is_exchange,
                                "cmd_change_reference_data_pinpad")

# "Vasco DIGIPASS 920 [CCID] 00 00"
# "FSIJ Gnuk (0.16-34006F06) 00 00"

def main(who, method, add_a_byte, pinmax, change_by_two_steps):
    card = Card(add_a_byte, pinmax)
    card.connection.connect()

    print "Reader/Token:", card.connection.getReader()
    print "ATR:", toHexString( card.connection.getATR() )

    card.get_features()

    card.cmd_select_openpgp()
    if method == "verify":
        card.cmd_verify_pinpad(who)
    elif method == "change":
        if change_by_two_steps:
            card.cmd_verify_pinpad(who)
            card.cmd_change_reference_data_pinpad(self, who, True)
        else:
            card.cmd_change_reference_data_pinpad(self, who, False)
    elif method == "unblock":
        # It's always by single step
        card.cmd_reset_retry_counter_pinpad(who)
    elif method == "put":
        # It's always by two steps
        card.cmd_verify_pinpad(BY_ADMIN)
        card.cmd_put_resetcode_pinpad()
    else:
        raise ValueError, method
    card.connection.disconnect()

    print "OK."
    return 0

def print_usage():
    print "pinpad-test: testing pinentry of PC/SC card reader"
    print "\thelp:"
    print "\t\t--help:\t\tthis message"
    print "\tmethod:\t\t\t\t\t\t\t\t[verify]"
    print "\t\t--verify:\tverify PIN"
    print "\t\t--change:\tchange PIN (old PIN, new PIN twice)"
    print "\t\t--change2:\tchange PIN by two steps (old PIN, new PIN twice)"
    print "\t\t--unblock:\tunblock PIN (admin PIN or resetcode, new PIN twice)"
    print "\t\t--put:\t\tsetup resetcode (admin PIN, new PIN twice)"
    print "\toptions:"
    print "\t\t--admin:\tby administrator\t\t\t[False]"
    print "\t\t--add:\t\tadd a dummy byte at the end of APDU\t[False]"
    print "\t\t--pinmax:\tspecify maximum length of PIN\t\t[15]"
    print "EXAMPLES:"
    print "   $ pinpad-test                   # verify user's PIN "
    print "   $ pinpad-test --admin --change  # change admin's PIN "
    print "   $ pinpad-test --admin --unblock # change user's PIN by admin's PIN"
    print "   $ pinpad-test --unblock         # change user's PIN by reset code"

if __name__ == '__main__':
    who = BY_USER
    method = "verify"
    add_a_byte = False
    pinmax = PIN_MAX_DEFAULT
    change_by_two_steps = False
    while len(sys.argv) >= 2:
        option = sys.argv[1]
        sys.argv.pop(1)
        if option == '--admin':
            who = BY_ADMIN
        elif option == '--change':
            method = "change"
        elif option == '--change2':
            method = "change"
            change_by_two_steps = True
        elif option == '--unblock':
            method = "unblock"
        elif option == '--add':
            add_a_byte = True
        elif option == '--pinmax':
            pinmax = int(sys.argv[1])
            sys.argv.pop(1)
        elif option == '--put':
            method = "put"
        elif option == "verify":
            method = "verify"
        elif option == '--help':
            print_usage()
            exit(0)
        else:
            raise ValueError, option
    main(who, method, add_a_byte, pinmax, change_by_two_steps)

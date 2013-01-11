#! /usr/bin/python

"""
pinpadtest.py - a tool to test variable length pin entry with pinpad

Copyright (C) 2011, 2012 Free Software Initiative of Japan
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

from getpass import getpass

CM_IOCTL_GET_FEATURE_REQUEST = (0x42000000 + 3400)
FEATURE_VERIFY_PIN_DIRECT    = 0x06
FEATURE_MODIFY_PIN_DIRECT    = 0x07

BY_ADMIN = 3
BY_USER = 1
PIN_MIN_DEFAULT = 6             # min of OpenPGP card
PIN_MAX_DEFAULT = 15            # max of VASCO DIGIPASS 920

def s2l(s):
    return [ ord(c) for c in s ]

def confirm_pin_setting(single_step):
    if single_step:
        return 0x01    # bConfirmPIN: new PIN twice
    else:
        return 0x03    # bConfirmPIN: old PIN and new PIN twice

class Card(object):
    def __init__(self, add_a_byte, pinmin, pinmax, fixed):
        cardtype = AnyCardType()
        cardrequest = CardRequest(timeout=10, cardType=cardtype)
        cardservice = cardrequest.waitforcard()
        self.connection = cardservice.connection
        self.verify_ioctl = -1
        self.modify_ioctl = -1
        self.another_byte = add_a_byte
        self.pinmin = pinmin
        self.pinmax = pinmax
        self.fixed = fixed

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
        if sw1 == 0x61:         # More data
            response, sw1, sw2 = self.connection.transmit([0x00, 0xc0, 0, 0, sw2])
        elif not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("cmd_select_openpgp %02x %02x" % (sw1, sw2))

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
                       self.fixed,    # bmPINBlockString
                       0x00,    # bmPINLengthFormat
                       self.pinmax, # wPINMaxExtraDigit Low  (PINmax)
                       self.pinmin, # wPINMaxExtraDigit High (PINmin) 
                       0x02,    # bEntryValidationCondition
                       0x01,    # bNumberMessage
                       0x00,    # wLangId Low
                       0x00,    # wLangId High
                       0x00,    # bMsgIndex
                       0x00,    # bTeoPrologue[0]
                       0x00,    # bTeoPrologue[1]
                       0x00     # bTeoPrologue[2]
                       ]
        if self.fixed > 0:
            apdu += str.ljust('', self.fixed, '\xff')
        else:
            apdu += self.possibly_add_dummy_byte()
        pin_verify += [ len(apdu), 0, 0, 0 ] + apdu
        data = self.connection.control(self.verify_ioctl,pin_verify)
        sw1 = data[0]
        sw2 = data[1]
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("cmd_verify_pinpad %02x %02x" % (sw1, sw2))

    def send_modify_pinpad(self, apdu, single_step, command):
        if self.modify_ioctl == -1:
            raise ValueError, "Not supported"
        pin_modify = [ 0x00, # bTimerOut
                       0x00, # bTimerOut2
                       0x82, # bmFormatString: Byte, pos=0, left, ASCII.
                       self.fixed, # bmPINBlockString
                       0x00, # bmPINLengthFormat
                       0x00, # bInsertionOffsetOld
                       self.fixed, # bInsertionOffsetNew
                       self.pinmax, # wPINMaxExtraDigit Low  (PINmax)
                       self.pinmin, # wPINMaxExtraDigit High (PINmin) 
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
        if self.fixed > 0:
            apdu += str.ljust('', 2*self.fixed, '\xff')
        else:
            apdu += self.possibly_add_dummy_byte()
        pin_modify += [ len(apdu), 0, 0, 0 ] + apdu
        data = self.connection.control(self.modify_ioctl,pin_modify)
        sw1 = data[0]
        sw2 = data[1]
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%s %02x %02x" % (command, sw1, sw2))

    def cmd_reset_retry_counter(self, who, data):
        if who == BY_ADMIN:
            apdu = [0x00, 0x2c, 0x02, 0x81, len(data) ] + data # BY_ADMIN
        else:
            apdu = [0x00, 0x2c, 0x00, 0x81, len(data) ] + data # BY_USER with resetcode
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("cmd_reset_retry_counter %02x %02x" % (sw1, sw2))

    # Note: CCID specification doesn't permit this (only 0x20 and 0x24)
    def cmd_reset_retry_counter_pinpad(self, who):
        if who == BY_ADMIN:
            apdu = [0x00, 0x2c, 0x02, 0x81] # BY_ADMIN
            self.send_modify_pinpad(apdu, True, "cmd_reset_retry_counter_pinpad")
        else:
            apdu = [0x00, 0x2c, 0x00, 0x81] # BY_USER with resetcode
            self.send_modify_pinpad(apdu, False, "cmd_reset_retry_counter_pinpad")

    def cmd_put_resetcode(self, data):
        apdu = [0x00, 0xda, 0x00, 0xd3, len(data) ] + data # BY_ADMIN
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("cmd_put_resetcode %02x %02x" % (sw1, sw2))

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

def main(who, method, add_a_byte, pinmin, pinmax, change_by_two_steps, fixed):
    card = Card(add_a_byte, pinmin, pinmax, fixed)
    card.connection.connect()

    print "Reader/Token:", card.connection.getReader()
    print "ATR:", toHexString( card.connection.getATR() )

    card.get_features()

    card.cmd_select_openpgp()
    if method == "verify":
        if who == BY_USER:
            print "Please input User's PIN"
        else:
            print "Please input Admin's PIN"
        card.cmd_verify_pinpad(who)
    elif method == "change":
        if change_by_two_steps:
            if who == BY_USER:
                print "Please input User's PIN"
            else:
                print "Please input Admin's PIN"
            card.cmd_verify_pinpad(who)
            if who == BY_USER:
                print "Please input New User's PIN twice"
            else:
                print "Please input New Admin's PIN twice"
            card.cmd_change_reference_data_pinpad(who, True)
        else:
            if who == BY_USER:
                print "Please input User's PIN"
                print "and New User's PIN twice"
            else:
                print "Please input Admin's PIN"
                print "and New Admin's PIN twice"
            card.cmd_change_reference_data_pinpad(who, False)
    elif method == "unblock":
        if change_by_two_steps:
            # It means using keyboard for new PIN
            if who == BY_USER:
                resetcode=s2l(getpass("Please input reset code from keyboard: "))
                newpin=s2l(getpass("Please input New User's PIN from keyboard: "))
                card.cmd_reset_retry_counter(who,resetcode+newpin)
            else:
                print "Please input Admin's PIN"
                card.cmd_verify_pinpad(BY_ADMIN)
                newpin=s2l(getpass("Please input New User's PIN from keyboard: "))
                card.cmd_reset_retry_counter(who,newpin)
        else:
            if who == BY_USER:
                print "Please input reset code"
                print "and New User's PIN twice"
            else:
                print "Please input Admin's PIN"
                card.cmd_verify_pinpad(BY_ADMIN)
                print "Please input New User's PIN twice"
            card.cmd_reset_retry_counter_pinpad(who)
    elif method == "put":
        if change_by_two_steps:
            # It means using keyboard for new PIN
            print "Please input Admin's PIN"
            card.cmd_verify_pinpad(BY_ADMIN)
            resetcode=s2l(getpass("Please input New Reset Code from keyboard: "))
            card.cmd_put_resetcode(resetcode)
        else:
            print "Please input Admin's PIN"
            card.cmd_verify_pinpad(BY_ADMIN)
            print "Please input New Reset Code twice"
            card.cmd_put_resetcode_pinpad()
    else:
        raise ValueError, method
    card.connection.disconnect()

    print "OK."
    return 0

def print_usage():
    print "pinpad-test: testing pinentry of PC/SC card reader"
    print "    help:"
    print "\t--help:\t\tthis message"
    print "    method:\t\t\t\t\t\t\t[verify]"
    print "\t--verify:\tverify PIN"
    print "\t--change:\tchange PIN (old PIN, new PIN twice)"
    print "\t--change2:\tchange PIN by two steps (old PIN, new PIN twice)"
    print "\t--unblock:\tunblock PIN (admin PIN/resetcode, new PIN twice)"
    print "\t--unblock2:\tunblock PIN (admin PIN:pinpad, new PIN:kbd)"
    print "\t--put:\t\tsetup resetcode (admin PIN, new PIN twice)"
    print "\t--put2::\t\tsetup resetcode (admin PIN:pinpad, new PIN:kbd)"
    print "    options:"
    print "\t--fixed N:\tUse fixed length input"
    print "\t--admin:\tby administrator\t\t\t[False]"
    print "\t--add:\t\tadd a dummy byte at the end of APDU\t[False]"
    print "\t--pinmin:\tspecify minimum length of PIN\t\t[6]"
    print "\t--pinmax:\tspecify maximum length of PIN\t\t[15]"
    print "EXAMPLES:"
    print "   $ pinpad-test                   # verify user's PIN "
    print "   $ pinpad-test --admin           # verify admin's PIN "
    print "   $ pinpad-test --change          # change user's PIN "
    print "   $ pinpad-test --change --admin  # change admin's PIN "
    print "   $ pinpad-test --change2         # change user's PIN by two steps"
    print "   $ pinpad-test --change2 --admin # change admin's PIN by two steps"
    print "   $ pinpad-test --unblock         # change user's PIN by reset code"
    print "   $ pinpad-test --unblock --admin # change user's PIN by admin's PIN"
    print "   $ pinpad-test --put             # setup resetcode "

if __name__ == '__main__':
    who = BY_USER
    method = "verify"
    add_a_byte = False
    pinmin = PIN_MIN_DEFAULT
    pinmax = PIN_MAX_DEFAULT
    change_by_two_steps = False
    fixed=0
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
        elif option == '--unblock2':
            method = "unblock"
            change_by_two_steps = True
        elif option == '--add':
            add_a_byte = True
        elif option == '--fixed':
            fixed = int(sys.argv[1])
            sys.argv.pop(1)
        elif option == '--pinmin':
            pinmin = int(sys.argv[1])
            sys.argv.pop(1)
        elif option == '--pinmax':
            pinmax = int(sys.argv[1])
            sys.argv.pop(1)
        elif option == '--put':
            method = "put"
        elif option == '--put2':
            method = "put"
            change_by_two_steps = True
        elif option == "verify":
            method = "verify"
        elif option == '--help':
            print_usage()
            exit(0)
        else:
            raise ValueError, option
    main(who, method, add_a_byte, pinmin, pinmax, change_by_two_steps, fixed)

# Failure
# 67 00: Wrong length; no further indication
# 69 82: Security status not satisfied: pin doesn't match
# 69 85: Conditions of use not satisfied
# 6b 00: Wrong parameters P1-P2
# 6b 80
# 64 02: PIN different

# General
# OpenPGP card v2 doesn't support CHANGE REFERENCE DATA in exchanging
# mode (with P1 == 01, replacing PIN).
#   FAIL: --change2 fails with 6b 00 (after input of PIN)
#   FAIL: --change2 --admin fails with 6b 00 (after input of PIN)

# "FSIJ Gnuk (0.16-34006F06) 00 00"
# Works well except --change2
# It could support --put and --unblock, but currently it's disabled.

# "Vasco DIGIPASS 920 [CCID] 00 00"
#   OK: --verify
#   OK: --verify --admin
#   OK: --change
#   OK: --change --admin
#   OK: --unblock
#   FAIL: --unblock --admin fails with 69 85   (after input of PIN)
#   FAIL: --put fails with 6b 80 (before input of resetcode)
#   OK: --put2
#   FAIL: --unblock2 fails with 69 85
#   FAIL: --unblock2 --admin fails with 69 85  (after input of PIN)

# 0c4b:0500 Reiner SCT Kartensysteme GmbH
# "REINER SCT cyberJack RFID standard (7592671050) 00 00"
#   OK: --verify
#   OK: --verify --admin
#   OK: --change
#   OK: --change --admin
#   OK: --unblock
#   OK: --unblock --admin
#   FAIL: --put fails with 69 85

# Gemalto GemPC Pinpad 00 00
# It asks users PIN with --add but it results 67 00
# It seems that it doesn't support variable length PIN
# Firmware version: GemTwRC2-V2.10-GL04

# 072f:90d2 Advanced Card Systems, Ltd
# ACS ACR83U 01 00
# --verify failed with 6b 80

# 08e6:34c2 Gemplus
# Gemalto Ezio Shield PinPad 01 00
# works well
#   FAIL: --unblock2 fails with 6d 00

# 076b:3821 OmniKey AG CardMan 3821
# OmniKey CardMan 3821 01 00
# Works well with --pinmax 31 --pinmin 1

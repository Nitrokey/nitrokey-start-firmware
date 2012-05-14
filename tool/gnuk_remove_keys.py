#! /usr/bin/python

"""
gnuk_remove_keys.py - a tool to remove keys in Gnuk Token

Copyright (C) 2012 Free Software Initiative of Japan
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

import sys, os, string

# Assume only single CCID device is attached to computer and it's Gnuk Token

from smartcard.CardType import AnyCardType
from smartcard.CardRequest import CardRequest
from smartcard.util import toHexString

def s2l(s):
    return [ ord(c) for c in s ]

class GnukToken(object):
    def __init__(self):
        cardtype = AnyCardType()
        cardrequest = CardRequest(timeout=1, cardType=cardtype)
        cardservice = cardrequest.waitforcard()
        self.connection = cardservice.connection

    def cmd_get_response(self, expected_len):
        apdu = [0x00, 0xc0, 0x00, 0x00, expected_len ]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%02x%02x" % (sw1, sw2))
        return response

    def cmd_verify(self, who, passwd):
        apdu = [0x00, 0x20, 0x00, 0x80+who, len(passwd)] + s2l(passwd)
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%02x%02x" % (sw1, sw2))

    def cmd_select_openpgp(self):
        apdu = [0x00, 0xa4, 0x04, 0x0c, 6, 0xd2, 0x76, 0x00, 0x01, 0x24, 0x01 ]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if sw1 == 0x61:
            response = self.cmd_get_response(sw2)
        elif not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%02x%02x" % (sw1, sw2))

    def cmd_put_data_key_import_remove(self, keyno):
        if keyno == 1:
            keyspec = 0xb6      # SIG
        elif keyno == 2:
            keyspec = 0xb8      # DEC
        else
            keyspec = 0xa4      # AUT
        apdu = [0x00, 0xdb, 0x3f, 0xff, 0x4d, 0x02, keyspec, 0x00 ]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if sw1 == 0x61:
            response = self.cmd_get_response(sw2)
        elif not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, ("%02x%02x" % (sw1, sw2))
        return response

DEFAULT_PW3 = "12345678"
BY_ADMIN = 3

def main(fileid, is_update, data, passwd):
    gnuk = GnukToken()

    gnuk.connection.connect()
    print "Token:", gnuk.connection.getReader()
    print "ATR:", toHexString( gnuk.connection.getATR() )

    gnuk.cmd_verify(BY_ADMIN, passwd)
    gnuk.cmd_select_openpgp()
    gnuk.cmd_put_data_key_import_remove(1)
    gnuk.cmd_put_data_key_import_remove(2)
    gnuk.cmd_put_data_key_import_remove(3)

    gnuk.connection.disconnect()
    return 0


if __name__ == '__main__':
    passwd = DEFAULT_PW3
    if sys.argv[1] == '-p':
        from getpass import getpass
        passwd = getpass("Admin password:")
        sys.argv.pop(1)
    main(passwd)

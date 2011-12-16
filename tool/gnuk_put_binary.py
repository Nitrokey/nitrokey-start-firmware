#! /usr/bin/python

"""
gnuk_put_binary.py - a tool to put binary to Gnuk Token
This tool is for importing certificate, updating random number, etc.

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

import sys, os, binascii, string

# INPUT: binary file

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

    def cmd_verify(self, who, passwd):
        apdu = [0x00, 0x20, 0x00, 0x80+who, 0, 0, len(passwd)] + s2l(passwd)
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, "cmd_verify"

    def cmd_write_binary(self, fileid, data, is_update):
        count = 0
        data_len = len(data)
        if is_update:
            ins = 0xd6
        else:
            ins = 0xd0
        while count*256 < data_len:
            if count == 0:
                d = data[:256]
                apdu = [0x00, ins, 0x80+fileid, 0x00, 0, len(d)>>8, len(d)&0xff] + s2l(d)
            else:
                d = data[256*count:256*(count+1)]
                apdu = [0x00, ins, count, 0x00, 0, len(d)>>8, len(d)&0xff] + s2l(d)
            response, sw1, sw2 = self.connection.transmit(apdu)
            if not (sw1 == 0x90 and sw2 == 0x00):
                if is_update:
                    raise ValueError, "cmd_update_binary"
                else:
                    raise ValueError, "cmd_write_binary"
            count += 1

    def cmd_select_openpgp(self):
        apdu = [0x00, 0xa4, 0x04, 0x00, 6, 0xd2, 0x76, 0x00, 0x01, 0x24, 0x01 ]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, "cmd_select_openpgp"

    def cmd_get_data(self, tagh, tagl):
        apdu = [0x00, 0xca, tagh, tagl]
        response, sw1, sw2 = self.connection.transmit(apdu)
        if not (sw1 == 0x90 and sw2 == 0x00):
            raise ValueError, "cmd_get_data"
        return response

def compare(data_original, data_in_device):
    i = 0 
    for d in data_original:
        if ord(d) != data_in_device[i]:
            raise ValueError, "verify failed at %08x" % i
        i += 1

DEFAULT_PW3 = "12345678"
BY_ADMIN = 3

def main(fileid, is_update, data, passwd):
    gnuk = GnukToken()

    gnuk.connection.connect()
    print "Token:", gnuk.connection.getReader()
    print "ATR:", toHexString( gnuk.connection.getATR() )

    gnuk.cmd_verify(BY_ADMIN, passwd)
    gnuk.cmd_write_binary(fileid, data, is_update)
    if fileid == 0:
        gnuk.cmd_select_openpgp()
        data_in_device = gnuk.cmd_get_data(0x7f, 0x21)
        compare(data[:-2], data_in_device)
    elif fileid == 2:
        gnuk.cmd_select_openpgp()
        data_in_device = gnuk.cmd_get_data(0x00, 0x4f)
        for d in data_in_device:
            print "%02x" % d,
        print
        compare(data, data_in_device[8:])

    gnuk.connection.disconnect()
    return 0


if __name__ == '__main__':
    passwd = DEFAULT_PW3
    if sys.argv[1] == '-p':
        from getpass import getpass
        passwd = getpass("Admin password:")
        sys.argv.pop(1)
    if sys.argv[1] == '-u':
        is_update = True
        sys.argv.pop(1)
    else:
        is_update = False
    if sys.argv[1] == '-s':
        fileid = 2              # serial number
        filename = sys.argv[2]
        f = open(filename)
        email = os.environ['EMAIL']
        serial_data_hex = None
        for line in f.readlines():
            field = string.split(line)
            if field[0] == email:
                serial_data_hex = field[1].replace(':','')
        f.close()
        if not serial_data_hex:
            print "No serial number"
            exit(1)
        print "Writing serial number"
        data = binascii.unhexlify(serial_data_hex)
    elif sys.argv[1] == '-r':
        fileid = 1              # Random number bits
        if len(sys.argv) == 3:
            filename = sys.argv[2]
            f = open(filename)
        else:
            filename = stdin
            f = sys.stdin
        data = f.read()
        f.close()
        print "%s: %d" % (filename, len(data))
        print "Updating random bits"
    else:
        fileid = 0              # Card holder certificate
        filename = sys.argv[1]
        f = open(filename)
        data = f.read()
        f.close()
        print "%s: %d" % (filename, len(data))
        data += "\x90\x00"
        print "Updating card holder certificate"
    main(fileid, is_update, data, passwd)

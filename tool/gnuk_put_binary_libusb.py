#! /usr/bin/python

"""
gnuk_put_binary.py - a tool to put binary to Gnuk Token
This tool is for importing certificate, writing serial number, etc.

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

from struct import *
import sys, time, os, binascii
from gnuk_token import *

# INPUT: binary file

# Assume only single CCID device is attached to computer, and it's Gnuk Token

DEFAULT_PW3 = "1234567890abcd"
BY_ADMIN = 3

def main(fileid, is_update, data, passwd):
    gnuk = None
    for (dev, config, intf) in gnuk_devices():
        try:
            gnuk = gnuk_token(dev, config, intf)
            print("Device: %s" % dev.filename)
            print("Configuration: %d" % config.value)
            print("Interface: %d" % intf.interfaceNumber)
            break
        except:
            pass
    if gnuk.icc_get_status() == 2:
        raise ValueError("No ICC present")
    elif gnuk.icc_get_status() == 1:
        gnuk.icc_power_on()
    gnuk.cmd_select_openpgp()
    gnuk.cmd_verify(BY_ADMIN, passwd.encode('UTF-8'))
    gnuk.cmd_write_binary(fileid, data, is_update)
    gnuk.cmd_select_openpgp()
    if fileid == 0:
        data_in_device = gnuk.cmd_get_data(0x00, 0x4f)
        print(' '.join([ "%02x" % d for d in data_in_device ]))
        compare(data + b'\x00\x00', data_in_device[8:].tostring())
    elif fileid >= 1 and fileid <= 4:
        data_in_device = gnuk.cmd_read_binary(fileid)
        compare(data, data_in_device)
    else:
        data_in_device = gnuk.cmd_get_data(0x7f, 0x21)
        compare(data, data_in_device)
    gnuk.icc_power_off()
    return 0

if __name__ == '__main__':
    passwd = DEFAULT_PW3
    if sys.argv[1] == '-p':
        from getpass import getpass
        passwd = getpass("Admin password: ")
        sys.argv.pop(1)
    if sys.argv[1] == '-u':
        is_update = True
        sys.argv.pop(1)
    else:
        is_update = False
    if sys.argv[1] == '-s':
        fileid = 0              # serial number
        filename = sys.argv[2]
        f = open(filename)
        email = os.environ['EMAIL']
        serial_data_hex = None
        for line in f.readlines():
            field = str.split(line)
            if field[0] == email:
                serial_data_hex = field[1].replace(':','')
        f.close()
        if not serial_data_hex:
            print("No serial number")
            exit(1)
        print("Writing serial number")
        data = binascii.unhexlify(serial_data_hex)
    elif sys.argv[1] == '-k':   # firmware update key
        keyno = sys.argv[2]
        fileid = 1 + int(keyno)
        filename = sys.argv[3]
        f = open(filename, "rb")
        data = f.read()
        f.close()
    else:
        fileid = 5              # Card holder certificate
        filename = sys.argv[1]
        f = open(filename, "rb")
        data = f.read()
        f.close()
        print("%s: %d" % (filename, len(data)))
        print("Updating card holder certificate")
    main(fileid, is_update, data, passwd)

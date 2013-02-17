#! /usr/bin/python

"""
upgrade_by_passwd.py - a tool to install another firmware for Gnuk Token
                       which is just shipped from factory

Copyright (C) 2012, 2013 Free Software Initiative of Japan
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

from gnuk_token import *
import sys, binascii, time, os
import rsa

DEFAULT_PW3 = "12345678"
BY_ADMIN = 3

KEYNO_FOR_AUTH=2 

def main(passwd, data_regnual, data_upgrade):
    l = len(data_regnual)
    if (l & 0x03) != 0:
        data_regnual = data_regnual.ljust(l + 4 - (l & 0x03), chr(0))
    crc32code = crc32(data_regnual)
    print "CRC32: %04x\n" % crc32code
    data_regnual += pack('<I', crc32code)

    rsa_key = rsa.read_key_from_file('rsa_example.key')
    rsa_raw_pubkey = rsa.get_raw_pubkey(rsa_key)

    gnuk = get_gnuk_device()
    gnuk.cmd_verify(BY_ADMIN, passwd)
    keyno = 0
    gnuk.cmd_write_binary(1+keyno, rsa_raw_pubkey, False)

    gnuk.cmd_select_openpgp()
    challenge = gnuk.cmd_get_challenge()
    digestinfo = binascii.unhexlify(SHA256_OID_PREFIX) + challenge
    signed = rsa.compute_signature(rsa_key, digestinfo)
    signed_bytes = rsa.integer_to_bytes_256(signed)
    gnuk.cmd_external_authenticate(keyno, signed_bytes)
    gnuk.stop_gnuk()
    mem_info = gnuk.mem_info()
    print "%08x:%08x" % mem_info

    print "Downloading flash upgrade program..."
    gnuk.download(mem_info[0], data_regnual)
    print "Run flash upgrade program..."
    gnuk.execute(mem_info[0] + len(data_regnual) - 4)
    #
    time.sleep(3)
    gnuk.reset_device()
    del gnuk
    gnuk = None
    #
    print "Wait 3 seconds..."
    time.sleep(3)
    # Then, send upgrade program...
    reg = None
    for dev in gnuk_devices_by_vidpid():
        try:
            reg = regnual(dev)
            print "Device: ", dev.filename
            break
        except:
            pass
    mem_info = reg.mem_info()
    print "%08x:%08x" % mem_info
    print "Downloading the program"
    reg.download(mem_info[0], data_upgrade)
    reg.protect()
    reg.finish()
    reg.reset_device()
    return 0

if __name__ == '__main__':
    if os.getcwd() != os.path.dirname(os.path.abspath(__file__)):
        print "Please change working directory to: %s" % os.path.dirname(os.path.abspath(__file__))
        exit(1)

    passwd = DEFAULT_PW3
    if len(sys.argv) > 1 and sys.argv[1] == '-p':
        from getpass import getpass
        passwd = getpass("Admin password: ")
        sys.argv.pop(1)
    filename_regnual = sys.argv[1]
    filename_upgrade = sys.argv[2]
    f = open(filename_regnual)
    data_regnual = f.read()
    f.close()
    print "%s: %d" % (filename_regnual, len(data_regnual))
    f = open(filename_upgrade)
    data_upgrade = f.read()
    f.close()
    print "%s: %d" % (filename_upgrade, len(data_upgrade))
    # First 4096-byte in data_upgrade is SYS, so, skip it.
    main(passwd, data_regnual, data_upgrade[4096:])

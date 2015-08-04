#! /usr/bin/python

"""
gnuk_upgrade.py - a tool to upgrade firmware of Gnuk Token

Copyright (C) 2012, 2015  Free Software Initiative of Japan
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

# INPUT: binary files (regnual_image, upgrade_firmware_image)

# Assume only single CCID device is attached to computer, and it's Gnuk Token

import usb

from gnuk_token import *

from subprocess import check_output

SHA256_OID_PREFIX="3031300d060960864801650304020105000420"

# When user specify KEYGRIP, use it.  Or else, connect to SCD directly.
def gpg_sign(keygrip, hash):
    if keygrip:
        result = check_output(["gpg-connect-agent",
                               "SIGKEY %s" % keygrip,
                               "SETHASH --hash=sha256 %s" % hash,
                               "PKSIGN --hash=sha256", "/bye"])
    else:
        result = check_output(["gpg-connect-agent",
                               "SCD SETDATA " + SHA256_OID_PREFIX + hash,
                               "SCD PKAUTH OPENPGP.3",
                               "/bye"])
    signed = ""
    while True:
        i = result.find('%')
        if i < 0:
            signed += result
            break
        hex_str = result[i+1:i+3]
        signed += result[0:i]
        signed += chr(int(hex_str,16))
        result = result[i+3:]

    if keygrip:
        pos = signed.index("D (7:sig-val(3:rsa(1:s256:") + 26
        signed = signed[pos:-7]
    else:
        pos = signed.index("D ") + 2
        signed = signed[pos:-4]     # \nOK\n
    if len(signed) != 256:
        raise ValueError(binascii.hexlify(signed))
    return signed

def main(keyno,keygrip, data_regnual, data_upgrade):
    l = len(data_regnual)
    if (l & 0x03) != 0:
        data_regnual = data_regnual.ljust(l + 4 - (l & 0x03), b'\x00')
    crc32code = crc32(data_regnual)
    print("CRC32: %04x\n" % crc32code)
    data_regnual += pack('<I', crc32code)
    for (dev, config, intf) in gnuk_devices():
        try:
            icc = gnuk_token(dev, config, intf)
            print("Device: %s" % dev.filename)
            print("Configuration: %d" % config.value)
            print("Interface: %d" % intf.interfaceNumber)
            break
        except:
            icc = None
    if icc.icc_get_status() == 2:
        raise ValueError("No ICC present")
    elif icc.icc_get_status() == 1:
        icc.icc_power_on()
    icc.cmd_select_openpgp()
    challenge = icc.cmd_get_challenge().tostring()
    signed = gpg_sign(keygrip, binascii.hexlify(challenge))
    icc.cmd_external_authenticate(keyno, signed)
    icc.stop_gnuk()
    mem_info = icc.mem_info()
    print("%08x:%08x" % mem_info)
    print("Downloading flash upgrade program...")
    icc.download(mem_info[0], data_regnual)
    print("Run flash upgrade program...")
    icc.execute(mem_info[0] + len(data_regnual) - 4)
    #
    time.sleep(3)
    icc.reset_device()
    del icc
    icc = None
    #
    print("Wait 3 seconds...")
    time.sleep(3)
    # Then, send upgrade program...
    reg = None
    for dev in gnuk_devices_by_vidpid():
        try:
            reg = regnual(dev)
            print("Device: %d" % dev.filename)
            break
        except:
            pass
    mem_info = reg.mem_info()
    print("%08x:%08x" % mem_info)
    print("Downloading the program")
    reg.download(mem_info[0], data_upgrade)
    reg.protect()
    reg.finish()
    reg.reset_device()
    return 0


if __name__ == '__main__':
    keyno = 0
    keygrip = None
    if sys.argv[1] == '-k':
        sys.argv.pop(1)
        keygrip = sys.argv[1]
        sys.argv.pop(1)
    filename_regnual = sys.argv[1]
    filename_upgrade = sys.argv[2]
    f = open(filename_regnual, "rb")
    data_regnual = f.read()
    f.close()
    print("%s: %d" % (filename_regnual, len(data_regnual)))
    f = open(filename_upgrade, "rb")
    data_upgrade = f.read()
    f.close()
    print("%s: %d" % (filename_upgrade, len(data_upgrade)))
    main(keyno, keygrip, data_regnual, data_upgrade[4096:])

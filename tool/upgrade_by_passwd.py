#! /usr/bin/python3

"""
upgrade_by_passwd.py - a tool to install another firmware for Gnuk Token
                       which is just shipped from factory

Copyright (C) 2012, 2013, 2015, 2018
              Free Software Initiative of Japan
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
from subprocess import check_output

from gnuk_token import get_gnuk_device, gnuk_devices_by_vidpid, \
     gnuk_token, regnual, SHA256_OID_PREFIX, crc32, parse_kdf_data
from kdf_calc import kdf_calc

import sys, binascii, time, os
import rsa
from struct import pack

DEFAULT_PW3 = "12345678"
BY_ADMIN = 3

KEYNO_FOR_AUTH=2 

def main(wait_e, keyno, passwd, data_regnual, data_upgrade):
    l = len(data_regnual)
    if (l & 0x03) != 0:
        data_regnual = data_regnual.ljust(l + 4 - (l & 0x03), chr(0))
    crc32code = crc32(data_regnual)
    print("CRC32: %04x\n" % crc32code)
    data_regnual += pack('<I', crc32code)

    rsa_key = rsa.read_key_from_file('rsa_example.key')
    rsa_raw_pubkey = rsa.get_raw_pubkey(rsa_key)

    gnuk = get_gnuk_device()
    gnuk.cmd_select_openpgp()
    # Compute passwd data
    try:
        kdf_data = gnuk.cmd_get_data(0x00, 0xf9).tostring()
    except:
        kdf_data = b""
    if kdf_data == b"":
        passwd_data = passwd.encode('UTF-8')
    else:
        algo, subalgo, iters, salt_user, salt_reset, salt_admin, \
            hash_user, hash_admin = parse_kdf_data(kdf_data)
        if salt_admin:
            salt = salt_admin
        else:
            salt = salt_user
        passwd_data = kdf_calc(passwd, salt, iters)
    # And authenticate with the passwd data
    gnuk.cmd_verify(BY_ADMIN, passwd_data)
    gnuk.cmd_write_binary(1+keyno, rsa_raw_pubkey, False)

    gnuk.cmd_select_openpgp()
    challenge = gnuk.cmd_get_challenge().tostring()
    digestinfo = binascii.unhexlify(SHA256_OID_PREFIX) + challenge
    signed = rsa.compute_signature(rsa_key, digestinfo)
    signed_bytes = rsa.integer_to_bytes_256(signed)
    gnuk.cmd_external_authenticate(keyno, signed_bytes)
    gnuk.stop_gnuk()
    mem_info = gnuk.mem_info()
    print("%08x:%08x" % mem_info)

    print("Downloading flash upgrade program...")
    gnuk.download(mem_info[0], data_regnual)
    print("Run flash upgrade program...")
    gnuk.execute(mem_info[0] + len(data_regnual) - 4)
    #
    time.sleep(3)
    gnuk.reset_device()
    del gnuk
    gnuk = None
    #
    reg = None
    print("Waiting for device to appear:")
    while reg == None:
        print("  Wait {} second{}...".format(wait_e, 's' if wait_e > 1 else ''))
        time.sleep(wait_e)
        for dev in gnuk_devices_by_vidpid():
            try:
                reg = regnual(dev)
                print("Device: %s" % dev.filename)
                break
            except:
                pass
    # Then, send upgrade program...
    mem_info = reg.mem_info()
    print("%08x:%08x" % mem_info)
    print("Downloading the program")
    reg.download(mem_info[0], data_upgrade)
    print("Protecting device")
    reg.protect()
    print("Finish flashing")
    reg.finish()
    print("Resetting device")
    reg.reset_device()
    print("Update procedure finished")
    return 0

from getpass import getpass

# This should be event driven, not guessing some period, or polling.
DEFAULT_WAIT_FOR_REENUMERATION=1

def validate_binary_file(path: str):
    import os.path
    if not os.path.exists(path):
        raise argparse.ArgumentTypeError('Path does not exist: "{}"'.format(path))
    if not path.endswith('.bin'):
        raise argparse.ArgumentTypeError('Supplied file "{}" does not have ".bin" extension. Make sure you are sending correct file to the device.'.format(os.path.basename(path)))
    return path

def validate_name(path: str, name: str):
    if name not in path:
        raise argparse.ArgumentTypeError('Supplied file "{}" does not have "{}" in name. Make sure you have not swapped the arguments.'.format(os.path.basename(path), name))
    return path

def validate_gnuk(path: str):
    validate_binary_file(path)
    validate_name(path, 'gnuk')
    return path

def validate_regnual(path: str):
    validate_binary_file(path)
    validate_name(path, 'regnual')
    return path


if __name__ == '__main__':
    if os.getcwd() != os.path.dirname(os.path.abspath(__file__)):
        print("Please change working directory to: %s" % os.path.dirname(os.path.abspath(__file__)))
        exit(1)

    import argparse
    parser = argparse.ArgumentParser(description='Update tool for GNUK')
    parser.add_argument('regnual', type=validate_regnual, help='path to regnual binary')
    parser.add_argument('gnuk', type=validate_gnuk, help='path to gnuk binary')
    parser.add_argument('-f', dest='default_password', action='store_true',
                        default=False, help='use default Admin password: {}'.format(DEFAULT_PW3))
    parser.add_argument('-e', dest='wait_e', default=DEFAULT_WAIT_FOR_REENUMERATION, type=int, help='time to wait for device to enumerate, after regnual was executed on device')
    parser.add_argument('-k', dest='keyno', default=0, type=int, help='selected key index')
    args = parser.parse_args()

    keyno = args.keyno
    passwd = None
    wait_e = args.wait_e

    if args.default_password:  # F for Factory setting
        passwd = DEFAULT_PW3
    if not passwd:
        try:
            passwd = getpass("Admin password: ")
        except:
            print('Quitting')
            exit(2)


    f = open(args.regnual,"rb")
    data_regnual = f.read()
    f.close()
    print("{}: {}".format(args.regnual, len(data_regnual)))
    f = open(args.gnuk,"rb")
    data_upgrade = f.read()
    f.close()
    print("{}: {}".format(args.gnuk, len(data_upgrade)))

    from usb_strings import get_devices, print_device

    dev_strings = get_devices()
    if len(dev_strings) > 1:
        print('Only one device should be connected. Please remove other devices and retry.')
        exit(1)

    print('Currently connected device strings:')
    print_device(dev_strings[0])

    update_done = False
    for attempt_counter in range(1):
        try:
            # First 4096-byte in data_upgrade is SYS, so, skip it.
            main(wait_e, keyno, passwd, data_regnual, data_upgrade[4096:])
            update_done = True
            break
        except ValueError as e:
            if 'No ICC present' in str(e):
                print('*** Could not connect to the device. Attempting to close scdaemon.')
                print('*** Running: gpg-connect-agent "SCD KILLSCD" "SCD BYE" /bye')
                result = check_output(["gpg-connect-agent",
                                       "SCD KILLSCD", "SCD BYE", "/bye"])
                time.sleep(1)
                print('*** Please try again...')
            else:
                print('*** Could not proceed with the update. '
                      'Please try again, and make sure the entered password is correct.')
                break

        except:
            # unknown error, bail
            break

    if not update_done:
        print('*** Could not proceed with the update. Please close other applications, that possibly use it (e.g. scdaemon, pcscd) and try again.')
        exit(1)


    dev_strings_upgraded = None
    print('Currently connected device strings (after upgrade):')
    for i in range(10):
        time.sleep(1)
        dev_strings_upgraded = get_devices()
        if len(dev_strings_upgraded) > 0: break
        print('.')
    print_device(dev_strings_upgraded[0])

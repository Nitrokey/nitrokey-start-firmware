#! /usr/bin/python3

from collections import defaultdict
from subprocess import check_output

from gnuk_token import get_gnuk_device, gnuk_devices_by_vidpid, \
    gnuk_token, regnual, SHA256_OID_PREFIX, crc32, parse_kdf_data
from kdf_calc import kdf_calc
from usb.core import USBError

import sys, binascii, time, os
import rsa
from struct import pack

def usage():
    print("Usage: \npython3 set_identity.py id")
    print("id must be 0, 1, or 2")

if __name__=="__main__":
    print("set_identity.py selects identity on a nitrokey start")
    if len(sys.argv)!=2:
        print("missing identity number")
        usage();
        sys.exit(1)
    if not sys.argv[1].isdigit():
        print("identity number must be a digit")
        usage();
        sys.exit(1)
    identity=int(sys.argv[1])
    if(identity<0 or identity>2):
        print("identity must be 0, 1 or 2")
        usage();
        sys.exit(1)
    print("Trying to set identity to %d"%(identity,))
    for x in range(3):
        try:
            gnuk = get_gnuk_device()
            gnuk.cmd_select_openpgp()
            try:
                gnuk.cmd_set_identity(identity)
            except USBError:
                print("device has reset, and should now have the new identity")
                sys.exit(0);
                
        except ValueError as e:
            if 'No ICC present' in str(e):
                print("Could not connect to device, trying to close scdaemon")
                result = check_output(["gpg-connect-agent",
                                           "SCD KILLSCD", "SCD BYE", "/bye"])
                time.sleep(3)
            else:
                print('*** Found error: {}'.format(str(e)))
    

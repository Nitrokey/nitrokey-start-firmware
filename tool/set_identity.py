#! /usr/bin/python3

import sys
import time
from subprocess import check_output

from gnuk_token import get_gnuk_device
from usb.core import USBError

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("missing identity number")
        sys.exit(1)
    if not sys.argv[1].isdigit():
        print("identity number must be a digit")
        sys.exit(1)
    identity = int(sys.argv[1])
    if identity < 0 or identity > 2:
        print("identity must be 0, 1 or 2")
        sys.exit(1)
    print("Trying to set identity to %d" % (identity,))
    for x in range(3):
        try:
            gnuk = get_gnuk_device()
            gnuk.cmd_select_openpgp()
            try:
                gnuk.cmd_set_identity(identity)
            except USBError:
                print("device has reset, and should now have the new identity")
                sys.exit(0)

        except ValueError as e:
            if 'No ICC present' in str(e):
                print("Could not connect to device, trying to close scdaemon")
                result = check_output(["gpg-connect-agent",
                                       "SCD KILLSCD", "SCD BYE", "/bye"])
                time.sleep(3)
            else:
                print('*** Found error: {}'.format(str(e)))

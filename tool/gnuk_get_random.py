#! /usr/bin/python3

from gnuk_token import get_gnuk_device, gnuk_token
from binascii import hexlify
import sys

if __name__ == '__main__':
    gnuk = get_gnuk_device()
    gnuk.cmd_select_openpgp()
    looping = (len(sys.argv) > 1)
    while True:
        challenge = gnuk.cmd_get_challenge().tostring()
        print(hexlify(challenge))
        if not looping:
            break

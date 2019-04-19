#! /usr/bin/python3

from gnuk_token import get_gnuk_device, gnuk_token
from binascii import hexlify
import sys

if __name__ == '__main__':
    count = 0
    gnuk = get_gnuk_device()
    gnuk.cmd_select_openpgp()
    looping = (len(sys.argv) > 1)
    while True:
        try:
            challenge = gnuk.cmd_get_challenge().tostring()
        except Exception as e:
            print(count)
            raise e
        print(hexlify(challenge))
        count = count + 1
        if not looping:
            break

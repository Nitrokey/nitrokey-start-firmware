#! /usr/bin/python

import sys, binascii
from subprocess import check_output

def get_gpg_public_key(keygrip):
    result = check_output(["gpg-connect-agent", "READKEY %s" % keygrip, "/bye"])
    key = ""
    while True:
        i = result.find('%')
        if i < 0:
            key += result
            break
        hex_str = result[i+1:i+3]
        key += result[0:i]
        key += chr(int(hex_str,16))
        result = result[i+3:]

    pos = key.index("D (10:public-key(3:rsa(1:n257:") + 31 # skip NUL too
    pos_last = key.index(")(1:e3:")
    key = key[pos:pos_last]
    if len(key) != 256:
        raise ValueError, binascii.hexlify(key)
    return key

if __name__ == '__main__':
    keygrip = sys.argv[1]
    k = get_gpg_public_key(keygrip)
    shorthand = keygrip[0:8] + ".bin"
    f = open(shorthand,"w")
    f.write(k)
    f.close()

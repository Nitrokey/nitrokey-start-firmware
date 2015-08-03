from Crypto import Random
from Crypto.PublicKey import RSA
from binascii import hexlify

def print_key_in_hex(k):
    prv = k.exportKey(format='DER', pkcs=8)
    n = prv[38:38+256]
    e = prv[38+256+2:38+256+2+3]
    p = prv[38+256+2+3+4+257+4:38+256+2+3+4+257+4+128]
    q = prv[38+256+2+3+4+257+4+128+4:38+256+2+3+4+257+4+128+4+128]
    n_str = hexlify(n)
    e_str = hexlify(e)
    p_str = hexlify(p)
    q_str = hexlify(q)
    if int(p_str, 16)*int(q_str, 16) != int(n_str, 16):
        raise ValueError("wrong key", k)
    print(n_str)
    print(e_str)
    print(p_str)
    print(q_str)

rng = Random.new().read
key = RSA.generate(2048, rng)

print_key_in_hex(key)

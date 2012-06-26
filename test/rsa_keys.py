from binascii import unhexlify

def read_key_from_file(file):
    f = open(file)
    n_str = f.readline()[:-1]
    e_str = f.readline()[:-1]
    p_str = f.readline()[:-1]
    q_str = f.readline()[:-1]
    f.close()
    e = int(e_str, 16)
    p = int(p_str, 16)
    q = int(q_str, 16)
    n = int(n_str, 16)
    if n != p * q:
        raise ValueError("wrong key", p, q, n)
    return (unhexlify(n_str), unhexlify(e_str), unhexlify(p_str), unhexlify(q_str))

key = [ None, None, None ]
key[0] = read_key_from_file('rsa-sig.key')
key[1] = read_key_from_file('rsa-dec.key')
key[2] = read_key_from_file('rsa-aut.key')

def build_privkey_template(openpgp_keyno, keyno):
    n_str = key[keyno][0]
    e_str = '\x00' + key[keyno][1]
    p_str = key[keyno][2]
    q_str = key[keyno][3]

    if openpgp_keyno == 1:
        keyspec = '\xb6'
    elif openpgp_keyno == 2:
        keyspec = '\xb8'
    else:
        keyspec = '\xa4'

    key_template = '\x91\x04'+ '\x92\x81\x80' + '\x93\x81\x80' 

    exthdr = keyspec + '\x00' + '\x7f\x48' + '\x08' + key_template

    suffix = '\x5f\x48' + '\x82\x01\x04'

    t = '\x4d' + '\x82\01\16' + exthdr + suffix + e_str + p_str + q_str
    return t

def build_privkey_template_for_remove(openpgp_keyno):
    if openpgp_keyno == 1:
        keyspec = '\xb6'
    elif openpgp_keyno == 2:
        keyspec = '\xb8'
    else:
        keyspec = '\xa4'
    return '\x4d\02' + keyspec + '\0x00'

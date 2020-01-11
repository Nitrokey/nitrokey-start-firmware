"""
add_openpgp_authkey_from_gpgssh.py

Copyright (C) 2014 Free Software Initiative of Japan
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

from gpg_agent import gpg_agent
from binascii import hexlify, unhexlify
from sexp import sexp
from time import time
from struct import pack, unpack
from hashlib import sha1, sha256
import re

ALGO_RSA=1
DIGEST_SHA256=8
OPENPGP_VERSION=4

def count_bits(mpi_bytes):
    return ord(mpi_bytes[0]).bit_length()+(len(mpi_bytes)-1)*8

class rsa_key(object):
    def __init__(self, timestamp, n, e):
        self.__timestamp = timestamp
        self.__n = n
        self.__e = e

    def hash_pubkey_key(self, md):
        hl = 6 + len(self.__n) + 2 + len(self.__e) + 2
        md.update(pack('>BHBLB', 0x99, hl, 4, self.__timestamp, ALGO_RSA))
        md.update(pack('>H', count_bits(self.__n)) + self.__n)
        md.update(pack('>H', count_bits(self.__e)) + self.__e)

    def compose_public_subkey_packet(self):
        psp = pack('>BLB', OPENPGP_VERSION, self.__timestamp, ALGO_RSA)
        psp += pack('>H', count_bits(self.__n)) + self.__n
        psp += pack('>H', count_bits(self.__e)) + self.__e
        return '\xB9' + pack('>H', len(psp)) + psp

    def compute_keygrip(self):
        md = sha1("\x00" + self.__n)
        return md.digest()

    def compute_fpr(self):
        md = sha1()
        self.hash_pubkey_key(md)
        return md.digest()

def compose_binding_signature_packet(g, primary_key, subkey, sig_timestamp):
    # Binding signature packet consists of: subpackets of hashed and unhashed
    # (1) hashed subpacket: this subpacket is the target to calculate digest
    sig_subp_hashed = pack('>B', 5) + '\x02' + pack('>L', sig_timestamp)
    sig_subp_hashed += pack('>B', 2) + '\x1b' + '\x20'     # Usage AUTH
    # (2) unhashed subpacket: this subpacket is _not_ the target for digest
    sig_subp_unhashed = pack('>B', 9) + '\x10' + primary_key.compute_fpr()[-8:]
    #
    md = sha256()
    primary_key.hash_pubkey_key(md)
    subkey.hash_pubkey_key(md)
    # Start building binding signature packet, starting OPENPGP_VERSION...
    sigp = pack('>BBBB', OPENPGP_VERSION, 0x18, ALGO_RSA, DIGEST_SHA256)
    sigp += pack('>H', len(sig_subp_hashed)) + sig_subp_hashed
    # And feed it to digest calculator
    md.update(sigp)
    md.update(pack('>BBL', OPENPGP_VERSION, 0xff, len(sig_subp_hashed)+6))
    digest = md.digest()
    # Then, add unhashed subpacket and first two bytes of digest
    sigp += pack('>H', len(sig_subp_unhashed)) + sig_subp_unhashed
    sigp += digest[0:2]
    print("Digest 2-byte: %s" % hexlify(digest[0:2]))
    # Ask signing to this digest by the corresponding secret key to PRIMARY_KEY
    signature = do_sign(g, primary_key, DIGEST_SHA256, digest)
    # Then, add the signature to the binding signature packet
    sigp += pack('>H', count_bits(signature)) + signature
    # Prepending header, it's the binding signature packet
    return '\x89' + pack('>H', len(sigp)) + sigp

def build_rsakey_from_ssh_key_under_gpg_agent(g, timestamp=None):
    # (1) Get the list of available key specifying '--with-ssh'
    g.send_command("KEYINFO --list --with-ssh --data\n")
    kl_str = g.get_response()
    kl_str = kl_str[0:-1]
    kl = kl_str.split('\n')
    # (2) Select SSH key(s)
    kl_ssh = [kg for kg in kl if re.search("S$", kg)] # Select SSH key
    # (3) Use the first entry of the list (in future, use all???)
    print("KEYINFO: %s" % kl_ssh[0])
    # KG: The keygrip of key in question
    kg = kl_ssh[0].split(' ')[0]
    # By READKEY command, get the public key information of KG
    g.send_command("READKEY %s\n" % kg)
    pubkey_info_str = g.get_response()
    # The information is in SEXP format, extract N and E
    s = sexp(pubkey_info_str)
    if s[0] != 'public-key':
        print(s)
        exit(1)
    rsa = s[1]
    if rsa[0] != 'rsa':
        print(rsa)
        exit(1)
    n_x = rsa[1]
    if n_x[0] != 'n':
        print(n_x)
        exit(1)
    n_byte_str = n_x[1]
    while n_byte_str[0] == '\x00':
        n_byte_str = n_byte_str[1:]
    n = n_byte_str
    e_x = rsa[2]
    if e_x[0] != 'e':
        print(e_x)
        exit(1)
    e = e_x[1]
    if not timestamp:
        timestamp = int(time()) 
    # Compose our RSA_KEY by TIMESTAMP, N, and E
    return rsa_key(timestamp,n,e)

BUFSIZE=1024
def build_rsakey_from_openpgp_file(filename):
    f = open(filename, "rb")
    openpgp_bytes = f.read(BUFSIZE)
    f.close()
    header_tag, packet_len, version, timestamp, algo, n_bitlen = \
        unpack('>BHBLBH', openpgp_bytes[:11])
    if header_tag != 0x99:
        print ("openpgp: 0x99 expected (0x%02x)" % header_tag)
        exit(1)
    n_len = (n_bitlen + 7) / 8
    n = openpgp_bytes[11:11+n_len]
    e_bitlen = unpack('>H', openpgp_bytes[11+n_len:11+n_len+2])[0]
    e_len = (e_bitlen + 7) / 8
    e = openpgp_bytes[11+n_len+2:11+n_len+2+e_len]
    return rsa_key(timestamp,n,e)

def do_sign(g, pubkey, digest_algo, digest):
    g.send_command('SIGKEY %s\n' % hexlify(pubkey.compute_keygrip()))
    if digest_algo == DIGEST_SHA256:
        g.send_command('SETHASH --hash=sha256 %s\n' % hexlify(digest))
    else:
        raise('Unknown digest algorithm', digest_algo)
    g.send_command('PKSIGN\n')
    sig_result_str = g.get_response()
    sig_sexp = sexp(sig_result_str)  # [ "sig-val" [ "rsa" [ "s" "xxx" ] ] ]
    return sig_sexp[1][1][1]

import sys

if __name__ == '__main__':
    #
    filename = sys.argv[1]
    # Connect to GPG-agent:
    g = gpg_agent()
    print("GPG-agent says: %s" % g.read_line())
    #
    primary_key = build_rsakey_from_openpgp_file(filename)
    print("Primary key fingerprint: %s" % hexlify(primary_key.compute_fpr()))
    print("Primary keygrip: %s" % hexlify(primary_key.compute_keygrip()))
    #
    subkey = build_rsakey_from_ssh_key_under_gpg_agent(g)
    print("Subkey fingerprint: %s" % hexlify(subkey.compute_fpr()))
    print("Subkey keygrip: %s" % hexlify(subkey.compute_keygrip()))
    #
    openpgp_subkey_packet = subkey.compose_public_subkey_packet()
    openpgp_sig_packet = compose_binding_signature_packet(g, primary_key, subkey, int(time()))
    # Query to GPG-agent finished
    g.close()
    # Append OpenPGP packets to file
    f = open(filename, "ab")
    f.write(openpgp_subkey_packet)
    f.write(openpgp_sig_packet)
    f.close()

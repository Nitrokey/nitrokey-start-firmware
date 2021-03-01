"""
card_test_brainpoolp384r1.py - test brainpoolp384r1 support

Copyright (C) 2021  Vincent Pelletier <plr.vincent@gmail.com>

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

from func_pso_auth import assert_ec_pso
from card_const import *

class Test_Card_BrainpoolP384R1(object):
    def test_ECDH_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(2, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc7027#appendix-A.2
        assert_ec_pso(
            card=card,
            key_index=1,
            key_attributes=KEY_ATTRIBUTES_ECDH_BRAINPOOLP384R1,
            key_attribute_caption='ECDH brainpoolp384r1',
            private_key=(
                b'\x1E\x20\xF5\xE0\x48\xA5\x88\x6F\x1F\x15\x7C\x74\xE9\x1B\xDE\x2B'
                b'\x98\xC8\xB5\x2D\x58\xE5\x00\x3D\x57\x05\x3F\xC4\xB0\xBD\x65\xD6'
                b'\xF1\x5E\xB5\xD1\xEE\x16\x10\xDF\x87\x07\x95\x14\x36\x27\xD0\x42'
            ),
            expected_public_key=(
                b'\x04'
                b'\x68\xB6\x65\xDD\x91\xC1\x95\x80\x06\x50\xCD\xD3\x63\xC6\x25\xF4'
                b'\xE7\x42\xE8\x13\x46\x67\xB7\x67\xB1\xB4\x76\x79\x35\x88\xF8\x85'
                b'\xAB\x69\x8C\x85\x2D\x4A\x6E\x77\xA2\x52\xD6\x38\x0F\xCA\xF0\x68'
                b'\x55\xBC\x91\xA3\x9C\x9E\xC0\x1D\xEE\x36\x01\x7B\x7D\x67\x3A\x93'
                b'\x12\x36\xD2\xF1\xF5\xC8\x39\x42\xD0\x49\xE3\xFA\x20\x60\x74\x93'
                b'\xE0\xD0\x38\xFF\x2F\xD3\x0C\x2A\xB6\x7D\x15\xC8\x5F\x7F\xAA\x59'
            ),
            pso_input=(
                b'\xa6\x66\x7f\x49\x63\x86\x61'
                b'\x04'
                b'\x4D\x44\x32\x6F\x26\x9A\x59\x7A\x5B\x58\xBB\xA5\x65\xDA\x55\x56'
                b'\xED\x7F\xD9\xA8\xA9\xEB\x76\xC2\x5F\x46\xDB\x69\xD1\x9D\xC8\xCE'
                b'\x6A\xD1\x8E\x40\x4B\x15\x73\x8B\x20\x86\xDF\x37\xE7\x1D\x1E\xB4'
                b'\x62\xD6\x92\x13\x6D\xE5\x6C\xBE\x93\xBF\x5F\xA3\x18\x8E\xF5\x8B'
                b'\xC8\xA3\xA0\xEC\x6C\x1E\x15\x1A\x21\x03\x8A\x42\xE9\x18\x53\x29'
                b'\xB5\xB2\x75\x90\x3D\x19\x2F\x8D\x4E\x1F\x32\xFE\x9C\xC7\x8C\x48'
            ),
            expected_pso_output=(
                b'\x0B\xD9\xD3\xA7\xEA\x0B\x3D\x51\x9D\x09\xD8\xE4\x8D\x07\x85\xFB'
                b'\x74\x4A\x6B\x35\x5E\x63\x04\xBC\x51\xC2\x29\xFB\xBC\xE2\x39\xBB'
                b'\xAD\xF6\x40\x37\x15\xC3\x5D\x4F\xB2\xA5\x44\x4F\x57\x5D\x4F\x42'
            ),
        )

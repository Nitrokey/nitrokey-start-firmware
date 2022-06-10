"""
card_test_brainpoolp256r1.py - test brainpoolp256r1 support

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

class Test_Card_BrainpoolP256R1(object):
    def test_ECDH_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(2, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc7027#appendix-A.1
        assert_ec_pso(
            card=card,
            key_index=1,
            key_attributes=KEY_ATTRIBUTES_ECDH_BRAINPOOLP256R1,
            key_attribute_caption='ECDH brainpoolp256r1',
            private_key=(
                b'\x81\xDB\x1E\xE1\x00\x15\x0F\xF2\xEA\x33\x8D\x70\x82\x71\xBE\x38'
                b'\x30\x0C\xB5\x42\x41\xD7\x99\x50\xF7\x7B\x06\x30\x39\x80\x4F\x1D'
            ),
            expected_public_key=(
                b'\x04'
                b'\x44\x10\x6E\x91\x3F\x92\xBC\x02\xA1\x70\x5D\x99\x53\xA8\x41\x4D'
                b'\xB9\x5E\x1A\xAA\x49\xE8\x1D\x9E\x85\xF9\x29\xA8\xE3\x10\x0B\xE5'
                b'\x8A\xB4\x84\x6F\x11\xCA\xCC\xB7\x3C\xE4\x9C\xBD\xD1\x20\xF5\xA9'
                b'\x00\xA6\x9F\xD3\x2C\x27\x22\x23\xF7\x89\xEF\x10\xEB\x08\x9B\xDC'
            ),
            pso_input=(
                b'\xa6\x46\x7f\x49\x43\x86\x41'
                b'\x04'
                b'\x8D\x2D\x68\x8C\x6C\xF9\x3E\x11\x60\xAD\x04\xCC\x44\x29\x11\x7D'
                b'\xC2\xC4\x18\x25\xE1\xE9\xFC\xA0\xAD\xDD\x34\xE6\xF1\xB3\x9F\x7B'
                b'\x99\x0C\x57\x52\x08\x12\xBE\x51\x26\x41\xE4\x70\x34\x83\x21\x06'
                b'\xBC\x7D\x3E\x8D\xD0\xE4\xC7\xF1\x13\x6D\x70\x06\x54\x7C\xEC\x6A'
            ),
            expected_pso_output=(
                b'\x89\xAF\xC3\x9D\x41\xD3\xB3\x27\x81\x4B\x80\x94\x0B\x04\x25\x90'
                b'\xF9\x65\x56\xEC\x91\xE6\xAE\x79\x39\xBC\xE3\x1F\x3A\x18\xBF\x2B'
            ),
        )

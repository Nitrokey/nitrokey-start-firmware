"""
card_test_ansix9p384r1.py - test ansix9p384r1 support

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

class Test_Card_AnsiX9P384R1(object):
    def test_ECDH_reference_vectors(self, card):
        assert card.verify(3, FACTORY_PASSPHRASE_PW3)
        assert card.verify(2, FACTORY_PASSPHRASE_PW1)
        # https://tools.ietf.org/html/rfc5903#section-8.2
        assert_ec_pso(
            card=card,
            key_index=1,
            key_attributes=KEY_ATTRIBUTES_ECDH_ANSIX9P384R1,
            key_attribute_caption='ECDH ansix9p384r1',
            private_key=(
                b'\x09\x9F\x3C\x70\x34\xD4\xA2\xC6\x99\x88\x4D\x73\xA3\x75\xA6\x7F'
                b'\x76\x24\xEF\x7C\x6B\x3C\x0F\x16\x06\x47\xB6\x74\x14\xDC\xE6\x55'
                b'\xE3\x5B\x53\x80\x41\xE6\x49\xEE\x3F\xAE\xF8\x96\x78\x3A\xB1\x94'
            ),
            expected_public_key=(
                b'\x04'
                b'\x66\x78\x42\xD7\xD1\x80\xAC\x2C\xDE\x6F\x74\xF3\x75\x51\xF5\x57'
                b'\x55\xC7\x64\x5C\x20\xEF\x73\xE3\x16\x34\xFE\x72\xB4\xC5\x5E\xE6'
                b'\xDE\x3A\xC8\x08\xAC\xB4\xBD\xB4\xC8\x87\x32\xAE\xE9\x5F\x41\xAA'
                b'\x94\x82\xED\x1F\xC0\xEE\xB9\xCA\xFC\x49\x84\x62\x5C\xCF\xC2\x3F'
                b'\x65\x03\x21\x49\xE0\xE1\x44\xAD\xA0\x24\x18\x15\x35\xA0\xF3\x8E'
                b'\xEB\x9F\xCF\xF3\xC2\xC9\x47\xDA\xE6\x9B\x4C\x63\x45\x73\xA8\x1C'
            ),
            pso_input=(
                b'\xa6\x66\x7f\x49\x63\x86\x61'
                b'\x04'
                b'\xE5\x58\xDB\xEF\x53\xEE\xCD\xE3\xD3\xFC\xCF\xC1\xAE\xA0\x8A\x89'
                b'\xA9\x87\x47\x5D\x12\xFD\x95\x0D\x83\xCF\xA4\x17\x32\xBC\x50\x9D'
                b'\x0D\x1A\xC4\x3A\x03\x36\xDE\xF9\x6F\xDA\x41\xD0\x77\x4A\x35\x71'
                b'\xDC\xFB\xEC\x7A\xAC\xF3\x19\x64\x72\x16\x9E\x83\x84\x30\x36\x7F'
                b'\x66\xEE\xBE\x3C\x6E\x70\xC4\x16\xDD\x5F\x0C\x68\x75\x9D\xD1\xFF'
                b'\xF8\x3F\xA4\x01\x42\x20\x9D\xFF\x5E\xAA\xD9\x6D\xB9\xE6\x38\x6C'
            ),
            expected_pso_output=(
                b'\x11\x18\x73\x31\xC2\x79\x96\x2D\x93\xD6\x04\x24\x3F\xD5\x92\xCB'
                b'\x9D\x0A\x92\x6F\x42\x2E\x47\x18\x75\x21\x28\x7E\x71\x56\xC5\xC4'
                b'\xD6\x03\x13\x55\x69\xB9\xE9\xD0\x9C\xF5\xD4\xA2\x70\xF5\x97\x46'
            ),
        )

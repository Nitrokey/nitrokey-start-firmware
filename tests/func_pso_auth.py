"""
func_pso_auth.py - functions for testing PSO commands

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

import pytest
from card_const import KEY_ATTRIBUTES_RSA2K

def _encodeDERLength(length):
    if length < 0x80:
        return length.to_bytes(1, 'big')
    if length < 0x100:
        return b'\x81' + length.to_bytes(1, 'big')
    return b'\x82' + length.to_bytes(2, 'big')

def get_ec_pso(
    card,
    key_index,
    key_attributes,
    key_attribute_caption,
    private_key, expected_public_key,
    pso_input,
):
    """
    If card supports, change key attributes for given key slot, store
    given private key, check that the card could derive the expected
    public key from it, then call the appropriate PSO for this key slot
    and return its result.
    Sets key attributes to RSA2K before returning to caller.
    Skips the test if initial key attribute change is rejected by the card.
    """
    key_attribute_index, control_reference_template, pso_p1, pso_p2 = (
        (0xc1, b'\xb6\x00', 0x9e, 0x9a), # Sign
        (0xc2, b'\xb8\x00', 0x80, 0x86), # Decrypt
        (0xc3, b'\xa4\x00', 0x9e, 0x9a), # Authenticate
    )[key_index]
    try:
        card.cmd_put_data(0x00, key_attribute_index, key_attributes)
    except ValueError:
        pytest.skip('No %s support' % (key_attribute_caption, ))
    try:
        private_key_len = len(private_key)
        r = card.cmd_put_data_odd(
            0x3f,
            0xff,
            b'\x4d' + _encodeDERLength(private_key_len + 10) +
                control_reference_template +
                b'\x7f\x48\x02'
                    b'\x92' + _encodeDERLength(private_key_len) +
                b'\x5f\x48' + _encodeDERLength(private_key_len) +
                    private_key,
        )
        assert r
        r = card.cmd_get_public_key(key_index + 1)
        expected_public_key_len = len(expected_public_key)
        encoded_expected_public_key_len = _encodeDERLength(
            expected_public_key_len,
        )
        expected_public_key_response = (
            b'\x7f\x49' + _encodeDERLength(
                expected_public_key_len +
                len(encoded_expected_public_key_len) + 1,
            ) +
                b'\x86' + encoded_expected_public_key_len +
                    expected_public_key
        )
        assert r == expected_public_key_response
        return card.cmd_pso(pso_p1, pso_p2, pso_input)
    finally:
        card.cmd_put_data(0x00, key_attribute_index, KEY_ATTRIBUTES_RSA2K)

def assert_ec_pso(
    card,
    key_index,
    key_attributes,
    key_attribute_caption,
    private_key, expected_public_key,
    pso_input, expected_pso_output,
):
    """
    Calls get_ec_pso and checks if produced output matches the expected value.
    """
    r = get_ec_pso(
        card=card,
        key_index=key_index,
        key_attributes=key_attributes,
        key_attribute_caption=key_attribute_caption,
        private_key=private_key,
        expected_public_key=expected_public_key,
        pso_input=pso_input,
    )
    assert r == expected_pso_output

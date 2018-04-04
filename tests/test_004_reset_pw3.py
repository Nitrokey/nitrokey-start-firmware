"""
test_004_reset_pw3.py - test resetting pw3

Copyright (C) 2018  g10 Code GmbH
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

FACTORY_PASSPHRASE_PW1=b"123456"
FACTORY_PASSPHRASE_PW3=b"12345678"

# Gnuk specific feature of clear PW3
def test_setup_pw3_0(card):
    r = card.cmd_change_reference_data(3, FACTORY_PASSPHRASE_PW3)
    assert r

def test_verify_pw3_0(card):
    v = card.cmd_verify(3, FACTORY_PASSPHRASE_PW3)
    assert v

def test_verify_pw1_0(card):
    v = card.cmd_verify(1, FACTORY_PASSPHRASE_PW1)
    assert v

def test_verify_pw1_0_2(card):
    v = card.cmd_verify(2, FACTORY_PASSPHRASE_PW1)
    assert v

# from tests.openpgp_card import OpenPGP_Card
import time
from binascii import hexlify

import pytest
import usb
from conftest import ReconnectableDevice

from util import get_data_object

import rsa_keys
from openpgp_card import OpenPGP_Card
from tool.gnuk_token import gnuk_token

CERT_DO_FILEID = 5


def test_identity_set(gnuk_re: ReconnectableDevice):
    """
    Test simple call
    """
    gnuk = next(gnuk_re.get_device())
    try:
        gnuk.cmd_set_identity(1)
    except usb.core.USBError:
        pass


def test_identity_wrong_id(gnuk_re: ReconnectableDevice):
    """
    On invalid identity identifier device should fail with 6581
    """
    gnuk = next(gnuk_re.get_device())
    with pytest.raises(ValueError) as v:
        try:
            gnuk.cmd_set_identity(4)
        except usb.core.USBError:
            pass
    assert '6581' in str(v)


def helper_get_smartcard_sn(gnuk: gnuk_token) -> str:
    """
    Get SC serial number
    """
    data = get_data_object(gnuk, 0x4f)
    serial = hexlify(data[10:14]).decode("UTF-8")
    return serial


PIN_USER = b"1234567890123456"
PIN_ADMIN = PIN_USER
PIN_USER_FACTORY = b"123456"
PIN_ADMIN_FACTORY = b"12345678"


def helper_personalize_card(opc_card: [OpenPGP_Card, None], card: gnuk_token, prefix: bytes, offset_i: int):
    assert offset_i < 3
    if opc_card:
        assert opc_card.change_passwd(3, PIN_ADMIN_FACTORY, PIN_ADMIN)
        assert opc_card.verify(3, PIN_ADMIN)
    card.cmd_verify(3, PIN_ADMIN_FACTORY)
    assert card.cmd_put_data(0x00, 0x5e, prefix + b"gpg_user")
    assert card.cmd_put_data(0x00, 0x5b, prefix + b"GnuPG User")
    assert card.cmd_put_data(0x5f, 0x2d, b"de")  # lang
    assert card.cmd_put_data(0x5f, 0x35, b"1")  # sex
    assert card.cmd_put_data(0x5f, 0x50, prefix + b"https://www.fsij.org/gnuk/")  # url
    assert bytes(get_data_object(card, 0x5e)) == prefix + b"gpg_user"
    for i in range(3):
        offset = (offset_i + i) % 3
        # import key
        t = rsa_keys.build_privkey_template(1+offset, 0+offset)
        assert card.cmd_put_data_odd(0x3f, 0xff, t)
        # put fingerprint
        fpr1 = rsa_keys.fpr[0+offset]
        assert card.cmd_put_data(0x00, 0xc7+offset, fpr1)
        # put timestamp
        timestamp1 = rsa_keys.timestamp[0 + offset]
        assert card.cmd_put_data(0x00, 0xce+offset, timestamp1)
        # test rsa public key
        key = card.cmd_get_public_key(offset + 1)
        key = bytes(key[0])
        assert hexlify(key[9:9 + 256]) in hexlify(rsa_keys.key[offset][0])  # FIXME make it equal


def test_multi_personalize(gnuk_re: ReconnectableDevice):
    gnuk = next(gnuk_re.get_device())
    for i in range(3):
        with pytest.raises(usb.core.USBError):
            gnuk.cmd_set_identity(i)
        time.sleep(1)
        gnuk = next(gnuk_re.get_device())
        helper_personalize_card(None, gnuk, f'{i}'.encode(), i)
#         FIXME test each id


@pytest.mark.parametrize("count", [10, 25, 100, 200])
def test_certificate_upload_limit(gnuk_re: ReconnectableDevice, count):
    gnuk = next(gnuk_re.get_device())

    gnuk.cmd_verify(3, PIN_ADMIN_FACTORY)
    data = b'0123456789'*count
    print('Writing')
    gnuk.cmd_write_binary(CERT_DO_FILEID, data, is_update=True)
    print('Writing finished')
    time.sleep(1)
    print('Check data')
    data_in_device = gnuk.cmd_read_binary(CERT_DO_FILEID)
    read_data = bytes(data_in_device)
    assert len(read_data) >= len(data)
    assert data == read_data[:len(data)]
    print(f'Data written: {len(data)}, data read: {len(read_data)}')


@pytest.mark.parametrize("count", [10, 512])
def test_counter_move(gnuk_re: ReconnectableDevice, count):
    """
    Change identity multiple times and test whether the smart card's serial number has changed
    to indicate current identity.
    First pass is short and checks basic working. Second is long, and tests the internal counter
    implementation which stores the current identity ID.
    """
    print()
    gnuk = next(gnuk_re.get_device())
    for i in range(count):
        mi_id = i % 3
        with pytest.raises(usb.core.USBError):
            gnuk.cmd_set_identity(mi_id)
        time.sleep(1)
        # Rebooted
        gnuk = next(gnuk_re.get_device())
        sn = helper_get_smartcard_sn(gnuk)
        print(f'\r {i} / {count}: {mi_id} / {sn}', end='')
        assert sn[1] == f'{mi_id}'
    print()

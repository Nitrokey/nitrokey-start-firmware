# from tests.openpgp_card import OpenPGP_Card
import time
from binascii import hexlify
from struct import pack

import pytest
import usb
from conftest import ReconnectableDevice, TEST_DATA512

from util import get_data_object

import rsa_keys
from openpgp_card import OpenPGP_Card

from conftest import IDENTITY_CERTSIZE
from tool.gnuk_token import gnuk_token

CERT_DO_FILEID = 5

from conftest import log_msg

def test_identity_set(gnuk_re: ReconnectableDevice):
    """
    Test simple call
    """
    device = gnuk_re
    gnuk = next(device)
    IDENTITY = 1
    with pytest.raises(usb.core.USBError):
        gnuk.cmd_set_identity(IDENTITY)
    time.sleep(1)
    gnuk = next(device)
    assert helper_get_current_identity(gnuk) == IDENTITY


@pytest.mark.skip
def test_identity_wrong_id(gnuk_re: ReconnectableDevice):
    """
    On invalid identity identifier device should fail with 6581
    """
    device = gnuk_re
    gnuk = next(device)
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
        t = rsa_keys.build_privkey_template(1 + offset, 0 + offset)
        assert card.cmd_put_data_odd(0x3f, 0xff, t)
        # put fingerprint
        fpr1 = rsa_keys.fpr[0 + offset]
        assert card.cmd_put_data(0x00, 0xc7 + offset, fpr1)
        # put timestamp
        timestamp1 = rsa_keys.timestamp[0 + offset]
        assert card.cmd_put_data(0x00, 0xce + offset, timestamp1)
        # test rsa public key
        key = card.cmd_get_public_key(offset + 1)
        key = bytes(key[0])
        assert hexlify(key[9:9 + 256]) in hexlify(rsa_keys.key[offset][0])  # FIXME make it equal


# @pytest.mark.skip
def test_multi_personalize(gnuk_re: ReconnectableDevice):
    gnuk = next(gnuk_re)
    for identity in range(3):
        with pytest.raises(usb.core.USBError):
            gnuk.cmd_set_identity(identity)
            time.sleep(1)
        # helper_get_gnuk_with_identity(gnuk, identity)
        gnuk = next(gnuk_re)
        log_msg('personalize card')
        helper_personalize_card(None, gnuk, f'{identity}'.encode(), identity)
        log_msg('personalize card completed')
#         FIXME test each id


@pytest.mark.parametrize("data_src", ['local', 'file'])
def test_certificate_crash(gnuk_re: ReconnectableDevice, data_src):
    gnuk = next(gnuk_re)
    data = b''
    if data_src is 'local':
        data = b'0123456789' * 60
        data = b'\xFE' * 4 + data[:512]
    elif data_src is 'file':
        data = TEST_DATA512
    else:
        raise Exception('No data selected')
    log_msg('Writing')
    gnuk.cmd_select_openpgp()
    gnuk.cmd_verify(3, PIN_ADMIN_FACTORY)
    gnuk.cmd_write_binary(CERT_DO_FILEID, data, is_update=True)
    gnuk.cmd_select_openpgp()
    log_msg('Reading')
    data_in_device = gnuk.cmd_get_data(0x7f, 0x21)
    read_data = bytes(data_in_device)
    assert len(read_data) >= len(data)
    assert data == read_data[:len(data)]


def test_certificate_read_order(gnuk_re: ReconnectableDevice):
    gnuk = next(gnuk_re)
    gnuk.cmd_read_binary(CERT_DO_FILEID)
    gnuk.cmd_select_openpgp()  # this is required to make get_data work
    gnuk.cmd_get_data(0x7f, 0x21)


@pytest.mark.parametrize("count", [
    255,
    2 ** 9,
    2 ** 10,
    2 ** 10 + 1,
    2 ** 11,
    # 2 ** 11 + 1,
])
@pytest.mark.parametrize("identity", range(3))
def test_certificate_upload_limit(gnuk_re: ReconnectableDevice, count, identity):
    gnuk = next(gnuk_re)

    if helper_get_current_identity(gnuk) == identity:
        log_msg('Skipping switch to the same identity')
    else:
        with pytest.raises(usb.core.USBError):
            gnuk.cmd_set_identity(identity)
        # Rebooted
        time.sleep(1)
        gnuk = next(gnuk_re)

    gnuk.cmd_verify(3, PIN_ADMIN_FACTORY)
    src_data = b'0123456789' * (1 + count // 10)
    src_data = src_data[:count - 2]
    data_len = pack('<H', len(src_data))
    src_data = data_len + src_data
    log_msg('\nWriting')
    if count > IDENTITY_CERTSIZE[identity]:
        return
        pytest.skip('Skip test due to being unable to restore the connection')
        with pytest.raises(ValueError) as e:
            gnuk.cmd_write_binary(CERT_DO_FILEID, src_data, is_update=True)
        assert '6581' in str(e)
        gnuk.cmd_select_openpgp()
        log_msg('Fail case test passed')
        return
    gnuk.cmd_write_binary(CERT_DO_FILEID, src_data, is_update=True)
    gnuk.cmd_select_openpgp()
    log_msg('Writing finished')
    log_msg('Check data')
    data_in_device_get_data = gnuk.cmd_get_data(0x7f, 0x21)
    data_in_device = gnuk.cmd_read_binary(CERT_DO_FILEID)
    log_msg(f'{len(data_in_device)} {len(data_in_device_get_data)}')
    read_data = bytes(data_in_device_get_data)
    assert len(read_data) >= len(src_data)
    assert src_data == read_data[:len(src_data)]
    log_msg(f'Data written: {len(src_data)}, data read: {len(read_data)}')
    gnuk.cmd_select_openpgp()



def helper_get_current_identity(gnuk):
    sn = helper_get_smartcard_sn(gnuk)
    return int(sn[1])


@pytest.mark.parametrize("count", [
    10,
    # 512,
])
def test_counter_move(gnuk_re: ReconnectableDevice, count):
    """
    Change identity multiple times and test whether the smart card's serial number has changed
    to indicate current identity.
    First pass is short and checks basic working. Second is long, and tests the internal counter
    implementation which stores the current identity ID.
    """
    print()
    gnuk = next(gnuk_re)
    for i in range(count):
        mi_id = i % 3
        with pytest.raises(usb.core.USBError):
            gnuk.cmd_set_identity(mi_id)
        time.sleep(0.7)
        # Rebooted
        gnuk = next(gnuk_re)
        sn = helper_get_smartcard_sn(gnuk)
        print(f'\r {i} / {count}: {mi_id} / {sn}', end='')
        assert sn[1] == f'{mi_id}'
    print()

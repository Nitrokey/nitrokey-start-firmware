import time
from typing import Generator


FORMAT = '%(relativeCreated)-8d %(message)s'
import logging
logging.basicConfig(format=FORMAT, level=logging.DEBUG)
logger = logging.getLogger()

log_msg = logger.debug

import pytest
from card_reader import get_ccid_device
from openpgp_card import OpenPGP_Card

from tool.gnuk_token import get_gnuk_device, gnuk_token

TEST_DATA512 = open('../tool/random512', 'rb').read()

def pytest_addoption(parser):
    parser.addoption("--reader", dest="reader", type=str, action="store",
                     default="gnuk", help="specify reader: gnuk or gemalto")

@pytest.fixture(scope="session")
def card() -> OpenPGP_Card:
    print()
    print("Test start!")
    reader = get_ccid_device()
    print("Reader:", reader.get_string(1), reader.get_string(2))
    card = OpenPGP_Card(reader)
    card.cmd_select_openpgp()
    yield card
    del card
    reader.ccid_power_off()


@pytest.fixture(scope="session")
def gnuk() -> gnuk_token:
    print("Getting GNUK")
    gnuk = get_gnuk_device()
    gnuk.cmd_select_openpgp()
    yield gnuk
    del gnuk


class ReconnectableDevice():
    def get_device(self) -> gnuk_token:
        while True:
            gnuk = None
            for i in range(10):
                try:
                    gnuk = get_gnuk_device(verbose=False)
                    gnuk.cmd_select_openpgp()
                    break
                except Exception as e:
                    log_msg(f'Connection error: {e}')
                    time.sleep(1)
            assert gnuk is not None
            yield gnuk
            del gnuk



IDENTITY_CERTSIZE = {
    0: 2048,
    1: 2048,
    2: 1024
}

@pytest.fixture(scope="session")
def gnuk_re() -> gnuk_token:
    devices = ReconnectableDevice().get_device()
    return devices

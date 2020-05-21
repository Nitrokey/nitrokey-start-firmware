import pytest
from card_reader import get_ccid_device
from openpgp_card import OpenPGP_Card

from tool.gnuk_token import get_gnuk_device, gnuk_token


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
        gnuk = get_gnuk_device(verbose=False)
        gnuk.cmd_select_openpgp()
        yield gnuk
        del gnuk

@pytest.fixture(scope="session")
def gnuk_re() -> ReconnectableDevice:
    return ReconnectableDevice()

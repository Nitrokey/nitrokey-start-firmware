import pytest

@pytest.fixture(scope="module",autouse=True)
def check_gnuk(card):
    if card.is_gnuk:
        pytest.skip("Gnuk has no support for those features", allow_module_level=True)

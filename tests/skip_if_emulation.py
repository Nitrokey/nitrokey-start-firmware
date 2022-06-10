import pytest

@pytest.fixture(scope="module",autouse=True)
def check_emulation(card):
    if card.is_emulated_gnuk:
        pytest.skip("Emulation requires KDF setup", allow_module_level=True)

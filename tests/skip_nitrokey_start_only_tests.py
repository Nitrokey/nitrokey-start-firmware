import pytest


@pytest.fixture(scope="module", autouse=True)
def check_start(gnuk_re):
    gnuk = next(gnuk_re)
    model = gnuk.get_model()
    if model != b"Nitrokey Start":
        pytest.skip(f"Nitrokey Start only feature, found {model}", allow_module_level=True)

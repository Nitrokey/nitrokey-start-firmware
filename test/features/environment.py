#
# @Before
# def ini(sc):
# def before_step(context, step):

# from card_reader import get_ccid_device
# from openpgp_card import OpenPGP_Card
from card_reader import get_ccid_device
from openpgp_card import OpenPGP_Card

def before_all(context):
    # glc.token = gnuk.get_gnuk_device()
    # glc.token.cmd_select_openpgp()
    reader = get_ccid_device()
    print("Reader:", reader.get_string(1), reader.get_string(2))
    card = OpenPGP_Card(reader)
    card.cmd_select_openpgp()
    context.token = card

# from freshen import *
# from freshen.checks import *
from behave import *


from nose.tools import assert_regexp_matches, assert_equal
from binascii import hexlify

import ast

# import gnuk_token as gnuk
import rsa_keys
from array import array

def text_to_bin(v):
    vr = v
    try:
        import binascii
        vr = vr.replace('\\x', '')
        vr = vr.replace('"', '')
        vr = binascii.unhexlify(vr)
    except:
        vr = v
        print('not binasci')

    try:
        vr = vr.encode()
    except:
        print('not encoded in text_to_bin')
        pass

    return vr

@given("cmd_verify with {who_str} and \"{pass_str}\"")
def cmd_verify(context,who_str,pass_str):
    who = int(who_str)
    pass_str = pass_str.encode()
    context.result = context.token.cmd_verify(who, pass_str)

@given("cmd_change_reference_data with {who_str} and \"{pass_str}\"")
def cmd_change_reference_data(context,who_str,pass_str):
    who = int(who_str)
    pass_str = pass_str.encode()
    context.result = context.token.cmd_change_reference_data(who, pass_str)

@given('cmd_put_data with {tag_str} and {content_str_repr}') # TODO test
def cmd_put_data(context,tag_str,content_str_repr):
    if content_str_repr == '""':
        content_str_repr = ''
    # content_str = ast.literal_eval("b" + content_str_repr + "")
    content_str = text_to_bin(content_str_repr)
    try:
        content_str = content_str.encode()
    except:
        print('not encoded')
        pass
    print(content_str_repr)
    print(content_str)
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    context.result = context.token.cmd_put_data(tagh, tagl, content_str)

@given('cmd_reset_retry_counter with {how_str} and "{data}"')
def cmd_reset_retry_counter(context,how_str, data):
    how = int(how_str)
    data = data.encode()
    context.result = context.token.cmd_reset_retry_counter(how, 0x81, data)

@given("a RSA key pair {keyno_str}")
def set_rsa_key(context,keyno_str):
    context.keyno = int(keyno_str)

@given("importing it to the token as OPENPGP.{openpgp_keyno_str}")
def import_key(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    t = rsa_keys.build_privkey_template(openpgp_keyno, context.keyno)
    context.result = context.token.cmd_put_data_odd(0x3f, 0xff, t)

@given("a fingerprint of OPENPGP.{openpgp_keyno_str} key")
def get_key_fpr(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    context.result = rsa_keys.fpr[openpgp_keyno - 1]

@given("a timestamp of OPENPGP.{openpgp_keyno_str} key")
def get_key_timestamp(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    context.result = rsa_keys.timestamp[openpgp_keyno - 1]

@given("put the data to {tag_str}")
def cmd_put_data_with_result(context,tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    context.result = context.token.cmd_put_data(tagh, tagl, context.result)

@given('a message {content_str_repr}')
def set_msg(context,content_str_repr):
    msg = text_to_bin(content_str_repr)
    context.digestinfo = rsa_keys.compute_digestinfo(msg)

@given("a public key from token for OPENPGP.{openpgp_keyno_str}")
def get_public_key(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    context.pubkey_info = context.token.cmd_get_public_key(openpgp_keyno)

@given("verify signature")
def verify_signature(context):
    context.result = rsa_keys.verify_signature(context.pubkey_info, context.digestinfo, context.sig)

@given("let a token compute digital signature")
def compute_signature(context):
    context.sig = int(hexlify(context.token.cmd_pso(0x9e, 0x9a, context.digestinfo)),16)

@given("let a token authenticate")
def internal_authenticate(context):
    context.sig = int(hexlify(context.token.cmd_internal_authenticate(context.digestinfo)),16)

@given("compute digital signature on host with RSA key pair {keyno_str}")
def compute_signature_on_host(context,keyno_str):
    keyno = int(keyno_str)
    context.result = rsa_keys.compute_signature(keyno, context.digestinfo)

@given('a plain text {content_str_repr}') #  TODO TEST
def set_plaintext(context,content_str_repr):
    # context.plaintext = ast.literal_eval("b" + content_str_repr + "")
    context.plaintext = text_to_bin(content_str_repr)

@given("encrypt it on host with RSA key pair {keyno_str}")
def encrypt_on_host(context,keyno_str):
    keyno = int(keyno_str)
    context.ciphertext = rsa_keys.encrypt(keyno, context.plaintext)

@given("encrypt it on host")
def encrypt_on_host_public_key(context):
    context.ciphertext = rsa_keys.encrypt_with_pubkey(context.pubkey_info, context.plaintext)

@given("let a token decrypt encrypted data")
def decrypt(context):
    print(context.ciphertext)
    context.result = context.token.cmd_pso(0x80, 0x86, context.ciphertext)

@given("USB version string of the token")
def usb_version_string(context):
    context.result = context.token.get_string(3)

@when("requesting {name}: {tag_str}")
def step_impl(context,name, tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    context.result = context.token.cmd_get_data(tagh, tagl)

@when("removing a key OPENPGP.{openpgp_keyno_str}")
def remove_key(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    t = rsa_keys.build_privkey_template_for_remove(openpgp_keyno)
    context.result = context.token.cmd_put_data_odd(0x3f, 0xff, t)

@when("generating a key of OPENPGP.{openpgp_keyno_str}")
def generate_key(context,openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    pubkey_info = context.token.cmd_genkey(openpgp_keyno)
    context.data = rsa_keys.calc_fpr(pubkey_info[0], pubkey_info[1])

@when("put the first data to {tag_str}")
def cmd_put_data_first_with_result(context,tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    context.result = context.token.cmd_put_data(tagh, tagl, context.data[0])

@when("put the second data to {tag_str}")
def cmd_put_data_second_with_result(context,tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    result = context.token.cmd_put_data(tagh, tagl, context.data[1])
    context.result = (context.result and result)

@then('you should get: {v}')
def check_result(context,v):
    # value = ast.literal_eval("b'" + v + "'")
    # v = v.replace('\\x', '')
    v = '"'+v+'"'
    v = text_to_bin(v)
    assert_equal(context.result, v)

@then("it should get success")
def check_success(context):
    assert_equal(context.result, True)

@then("you should get NULL")
def check_null(context):
    assert_equal(context.result, b'')

@then("data should match: {re}")
def check_regexp(context,re):
    re = re.encode()
    assert_regexp_matches(context.result, re)

@then("results should be same")
def check_signature(context):
    assert_equal(context.sig, context.result)

@then("decrypted data should be same as a plain text")
def check_decrypt(context):
    assert_equal(context.plaintext, context.result)

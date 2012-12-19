from freshen import *
from freshen.checks import *
from nose.tools import assert_regexp_matches
from binascii import hexlify

import ast

import gnuk_token as gnuk
import rsa_keys

@Before
def ini(sc):
    if not ftc.token:
        ftc.token = gnuk.get_gnuk_device()
        ftc.token.cmd_select_openpgp()

@Given("cmd_verify with (.*) and \"(.*)\"")
def cmd_verify(who_str,pass_str):
    who = int(who_str)
    scc.result = ftc.token.cmd_verify(who, pass_str)

@Given("cmd_change_reference_data with (.*) and \"(.*)\"")
def cmd_change_reference_data(who_str,pass_str):
    who = int(who_str)
    scc.result = ftc.token.cmd_change_reference_data(who, pass_str)

@Given("cmd_put_data with (.*) and (\".*\")")
def cmd_put_data(tag_str,content_str_repr):
    content_str = ast.literal_eval(content_str_repr)
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    scc.result = ftc.token.cmd_put_data(tagh, tagl, content_str)

@Given("cmd_reset_retry_counter with (.*) and \"(.*)\"")
def cmd_reset_retry_counter(how_str, data):
    how = int(how_str)
    scc.result = ftc.token.cmd_reset_retry_counter(how, data)

@Given("a RSA key pair (.*)")
def set_rsa_key(keyno_str):
    scc.keyno = int(keyno_str)

@Given("importing it to the token as OPENPGP.(.*)")
def import_key(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    t = rsa_keys.build_privkey_template(openpgp_keyno, scc.keyno)
    scc.result = ftc.token.cmd_put_data_odd(0x3f, 0xff, t)

@Given("a fingerprint of OPENPGP.(.*) key")
def get_key_fpr(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    scc.result = rsa_keys.fpr[openpgp_keyno - 1]

@Given("a timestamp of OPENPGP.(.*) key")
def get_key_timestamp(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    scc.result = rsa_keys.timestamp[openpgp_keyno - 1]

@Given("put the data to (.*)")
def cmd_put_data_with_result(tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    scc.result = ftc.token.cmd_put_data(tagh, tagl, scc.result)

@Given("a message (\".*\")")
def set_msg(content_str_repr):
    msg = ast.literal_eval(content_str_repr)
    scc.digestinfo = rsa_keys.compute_digestinfo(msg)

@Given("a public key from token for OPENPGP.(.*)")
def get_public_key(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    scc.pubkey_info = ftc.token.cmd_get_public_key(openpgp_keyno)

@Given("verify signature")
def verify_signature():
    scc.result = rsa_keys.verify_signature(scc.pubkey_info, scc.digestinfo, scc.sig)

@Given("let a token compute digital signature")
def compute_signature():
    scc.sig = int(hexlify(ftc.token.cmd_pso(0x9e, 0x9a, scc.digestinfo)),16)

@Given("let a token authenticate")
def internal_authenticate():
    scc.sig = int(hexlify(ftc.token.cmd_internal_authenticate(scc.digestinfo)),16)

@Given("compute digital signature on host with RSA key pair (.*)")
def compute_signature_on_host(keyno_str):
    keyno = int(keyno_str)
    scc.result = rsa_keys.compute_signature(keyno, scc.digestinfo)

@Given("a plain text (\".*\")")
def set_plaintext(content_str_repr):
    scc.plaintext = ast.literal_eval(content_str_repr)

@Given("encrypt it on host with RSA key pair (.*)$")
def encrypt_on_host(keyno_str):
    keyno = int(keyno_str)
    scc.ciphertext = rsa_keys.encrypt(keyno, scc.plaintext)

@Given("encrypt it on host$")
def encrypt_on_host_public_key():
    scc.ciphertext = rsa_keys.encrypt_with_pubkey(scc.pubkey_info, scc.plaintext)

@Given("let a token decrypt encrypted data")
def decrypt():
    scc.result = ftc.token.cmd_pso_longdata(0x80, 0x86, scc.ciphertext)

@Given("USB version string of the token")
def usb_version_string():
    scc.result = ftc.token.get_string(3)

@When("requesting (.+): ([0-9a-fA-F]+)")
def get_data(name, tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    scc.result = ftc.token.cmd_get_data(tagh, tagl)

@When("removing a key OPENPGP.(.*)")
def remove_key(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    t = rsa_keys.build_privkey_template_for_remove(openpgp_keyno)
    scc.result = ftc.token.cmd_put_data_odd(0x3f, 0xff, t)

@When("generating a key of OPENPGP.(.*)")
def generate_key(openpgp_keyno_str):
    openpgp_keyno = int(openpgp_keyno_str)
    pubkey_info = ftc.token.cmd_genkey(openpgp_keyno)
    scc.data = rsa_keys.calc_fpr(pubkey_info[0], pubkey_info[1])

@When("put the first data to (.*)")
def cmd_put_data_first_with_result(tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    scc.result = ftc.token.cmd_put_data(tagh, tagl, scc.data[0])

@When("put the second data to (.*)")
def cmd_put_data_second_with_result(tag_str):
    tag = int(tag_str, 16)
    tagh = tag >> 8
    tagl = tag & 0xff
    result = ftc.token.cmd_put_data(tagh, tagl, scc.data[1])
    scc.result = (scc.result and result)

@Then("you should get: (.*)")
def check_result(v):
    value = ast.literal_eval("'" + v + "'")
    assert_equal(scc.result, value)

@Then("it should get success")
def check_success():
    assert_equal(scc.result, True)

@Then("you should get NULL")
def check_null():
    assert_equal(scc.result, "")

@Then("data should match: (.*)")
def check_regexp(re):
    assert_regexp_matches(scc.result, re)

@Then("results should be same")
def check_signature():
    assert_equal(scc.sig, scc.result)

@Then("decrypted data should be same as a plain text")
def check_decrypt():
    assert_equal(scc.plaintext, scc.result)

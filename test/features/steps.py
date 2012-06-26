from freshen import *
from freshen.checks import *
from nose.tools import assert_regexp_matches

import ast

import gnuk
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

@keygen
Feature: setup pass phrase
  In order to conform OpenPGP card 2.0 specification
  A token should support pass phrase: PW1, PW3 and reset code
#
#  Scenario: setup PW1 (admin-less mode)
#     Given cmd_change_reference_data with 1 and "123456user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW1 (1)
#     Given cmd_verify with 1 and "user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW1 (2)
#     Given cmd_verify with 2 and "user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW3 (admin-less mode)
#     Given cmd_verify with 3 and "user pass phrase"
#     Then it should get success
#
#  Scenario: setup reset code (in admin-less mode)
#     Given cmd_put_data with d3 and "example reset code 000"
#     Then it should get success
#
#  Scenario: reset pass phrase by reset code (in admin-less mode)
#     Given cmd_reset_retry_counter with 0 and "example reset code 000another user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW1 (1) again
#     Given cmd_verify with 1 and "another user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW1 (2) again
#     Given cmd_verify with 2 and "another user pass phrase"
#     Then it should get success
#
#  Scenario: verify PW3 (admin-less mode) again
#     Given cmd_verify with 3 and "another user pass phrase"
#     Then it should get success

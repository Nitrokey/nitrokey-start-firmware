Feature: setup pass phrase
  In order to conform OpenPGP card 2.0 specification
  A token should support pass phrase: PW1, PW3 and reset code

  Scenario: change PW1
     Given cmd_change_reference_data with 1 and "another user pass phrasePASSPHRASE SHOULD BE LONG"
     Then it should get success

  Scenario: verify PW1 (1) again
     Given cmd_verify with 1 and "PASSPHRASE SHOULD BE LONG"
     Then it should get success

  Scenario: verify PW1 (2) again
     Given cmd_verify with 2 and "PASSPHRASE SHOULD BE LONG"
     Then it should get success

  Scenario: setup reset code again (in admin-full mode)
     Given cmd_put_data with d3 and "example reset code 000"
     Then it should get success

  Scenario: reset pass phrase by reset code (in admin-full mode)
     Given cmd_reset_retry_counter with 0 and "example reset code 000new user pass phrase"
     Then it should get success

  Scenario: verify PW1 (1) again
     Given cmd_verify with 1 and "new user pass phrase"
     Then it should get success

  Scenario: verify PW1 (2) again
     Given cmd_verify with 2 and "new user pass phrase"
     Then it should get success

  Scenario: change PW3 (admin-full mode)
     Given cmd_change_reference_data with 3 and "admin pass phraseanother admin pass phrase"
     Then it should get success

  Scenario: verify PW3 (admin-full mode)
     Given cmd_verify with 3 and "another admin pass phrase"
     Then it should get success

  Scenario: reset pass phrase by admin (in admin-full mode)
     Given cmd_reset_retry_counter with 2 and "new user pass phrase"
     Then it should get success

  Scenario: verify PW1 (1) again
     Given cmd_verify with 1 and "new user pass phrase"
     Then it should get success

  Scenario: verify PW1 (2) again
     Given cmd_verify with 2 and "new user pass phrase"
     Then it should get success

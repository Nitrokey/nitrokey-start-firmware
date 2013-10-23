Feature: setup pass phrase
  In order to conform OpenPGP card 2.0 specification
  A token should support pass phrase: PW1, PW3 and reset code

  Scenario: setup PW3 (admin-full mode)
     Given cmd_change_reference_data with 3 and "12345678admin pass phrase"
     Then it should get success

  Scenario: verify PW3 (admin-full mode)
     Given cmd_verify with 3 and "admin pass phrase"
     Then it should get success

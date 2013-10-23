Feature: setup pass phrase
  In order to conform OpenPGP card 2.0 specification
  A token should support pass phrase: PW1, PW3 and reset code

  Scenario: verify PW3 (admin-less mode)
     Given cmd_verify with 3 and "12345678"
     Then it should get success

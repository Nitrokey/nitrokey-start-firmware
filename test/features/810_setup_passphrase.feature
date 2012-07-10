Feature: check pass phrase
  In order to conform OpenPGP card 2.0 specification
  A token should support pass phrase: PW1, PW3 and reset code

  Scenario: verify PW1 (1)
     Given cmd_verify with 1 and "123456"
     Then it should get success

  Scenario: verify PW1 (2)
     Given cmd_verify with 2 and "123456"
     Then it should get success

  Scenario: verify PW3
     Given cmd_verify with 3 and "12345678"
     Then it should get success

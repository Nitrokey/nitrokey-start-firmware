Feature: confirm empty token
  In order to start tests
  A token should be empty (no pass phrase)

  Scenario: verify PW1 factory setting (1)
     Given cmd_verify with 1 and "123456"
     Then it should get success

  Scenario: verify PW1 factory setting (2)
     Given cmd_verify with 2 and "123456"
     Then it should get success

  Scenario: verify PW3 factory setting
     Given cmd_verify with 3 and "12345678"
     Then it should get success

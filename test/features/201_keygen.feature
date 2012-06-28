@keygen
Feature: key generation
  In order to use a token
  A token should have keys

  Scenario: generate OPENPGP.1 key (sign)
     When generating a key of OPENPGP.1
     And put the first data to c7
     And put the second data to ce
     Then it should get success

  Scenario: generate OPENPGP.2 key (decrypt)
     When generating a key of OPENPGP.2
     And put the first data to c8
     And put the second data to cf
     Then it should get success

  Scenario: generate OPENPGP.3 key (authentication)
     When generating a key of OPENPGP.3
     And put the first data to c9
     And put the second data to d0
     Then it should get success

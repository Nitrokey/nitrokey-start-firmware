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

  Scenario: compute digital signature by OPENPGP.1 key
     Given a message "GnuPG assumes that PW1 keeps valid after keygen."
     And a public key from token for OPENPGP.1
     And let a token compute digital signature
     And verify signature
     Then it should get success

  Scenario: verify PW1 (1) after keygen
     Given cmd_verify with 1 and "123456"
     Then it should get success

  Scenario: verify PW1 (2) after keygen
     Given cmd_verify with 2 and "123456"
     Then it should get success

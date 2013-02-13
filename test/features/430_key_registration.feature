Feature: import keys to token 
  In order to use a token
  A token should have keys

  Scenario: importing OPENPGP.1 key (sign)
     Given a RSA key pair 0
     And importing it to the token as OPENPGP.1
     Then it should get success

  Scenario: importing OPENPGP.2 key (decrypt)
     Given a RSA key pair 1
     And importing it to the token as OPENPGP.2
     Then it should get success

  Scenario: importing OPENPGP.3 key (authentication)
     Given a RSA key pair 2
     And importing it to the token as OPENPGP.3
     Then it should get success

  Scenario: setup data object Finger print sig
     Given a fingerprint of OPENPGP.1 key
     And put the data to c7
     Then it should get success

  Scenario: setup data object Finger print dec
     Given a fingerprint of OPENPGP.2 key
     And put the data to c8
     Then it should get success

  Scenario: setup data object Finger print aut
     Given a fingerprint of OPENPGP.3 key
     And put the data to c9
     Then it should get success

  Scenario: setup data object keygeneration data/time sig
     Given a timestamp of OPENPGP.1 key
     And put the data to ce
     Then it should get success

  Scenario: setup data object keygeneration data/time dec
     Given a timestamp of OPENPGP.2 key
     And put the data to cf
     Then it should get success

  Scenario: setup data object keygeneration data/time aut
     Given a timestamp of OPENPGP.3 key
     And put the data to d0
     Then it should get success

  Scenario: verify PW1 (1) again
     Given cmd_verify with 1 and "another user pass phrase"
     Then it should get success

  Scenario: verify PW1 (2) again
     Given cmd_verify with 2 and "another user pass phrase"
     Then it should get success

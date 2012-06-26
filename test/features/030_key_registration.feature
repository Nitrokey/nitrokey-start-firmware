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

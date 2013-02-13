@keygen
Feature: decryption 
  In order to use a token
  A token should decrypt encrypted data

  Scenario: decrypt by OPENPGP.2 key (1)
     Given a plain text "This is a test message."
     And a public key from token for OPENPGP.2
     And encrypt it on host
     And let a token decrypt encrypted data
     Then decrypted data should be same as a plain text

  Scenario: decrypt by OPENPGP.2 key (2)
     Given a plain text "RSA decryption is as easy as pie."
     And a public key from token for OPENPGP.2
     And encrypt it on host
     And let a token decrypt encrypted data
     Then decrypted data should be same as a plain text


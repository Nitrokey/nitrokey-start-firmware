Feature: decryption 
  In order to use a token
  A token should decrypt encrypted data

  Scenario: decrypt by OPENPGP.2 key (1)
     Given a plain text "This is a test message."
     And encrypt it on host with RSA key pair 1
     And let a token decrypt encrypted data
     Then decrypted data should be same as a plain text

  Scenario: decrypt by OPENPGP.2 key (2)
     Given a plain text "RSA decryption is as easy as pie."
     And encrypt it on host with RSA key pair 1
     And let a token decrypt encrypted data
     Then decrypted data should be same as a plain text


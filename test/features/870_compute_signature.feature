@keygen
Feature: compute digital signature
  In order to use a token
  A token should compute digital signature properly

  Scenario: compute digital signature by OPENPGP.1 key (1)
     Given a message "This is a test message."
     And a public key from token for OPENPGP.1
     And let a token compute digital signature
     And verify signature
     Then it should get success

  Scenario: compute digital signature by OPENPGP.1 key (2)
     Given a message "This is another test message.\nMultiple lines.\n"
     And a public key from token for OPENPGP.1
     And let a token compute digital signature
     And verify signature
     Then it should get success

  Scenario: compute digital signature by OPENPGP.3 key (1)
     Given a message "This is a test message."
     And a public key from token for OPENPGP.3
     And let a token authenticate
     And verify signature
     Then it should get success

  Scenario: compute digital signature by OPENPGP.3 key (2)
     Given a message "This is another test message.\nMultiple lines.\n"
     And a public key from token for OPENPGP.3
     And let a token authenticate
     And verify signature
     Then it should get success

  Scenario: data object ds counter
     When requesting ds counter: 93
     Then data should match: \x00\x00(\x02|\x03)

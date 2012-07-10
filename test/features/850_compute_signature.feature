Feature: compute digital signature
  In order to use a token
  A token should compute digital signature properly

  Scenario: compute digital signature by OPENPGP.1 key (1)
     Given a message "This is a test message."
     And let a token compute digital signature
     And compute digital signature on host with RSA key pair 0
     Then results should be same

  Scenario: compute digital signature by OPENPGP.1 key (2)
     Given a message "This is another test message.\nMultiple lines.\n"
     And let a token compute digital signature
     And compute digital signature on host with RSA key pair 0
     Then results should be same

  Scenario: compute digital signature by OPENPGP.3 key (1)
     Given a message "This is a test message."
     And let a token authenticate
     And compute digital signature on host with RSA key pair 2
     Then results should be same

  Scenario: compute digital signature by OPENPGP.3 key (2)
     Given a message "This is another test message.\nMultiple lines.\n"
     And let a token authenticate
     And compute digital signature on host with RSA key pair 2
     Then results should be same

  Scenario: data object ds counter
     When requesting ds counter: 93
     Then you should get: \x00\x00\x02

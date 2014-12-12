@keyattr
Feature: key attribute change
  In order to use a token with multiple different kind of key algorighm
  A token should support key attribute change

  Scenario: key attribute data object write: algorithm for signature (RSA-4K)
     Given cmd_put_data with c1 and "\x01\x10\x00\x00\x20\x00"
     Then it should get success

  Scenario: key attribute data object write: algorithm for signature (RSA-2K)
     Given cmd_put_data with c1 and "\x01\x08\x00\x00\x20\x00"
     Then it should get success

  Scenario: key attribute data object write: algorithm for decryption (RSA-4K)
     Given cmd_put_data with c2 and "\x01\x10\x00\x00\x20\x00"
     Then it should get success

  Scenario: key attribute data object write: algorithm for decryption (RSA-2K)
     Given cmd_put_data with c2 and "\x01\x08\x00\x00\x20\x00"
     Then it should get success

  Scenario: key attribute data object write: algorithm for authentication (RSA-4K)
     Given cmd_put_data with c3 and "\x01\x10\x00\x00\x20\x00"
     Then it should get success

  Scenario: key attribute data object write: algorithm for authentication (RSA-2K)
     Given cmd_put_data with c3 and "\x01\x08\x00\x00\x20\x00"
     Then it should get success

Feature: command GET DATA
  In order to conform OpenPGP card 2.0 specification
  A token should support all mandatory features of the specification

  Scenario: data object historical bytes
     When requesting historical bytes: 5f52
     Then you should get: \x00\x31\x84\x73\x80\x01\x80\x00\x90\x00

  Scenario: data object extended capabilities
     When requesting extended capabilities: c0
     Then data should match: [\x70\x74]\x00\x00\x20[\x00\x08]\x00\x00\xff\x01\x00

  Scenario: data object algorithm attributes 1
     When requesting algorithm attributes 1: c1
     Then you should get: \x01\x08\x00\x00\x20\x00

  Scenario: data object algorithm attributes 2
     When requesting algorithm attributes 2: c2
     Then you should get: \x01\x08\x00\x00\x20\x00

  Scenario: data object algorithm attributes 3
     When requesting algorighm attributes 3: c3
     Then you should get: \x01\x08\x00\x00\x20\x00

  Scenario: data object AID
     When requesting AID: 4f
     Then data should match: \xd2\x76\x00\x01\x24\x01\x02\x00......\x00\x00

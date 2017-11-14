Feature: confirm empty token
  In order to start tests
  A token should be empty (no data, no keys)

  Scenario: data object Login
     When requesting login data: 5e
     Then you should get NULL

  Scenario: data object Name
     When requesting name: 5b
     Then you should get NULL

  Scenario: data object Language preference
     When requesting anguage preference: 5f2d
     Then you should get NULL

  Scenario: data object Sex
     When requesting sex: 5f35
     Then you should get NULL

  Scenario: data object URL
     When requesting URL: 5f50
     Then you should get NULL

  Scenario: data object ds counter
     When requesting ds counter: 93
     Then you should get: \x00\x00\x00

  Scenario: data object pw1 status bytes
     When requesting pw1 status bytes: c4
     Then you should get: 00202020030003

  Scenario: data object finger print 0
     When requesting finger print: c5
     Then you should get: \x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00

  Scenario: data object finger print 1
     When requesting finger print: c7
     Then you should get NULL

  Scenario: data object finger print 2
     When requesting finger print: c8
     Then you should get NULL

  Scenario: data object finger print 3
     When requesting finger print: c9
     Then you should get NULL

  Scenario: data object CA finger print 0
     When requesting finger print: c6
     Then you should get: \x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00

  Scenario: data object CA finger print 1
     When requesting finger print: ca
     Then you should get NULL

  Scenario: data object CA finger print 2
     When requesting finger print: cb
     Then you should get NULL

  Scenario: data object CA finger print 3
     When requesting finger print: cc
     Then you should get NULL

  Scenario: data object date/time of key pair 0
     When requesting date/time of key pair: cd
     Then you should get: \x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00

  Scenario: data object date/time of key pair 1
     When requesting date/time of key pair: ce
     Then you should get NULL

  Scenario: data object date/time of key pair 2
     When requesting date/time of key pair: cf
     Then you should get NULL

  Scenario: data object date/time of key pair 3
     When requesting date/time of key pair: d0
     Then you should get NULL

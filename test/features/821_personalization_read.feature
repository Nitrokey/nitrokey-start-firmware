Feature: personalize token read
  In order to use a token
  A token should be personalized with name, sex, url, etc.

  Scenario: data object Login
     When requesting login data: 5e
     Then you should get: gpg_user

  Scenario: data object Name
     When requesting name: 5b
     Then you should get: GnuPG User

  Scenario: data object Language preference
     When requesting anguage preference: 5f2d
     Then you should get: ja

  Scenario: data object Sex
     When requesting sex: 5f35
     Then you should get: 1

  Scenario: data object URL
     When requesting URL: 5f50
     Then you should get: http://www.fsij.org/gnuk/

  Scenario: data object pw1 status bytes
     When requesting pw1 status bytes: c4
     Then you should get: \x01\x7f\x7f\x7f\x03\x03\x03

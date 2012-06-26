Feature: personalize token write
  In order to use a token
  A token should be personalized with name, sex, url, etc.

  Scenario: data object Login
     Given cmd_put_data with 5e and "gpg_user"
     Then it should get success

  Scenario: data object Name
     Given cmd_put_data with 5b and "GnuPG User"
     Then it should get success

  Scenario: data object Language preference
     Given cmd_put_data with 5f2d and "ja"
     Then it should get success

  Scenario: data object Sex
     Given cmd_put_data with 5f35 and "1"
     Then it should get success

  Scenario: data object URL
     Given cmd_put_data with 5f50 and "http://www.fsij.org/gnuk/"
     Then it should get success

  Scenario: data object pw1 status bytes
     Given cmd_put_data with c4 and "\x01"
     Then it should get success

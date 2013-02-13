@keygen
Feature: key removal
  In order to use a token
  A token should have keys

  Scenario: remove OPENPGP.1 key (sign)
     When removing a key OPENPGP.1
     Then it should get success

  Scenario: remove OPENPGP.2 key (decrypt)
     When removing a key OPENPGP.2
     Then it should get success

  Scenario: remove OPENPGP.3 key (authentication)
     When removing a key OPENPGP.3
     Then it should get success

  Scenario: remove data object Finger print sig
     Given cmd_put_data with c7 and ""
     Then it should get success

  Scenario: remove data object Finger print dec
     Given cmd_put_data with c8 and ""
     Then it should get success

  Scenario: remove data object Finger print aut
     Given cmd_put_data with c9 and ""
     Then it should get success

  Scenario: remove data object keygeneration data/time sig
     Given cmd_put_data with ce and ""
     Then it should get success

  Scenario: remove data object keygeneration data/time dec
     Given cmd_put_data with cf and ""
     Then it should get success

  Scenario: remove data object keygeneration data/time aut
     Given cmd_put_data with d0 and ""
     Then it should get success

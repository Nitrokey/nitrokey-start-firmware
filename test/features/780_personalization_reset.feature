Feature: removal of data objects
  In order to use a token
  A token should have personalized data

  Scenario: remove data object Login
     Given cmd_put_data with 5e and ""
     Then it should get success

  Scenario: remove data object Name
     Given cmd_put_data with 5b and ""
     Then it should get success

  Scenario: remove data object Language preference
     Given cmd_put_data with 5f2d and ""
     Then it should get success

  Scenario: remove data object Sex
     Given cmd_put_data with 5f35 and ""
     Then it should get success

  Scenario: remove data object URL
     Given cmd_put_data with 5f50 and ""
     Then it should get success

  Scenario: remove data object pw1 status bytes
     Given cmd_put_data with c4 and "\x00"
     Then it should get success

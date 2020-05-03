Feature: There is no minimum difficulty when the network has not enough minting
  Before 0.5, there was a difficulty floor around 0.00024.

  Scenario:
    Given a network with nodes "Alice" and "Bob" able to mint
    And node "Alice" finds 3 blocks at 200 days interval
    Then the difficulty should be below 0.00023

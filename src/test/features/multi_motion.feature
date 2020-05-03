Feature: Shareholders can vote for multiple motions
  Scenario: Two voters and two motions
    Given a network with nodes "Alice" and "Bob" able to mint
    When node "Alice" votes for the motions:
      | 3f786850e387550fdab836ed7e6dc881de23001b |
    When node "Bob" votes for the motions:
      | 89e6c98d92887913cadf06b2adb97f26cde4849b |
      | 3f786850e387550fdab836ed7e6dc881de23001b |
    And node "Alice" finds 5 blocks received by all nodes
    And node "Bob" finds 3 blocks received by all nodes
    Then motion "89e6c98d92887913cadf06b2adb97f26cde4849b" should have 3 blocks
    And motion "3f786850e387550fdab836ed7e6dc881de23001b" should have 8 blocks
    And motion "0000000000000000000000000000000000000000" should not have any vote

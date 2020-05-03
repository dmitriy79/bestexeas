Feature: Split outputs appear as a single transaction in listtransactions
  Scenario:
    Given a network with node "Alice" able to mint
    And a node "Bob" with an empty wallet
    And node "Bob" generates a new address "bob"
    When node "Alice" sends "3" BKS to "bob"
    Then all nodes should have 1 transaction in memory pool
    And node "Bob" should have 1 BKS transaction
    And the 1st transaction should be a receive of "3" to "bob"

    When node "Alice" finds a block received by all other nodes
    And node "Alice" generates a new address "alice"
    And node "Bob" sends "0.5" BKS to "alice"
    Then node "Bob" should have 2 BKS transaction
    And the 2nd transaction should be a send of "0.5" to "alice"

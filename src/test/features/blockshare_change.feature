Feature: BlockShare change is part of the keys exported to Bitcoin
  Scenario: After shares are sent, the total outputs of the exported keys include the change
    Given a network with nodes "Alice" and "Bob" able to mint
    When node "Bob" generates a BlockShare address "bob"
    And node "Alice" sends "1" BlockShares to "bob"
    And node "Alice" reaches a balance of "2,499.00" BlockShares minus the transaction fees
    Then the total unspent amount of all the Bitcoin keys on node "Alice" should be the same as her balance

  Scenario: Sending BlockShares generates a single transaction
    Given a network with nodes "Alice" and "Bob" able to mint
    And node "Bob" generates a BlockShares address "bob"
    And node "Alice" sends "1" BlockShares to "bob"
    Then node "Alice" should have 2 BlockShares transactions
    And the 1st transaction should be the initial distribution of shares
    And the 2nd transaction should be a send of "1" to "bob"

    When node "Alice" finds a block received by all other nodes
    Then node "Bob" should have 2 BlockShare transactions
    And the 1st transaction should be the initial distribution of shares
    And the 2nd transaction should be a receive of "1" to "bob"

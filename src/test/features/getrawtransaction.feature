Feature: getrawtransaction returns data about known transactions

  Scenario: getrawtransaction on a normal block
    Given a network with nodes "Alice" and "Bob" able to mint
    And node "Bob" generates a BKS address "bob"
    When node "Alice" finds a block "A"
    And node "Alice" sends "1" BKS to "bob"
    And node "Alice" finds a block "B"
    And the 1st transaction of block "B" on node "Alice" is named "coinbase"
    And the 2nd transaction of block "B" on node "Alice" is named "coinstake"
    And the 3rd transaction of block "B" on node "Alice" is named "tx"
    And all nodes reach block "B"
    Then getrawtransaction of "tx" should work on all nodes
    And getrawtransaction of "coinbase" should work on all nodes
    And getrawtransaction of "coinstake" should work on all nodes

  Scenario: getrawtransaction on orphaned block transactions
    Given a network with nodes "Alice", "Bob" and "Charlie" able to mint
    And node "Bob" generates a BKS address "bob"
    When node "Alice" finds a block "A"
    And all nodes reach block "A"
    And node "Alice" sends "1" BKS to "bob"
    And node "Alice" finds a block "B" not received by node "Bob"
    And the 1st transaction of block "B" on node "Alice" is named "orphaned coinbase"
    And the 2nd transaction of block "B" on node "Alice" is named "orphaned coinstake"
    And the 3rd transaction of block "B" on node "Alice" is named "orphaned tx"
    And nodes "Alice" and "Charlie" reach block "B"
    And node "Bob" is at block "A"
    And node "Bob" finds a block "B2"
    And node "Bob" finds a block "C2"
    And all nodes reach block "C2"
    Then getrawtransaction of "orphaned tx" should work on nodes "Alice" and "Charlie"
    And getrawtransaction of "orphaned coinbase" should work on nodes "Alice" and "Charlie"
    And getrawtransaction of "orphaned coinstake" should work on nodes "Alice" and "Charlie"

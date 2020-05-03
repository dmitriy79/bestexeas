Feature: Shareholders can vote for the minimum transaction fee of each unit
  Scenario: BKS fee changed by vote
    Given a network with nodes "Alice" and "Bob" able to mint
    And a node "Charlie" with an empty wallet
    And the network has switched to dynamic fees
    When node "Alice" votes for an BKS fee of 0.2
    And node "Alice" finds enough blocks for a fee vote to pass
    And node "Alice" finds enough blocks for the voted fee to be effective
    Then the BKS fee should be 0.2
    When all nodes reach the same height
    And node "Charlie" generates an BKS address "charlie"
    And node "Bob" sends "3" BKS to "charlie" in transaction "tx"
    Then transaction "tx" on node "Charlie" should have a fee of 0.2 per 1000 bytes
    And all nodes should reach 1 transaction in memory pool
    When node "Alice" finds a block
    Then node "Charlie" should have a balance of "3" BKS

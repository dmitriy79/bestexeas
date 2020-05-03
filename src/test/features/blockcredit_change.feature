Feature: BlockCredit change is not reported as a transaction

  Scenario: Sending BlockCredits generates a single transaction
    Given a network with nodes "Alice", "Bob" and "Custodian" able to mint
    When node "Custodian" generates a BlockCredit address "cust"
    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And node "Bob" generates a BlockCredit address "bob"
    And all nodes reach the same height
    And some time pass
    And node "Custodian" sends "1000" BlockCredits to "bob"
    Then node "Custodian" should have 2 BlockCredit transactions
    And the 2nd transaction should be a send of "1000" to "bob"
    And node "Custodian" should have a balance of "999,000" BlockCredits minus the transaction fees
    And node "Bob" should reach an unconfirmed balance of "1000" BlockCredits
    And node "Bob" should have 1 BlockCredit transactions
    And the 1st transaction should be a receive of "1000" to "bob"
    And all nodes should have 1 transaction in memory pool

    When node "Alice" finds a block
    And all nodes reach the same height
    Then node "Custodian" should have a balance of "999,000" BlockCredits minus the transaction fees
    And node "Bob" should reach a balance of "1000" BlockCredits

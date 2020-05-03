Feature: Split money supply
  The money supply is tracked independently for each unit

  Scenario: Initial money supply of the network
    Given a network with node "A" able to mint
    Then the "BlockShare" supply should be between "209,990" and "210,010"
    And the "BlockCredit" supply should be "0"

  Scenario: Money supply after a block is found
    Given a network with nodes "A" and "B" able to mint
    Then "BlockShare" money supply on node "B" should increase by "0.01" when node "A" finds a block
    And "BlockCredit" money supply on node "B" should increase by "0" when node "A" finds a block

  Scenario: Money supply after a BlockCredits custodian is elected
    Given a network with nodes "A" and "B" able to mint
    When node "B" generates a BlockCredit address "cust"
    And node "A" votes an amount of "1,000,000" for custodian "cust"
    And node "A" finds blocks until custodian "cust" is elected
    Then the "BlockShare" supply should be between "209,990" and "210,010"
    And the "BlockCredit" supply should be "1,000,000"

  Scenario: Money supply after a BlockShares custodian is elected
    Given a network with nodes "A" and "B" able to mint
    And the network is at protocol 2.0
    When node "B" generates a BlockShares address "cust"
    And node "A" votes an amount of "10,000" for custodian "cust"
    And node "A" finds blocks until custodian "cust" is elected
    Then the "BlockShare" supply should be between "219,990" and "220,010"
    And the "BlockCredit" supply should be "0"

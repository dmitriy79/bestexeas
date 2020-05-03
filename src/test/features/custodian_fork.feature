Feature:
  The network should handle a fork that happens before a custodian grant and is resolved after it.
  Keeping track of the already elected custodians may make this fail.

  Scenario: The network forks around a custodian grant
    Given a network with nodes "Alice" and "Bob" able to mint
    And a node "Custodian"
    When node "Custodian" generates a BlockCredit address "cust"
    And node "Bob" ignores all new blocks
    And node "Alice" finds 3 blocks
    And node "Alice" votes an amount of "1,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And node "Bob" stops ignoring new blocks
    And node "Bob" votes an amount of "1,000" for custodian "cust"
    And node "Bob" finds blocks until custodian "cust" is elected
    And node "Bob" finds 5 blocks
    And node "Bob" finds a block "X"
    Then node "Alice" should reach block "X"

  Scenario: A custodian is elected but a reorganization removes the grant
    Given a network with nodes "Alice" and "Bob" able to mint
    And a node "Custodian"
    When node "Custodian" generates a BlockCredit address "cust"
    And node "Bob" ignores all new blocks
    And node "Alice" finds 3 blocks
    And node "Alice" votes an amount of "1,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected
    And node "Custodian" reaches the same height as node "Alice"
    And node "Custodian" reaches a balance of "1000" BKC
    And node "Custodian" sends a liquidity of "1000" buy and "2000" sell on unit "C" from address "cust" with identifier "1:"
    Then node "Alice" should reach a total liquidity info of "1000" buy and "2000" sell on unit "C"
    And node "Bob" should reach a total liquidity info of "0" buy and "0" sell on unit "C"

    When node "Bob" stops ignoring new blocks
    And node "Bob" finds blocks until it reaches a higher trust than node "Alice"
    And node "Bob" finds a block "X"
    Then node "Alice" should reach block "X"
    And node "Alice" should reach a total liquidity info of "0" buy and "0" sell on unit "C"
    And node "Bob" should reach a total liquidity info of "0" buy and "0" sell on unit "C"
    And node "Custodian" should reach block "X"
    And node "Custodian" should reach a balance of "0" BKC

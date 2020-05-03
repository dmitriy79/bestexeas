Feature: Distribution to shareholders
  Scenario: Distribution is sent to shareholders
    Given a network with nodes "Alice" able to mint
    And a node "Bob" with an empty wallet
    And a node "Charly" with an empty wallet

    When node "Bob" generates a BlockShare address "bob1"
    And node "Bob" generates a BlockShare address "bob2"
    And node "Charly" generates a BlockShare address "charly"
    And node "Alice" sends "10" BlockShares to "charly"
    And node "Alice" sends "20" BlockShares to "bob1"
    And node "Alice" sends "15.456" BlockShares to "bob1"
    And node "Alice" finds 2 blocks
    And node "Alice" distributes "720,000" Bitcoins

    Then the distribution should send "28.57" Bitcoins to "charly", adjusted by the real BlockShares supply
    And the distribution should send "101.30" Bitcoins to "bob1", adjusted by the real BlockShares supply
    And the distribution should not send anything to "bob2"

  Scenario: Distribution just above and below the minimum payout
    Given a network with nodes "Alice" able to mint
    And a node "Bob" with an empty wallet

    When node "Bob" generates a BlockShare address "above"
    And node "Bob" generates a BlockShare address "below"
    And node "Alice" sends "0.030" BlockShares to "above"
    And node "Alice" sends "0.029" BlockShares to "below"
    And node "Alice" finds 2 blocks
    And node "Alice" distributes "72,000" Bitcoins

    Then the distribution should send "0.01" Bitcoins to "above", adjusted by the real BlockShares supply
    And the distribution should not send anything to "below"


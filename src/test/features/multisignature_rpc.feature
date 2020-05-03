Feature: Ability to sign multisignature transaction without all the keys
  Scenario: Listing unspent outputs of a multisignature address
    Given a network with nodes "Alice", "Bob"

    When node "Bob" generates a new address "a"
    And node "Bob" generates a new address "b"
    And the public key of address "a" is retreived from node "Bob"
    And the public key of address "b" is retreived from node "Bob"

    And node "Bob" adds a multisig address "multi" requiring 2 keys from the public keys "a" and "b"
    And node "Alice" sends "5" to "multi"
    Then node "Bob" should reach an unspent amount of "5" on address "multi"

  Scenario: Two nodes send coins from a multisignature output
    Given a network with node "Alice", "Bob" and "Carol" able to mint
    And a node "Dan" with an empty wallet
    And a node "Erin" with an empty wallet

    When node "Alice" generates a new address "alice"
    And the public key of address "alice" is retreived from node "Alice"

    And node "Bob" generates a new address "bob"
    And the public key of address "bob" is retreived from node "Bob"

    And node "Carol" creates a multisig address "multi" requiring 2 keys from the public keys "alice" and "bob"
    And node "Carol" sends "5" to "multi" in transaction "tx"
    And node "Carol" finds a block received by all other nodes

    And node "Erin" generates a new address "recipient"
    And node "Dan" generates a new address "change"
    And node "Dan" generates a raw transaction "raw" to send the amount sent to address "multi" in transaction "tx" to:
      | Address   | Value |
      | recipient | 1.00  |
      | change    | 2.00  |
    And node "Alice" adds a multisig address "multi" requiring 2 keys from the public keys "alice" and "bob"
    And node "Alice" signs the raw transaction "raw"
    And node "Bob" adds a multisig address "multi" requiring 2 keys from the public keys "alice" and "bob"
    And node "Bob" signs the raw transaction "raw"
    Then the raw transaction "raw" should be complete

    When node "Dan" sends the raw transaction "raw"
    Then all nodes should reach 1 transaction in memory pool

    When node "Alice" finds a block
    Then node "Erin" should reach a balance of "1"

  Scenario: Multisignature custodian
    Given a network with nodes "Alice", "Cust1", "Cust2" and "Recipient"

    And node "Cust1" generates a BlockCredit address "cust1"
    And the public key of address "cust1" is retreived from node "Cust1"

    And node "Cust2" generates a BlockCredit address "cust2"
    And the public key of address "cust2" is retreived from node "Cust2"

    And node "Cust1" adds a BlockCredit multisig address "cust" requiring 2 keys from the public keys "cust1" and "cust2"
    And node "Cust2" adds a BlockCredit multisig address "cust" requiring 2 keys from the public keys "cust1" and "cust2"

    And node "Alice" votes an amount of "1,000,000" for custodian "cust"
    And node "Alice" finds blocks until custodian "cust" is elected in transaction "grant"
    And all nodes reach the same height

    And node "Recipient" generates a BlockCredit address "recipient"
    And node "Cust1" generates a raw BlockCredit transaction "raw" to send the amount sent to address "cust" in transaction "grant" to:
      | Address   | Value      |
      | recipient | 100,000.00 |
      | cust      | 899,990.00 |
    And node "Cust1" signs the raw BlockCredit transaction "raw"
    And node "Cust2" signs the raw BlockCredit transaction "raw"
    Then the raw transaction "raw" should be complete

    When node "Cust2" sends the raw transaction "raw"
    Then all nodes should reach 1 transaction in memory pool

    When node "Alice" finds a block
    Then node "Recipient" should reach a balance of "100,000" BlockCredits


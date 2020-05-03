Feature: A Nubits wallet is imported and the BKS for the private keys become available

  Scenario: Wallet is imported and BKS become available
    Given a network with nodes "Alice" able to mint
    And a node "Bob" with an empty wallet
    And node "Bob" generates a BlockShares address "existing_address"

    When node "Alice" sends "100" BlockShares to "existing_address"
    And node "Alice" sends "50" BlockShares to raw address "tBP2ojyVr14hamNWCrZ6UEviQMu2R1aTRW"
    And node "Alice" sends "50" BlockShares to raw address "tSHHnzgLYfoP5WhJaGqpM1SKxrmvfxvwKB"

    When node "Alice" finds a block received by all nodes
    Then node "Bob" should have a balance of "100" BlockShares

    When node "Bob" imports wallet "data/walletS.dat"
    Then node "Bob" should have a balance of "200" BlockShares

  Scenario: Encrypted wallet is imported and BKS become available
    Given a network with nodes "Alice" able to mint
    And a node "Bob" with an empty wallet

    When node "Alice" sends "50" BlockShares to raw address "tV2koTDQruNoNvQXGRSemAtvV7yEFQDS3m"
    And node "Alice" sends "50" BlockShares to raw address "tQhRvXAwerWXxW51TEqAEf9mWCjRBdMq12"

    When node "Alice" finds a block received by all nodes
    Then node "Bob" should have a balance of "0" BlockShares

    When node "Bob" imports encrypted wallet "data/walletS.enc.dat" with password "password"
    Then node "Bob" should have a balance of "100" BlockShares

  Scenario: Multisig wallet is imported and BKS become available
    Given a network with nodes "Alice" able to mint
    And a node "Bob" with an empty wallet

    When node "Alice" sends "100" BlockShares to raw address "uRSEEuREXSscqosTfjcUk8M845tBizN4py"

    When node "Alice" finds a block received by all nodes
    Then node "Bob" should have a balance of "0" BlockShares

    When node "Bob" imports wallet "data/walletS.multisig.dat"
    Then node "Bob" should have a balance of "100" BlockShares
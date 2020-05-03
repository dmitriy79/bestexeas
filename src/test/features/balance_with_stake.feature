Feature: The balance is correctly reported when shares have been used to mint

  Scenario: A shareholder finds a block and gets the balance
    Given a network with nodes "Alice" and "Bob"
    Then node "Alice" should reach a balance of "2,500" on account ""

    When node "Alice" finds a block received by all other nodes
    And node "Bob" finds enough blocks to mature a Proof of Stake block
    Then node "Alice" should reach a balance of "2,500.01" on account ""



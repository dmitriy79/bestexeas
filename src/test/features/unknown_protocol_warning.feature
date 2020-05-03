Feature: Users should be warned if their client receive votes for an unknown protocol. It probably means there's a protocol upgrade to install.
  Scenario:
    Given a network with nodes "Alice" and "Bob"
    And node "Alice" votes for a protocol version above the current protocol
    When node "Alice" finds 1 block received by all other nodes
    Then node "Bob" should have no error

    When node "Alice" finds 1 block received by all other nodes
    Then node "Bob" should have a warning about the unknown protocol

Feature: Shareholders can vote for assets
  Scenario: A shareholder votes for an asset
    Given a network at protocol 5.0 with nodes "Alice" and "Bob" and "Eve"
    And node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
      |    ZXCVB |            60 | 2 | 3 |  6000000 |        6 |            4 |
    And node "Bob" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     1000 |    0.001 |            8 |
      |   QWERTY |             6 | 2 | 3 |    10000 |      100 |            6 |

    When node "Alice" finds a block "A" received by all nodes
    And block "A" on node "Alice" should contain the following asset votes:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
      |    ZXCVB |            60 | 2 | 3 |  6000000 |        6 |            4 |
    Then the active assets in node "Eve" are empty

    When node "Bob" finds a block "B" received by all nodes
    And block "B" on node "Alice" should contain the following asset votes:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     1000 |    0.001 |            8 |
      |   QWERTY |             6 | 2 | 3 |    10000 |      100 |            6 |
    Then the active assets in node "Eve" are empty

    When node "Alice" finds 1 blocks received by all nodes
    And node "Eve" finds 3 blocks received by all nodes
    And the active assets in node "Eve" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     1000 |    0.001 |            8 |

    When node "Bob" finds 3 blocks received by all nodes
    And node "Eve" finds 3 blocks received by all nodes
    And the active assets in node "Eve" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     1000 |    0.001 |            8 |
      |   QWERTY |             6 | 2 | 3 |    10000 |      100 |            6 |


    When node "Alice" finds 3 blocks received by all nodes
    And node "Eve" finds 3 blocks received by all nodes
    And the active assets in node "Eve" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
      |    ZXCVB |            60 | 2 | 3 |  6000000 |        6 |            4 |
      |   QWERTY |             6 | 2 | 3 |    10000 |      100 |            6 |


  Scenario: Shareholders votes for an asset with different values
    Given a network at protocol 5.0 with nodes "Alice" and "Bob" and "Eve"
    And node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
    And node "Bob" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     1000 |    0.001 |            8 |
    And node "Eve" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 3 | 5 |     2000 |    0.002 |            8 |

    When node "Alice" finds 2 blocks received by all nodes
    And node "Bob" finds 2 blocks received by all nodes
    And node "Eve" finds 1 blocks received by all nodes
    And node "Eve" resets her vote
    And node "Eve" finds 3 blocks received by all nodes
    And the active assets in node "Eve" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 3 | 5 |     2000 |    0.002 |            8 |

  Scenario: A shareholder updates an asset
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    And node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
    And node "Alice" finds 6 blocks received by all nodes
    Then the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |

    When node "Alice" votes for no assets
    And node "Alice" finds 5 blocks received by all nodes
    Then the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |

    When node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     9000 |    0.009 |            8 |
    And node "Alice" finds 3 blocks received by all nodes
    And the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |

    When node "Alice" finds 3 block received by all nodes
    And the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |           120 | 3 | 5 |     9000 |    0.009 |            8 |

  Scenario: Different exponent values
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    And node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |
    And node "Alice" finds 6 blocks received by all nodes
    And the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     3000 |    0.003 |            8 |

    When node "Alice" votes for the following assets:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     9000 |    0.009 |            4 |
    And node "Alice" finds 6 blocks received by all nodes
    And the active assets in node "Bob" are:
      |  AssetId | Confirmations | M | N | MaxTrade | MinTrade | UnitExponent |
      |    ABC09 |            60 | 2 | 3 |     9000 |    0.009 |            8 |

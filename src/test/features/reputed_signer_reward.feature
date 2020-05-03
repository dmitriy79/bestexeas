Feature: Reputed signers receive a extra reward according to their relative reputation score
  From the design document:

  > The following voting will be added:
  >
  > [...]
  >
  > * number of reputed signers eligible for reputed signer block rewards
  >
  > [...]
  >
  > All of the new voting types will apply a protocol rule that an abstention will be interpreted as the value currently enforced by the protocol. [...] The protocol values applied to the current block should be the consensus 60 blocks deep.
  >
  > [...]
  >
  > Each block will have a reputed signer reward given to a single signer. The reward should be given in proportion to the reputations as they were 60 blocks deep. Here is an example using small numbers for clarity: Let us suppose that shareholders have voted to reward 3 reputed share addresses. Let us suppose that reputed share address A has 20 weighted reputation points, share address B has 50 weighted reputation points and share address C has 30 weighted reputation points. Over a period of 2000 blocks, the total rewards for each reputed address can be calculated. If A has received 19.9% of the reward, B has received 50.3% of the rewards, and C has received 29.8% of the rewards, the block reward must be awarded to C, because his reward over the last 2000 is the farthest below what it should be, according to his reputation score.
  >
  > [...]
  >
  > Voting would be weighted most heavily toward recent votes. The last 5000 blocks of votes would receive full weight, the next most recent 10000 blocks would receive half weight, and the 20000 before that would receive quarter weight.

  > [the signer reward amount] should be an additional item that can be voted on. The mint reward should never be subject to a vote, but the reputed signer block reward should be of variable size according to its own vote.

  Scenario: Reputed signers receive a reward
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    And a node "Carol"
    And a node "Dan"
    And node "Carol" generates a new address "carol"
    And node "Dan" generates a new address "dan"

    When node "Alice" votes to reward 2 signers with "0.12" BKS
    And node "Alice" finds 2 blocks received by all other nodes
    And node "Alice" resets her vote
    And node "Bob" votes to reward 5 signers with "1.50" BKS
    And node "Bob" finds 2 blocks received by all other nodes
    And node "Bob" resets his vote
    And node "Alice" finds enough block for the signer reward vote to become effective
    Then the effective signer reward on node "Alice" should be
      """
      {
        "count": 2,
        "amount": 0.12
      }
      """
    And the signer reward vote details on node "Alice" 4 blocks ago should be
      """
      {
        "count": [
          {"vote": null, "value": 0, "count": 3, "accumulated_percentage": 50.0},
          {"vote":    2, "value": 2, "count": 2, "accumulated_percentage": 83.3},
          {"vote":    5, "value": 5, "count": 1, "accumulated_percentage": 100.0}
        ],
        "amount": [
          {"vote": null, "value":    0, "count": 3, "accumulated_percentage": 50.0},
          {"vote": 0.12, "value": 0.12, "count": 2, "accumulated_percentage": 83.3},
          {"vote":  1.5, "value":  1.5, "count": 1, "accumulated_percentage": 100.0}
        ]
      }
      """
    And the signer reward vote details on node "Alice" 3 blocks ago should be
      """
      {
        "count": [
          {"vote": null, "value": 2, "count": 2, "accumulated_percentage": 33.3},
          {"vote":    2, "value": 2, "count": 2, "accumulated_percentage": 66.7},
          {"vote":    5, "value": 5, "count": 2, "accumulated_percentage": 100.0}
        ],
        "amount": [
          {"vote": null, "value": 0.12, "count": 2, "accumulated_percentage": 33.3},
          {"vote": 0.12, "value": 0.12, "count": 2, "accumulated_percentage": 66.7},
          {"vote":  1.5, "value":  1.5, "count": 2, "accumulated_percentage": 100.0}
        ]
      }
      """
    And node "Alice" upvotes "carol" for 3 blocks
    And node "Bob" upvotes "dan" for 2 blocks
    And node "Alice" finds enough block for the voted reputation to become effective
    Then the reputation of "carol" on node "Alice" should be "9"
    And the reputation of "dan" on node "Alice" should be "6"

    When all nodes reach the same height
    Then node "Carol" should have an unconfirmed balance of "0.24" BKS

  Scenario: Reputed signers vote before protocol v4
    Given a network at protocol 2.0 with nodes "Alice" and "Bob"

    When node "Alice" votes to reward 2 signers with "0.12" BKS
    And node "Alice" finds a block "X"
    Then all nodes should be at block "X"

Feature: Shareholders can vote for or against signer addresses

    When a block is minted, the minter may enter up to three upvotes or downvotes, each associated with share address, presumably that of a reputed signer. What the user may place in his client from which these three upvotes or downvotes will be derived is a bit more complex.

    The user may place as many pairs of addresses and numbers as they like. This way it is possible for the user to express what the relative reputation of any number of addresses should be. Consider this example user entry as the basis for determining a reputation vote:

    tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW 5
    tHgyFmmBPmcwHxHh3bpng8uiRVgQ9zqWuK 10
    uUCJuuivVpATbmWcBEKeKhqs9vvJ44UYSL 1
    t82WQUtxiXJZjzSSC1nF5tDaNrxGHwsV3H -5

    Only three pairs can ever be selected for inclusion in a block, and the quantity of upvote or downvote cannot be specified: it is always understood as one upvote or one downvote. The absolute value of the number beside the share addresses indicate how likely (relatively) each is for inclusion in the block. Whether it is negative or positive corresponds to being an upvote or downvote. So, the first address above is just as likely to be selected for inclusion as the last address, but the first address will always be upvoted and the last address always downvoted. The second address is ten times as likely to be chosen for inclusion as the third address.

    Voting would be weighted most heavily toward recent votes. The last 5000 blocks of votes would receive full weight, the next most recent 10000 blocks would receive half weight, and the 20000 before that would receive quarter weight.

  Scenario: An user votes for signers
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    When node "Alice" votes for the following signers:
      | Address                            | Number |
      | tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW |      5 |
      | tHgyFmmBPmcwHxHh3bpng8uiRVgQ9zqWuK |     10 |
      | uUCJuuivVpATbmWcBEKeKhqs9vvJ44UYSL |      1 |
      | t82WQUtxiXJZjzSSC1nF5tDaNrxGHwsV3H |     -5 |
    And node "Alice" finds a block "A"
    Then block "A" on node "Alice" should contain 3 reputed signer votes among these:
      | Address                            | Number |
      | tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW |      1 |
      | tHgyFmmBPmcwHxHh3bpng8uiRVgQ9zqWuK |      1 |
      | uUCJuuivVpATbmWcBEKeKhqs9vvJ44UYSL |      1 |
      | t82WQUtxiXJZjzSSC1nF5tDaNrxGHwsV3H |     -1 |

  Scenario: An user votes for a single address with a weight of 0
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    When node "Alice" votes for the following signers:
      | Address                            | Number |
      | tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW |      0 |
    And node "Alice" finds a block "A"
    Then block "A" on node "Alice" should contain 0 reputed signer votes

  Scenario: Reputation RPC
    Given a network at protocol 5.0 with nodes "Alice" and "Bob"
    When node "Alice" upvotes "tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW" for 3 blocks
    And node "Bob" downvotes "tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW" for 2 blocks
    And node "Alice" upvotes "tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW" for 3 blocks
    And node "Alice" finds enough block for the voted reputation to become effective
    Then the reputation of "tBfFT8qt88FGRvCDKiaqFB7Zw8cAtKnyPW" on node "Alice" should be "12"

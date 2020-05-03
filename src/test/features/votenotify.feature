Feature: A command is called each time the vote changes. This can be used to update data feeds.
  Background:
    Given sample vote "alice vote" is:
      """
      {
         "custodians":[
            {"address":"cMJxKcoyfwD3jY8MhhstQmvmhf9oDka4ML", "amount":100.00000000},
            {"address":"cL4NEzwHDyWYNVBAMNmE6PPKabjJN1jVqb", "amount":5.50000000}
         ],
         "parkrates":[
            {
               "unit":"C",
               "rates":[
                  {"blocks":8192, "rate":0.00030000},
                  {"blocks":16384, "rate":0.00060000},
                  {"blocks":32768, "rate":0.00130000}
               ]
            }
         ],
         "motions":[
            "8151325dcdbae9e0ff95f9f9658432dbedfdb209",
            "3f786850e387550fdab836ed7e6dc881de23001b"
         ],
         "fees": {
            "C": 0.05
         },
         "reputations":[
           {"address": "tV5zFXK46bFTWM3ra8UcCWzCRsc12mg3as", "weight": 10},
           {"address": "tHpbt9ZTgWfGxYDoukHj1P3AMsuvKm5XtG", "weight": -2}
         ],
         "signerreward":{
           "count": 5,
           "amount": 12.0512
         },
         "assets":[
           {"assetid": "BTC", "confirmations": 6, "reqsigners": 2, "totalsigners": 3, "maxtrade": 20.0, "mintrade": 0.001, "unitexponent": 8},
           {"assetid": "LTC", "confirmations": 60, "reqsigners": 2, "totalsigners": 3, "maxtrade": 50000.0, "mintrade": 0.01, "unitexponent": 4}
         ]
      }
      """

  Scenario: Votenotify is used as data feed source
    Given a node "Alice" started with a votenotify script
    And node "Alice" generates a BKS address "alice"
    And the votenotify script of node "Alice" is written to dump the vote and sign it with address "alice"
    And a data feed "Alice"
    And a node "Bob"
    When node "Alice" sets her vote to sample vote "alice vote"
    And the data feed "Alice" returns the content of the dumped vote and signature on node "Alice"
    And node "Bob" sets his data feed to the URL of "Alice" with address "alice"
    Then the vote of node "Bob" should be sample vote "alice vote"

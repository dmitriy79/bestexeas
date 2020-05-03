Feature: The user can define a data feed URL to automatically update his vote from an external source
  Background:
    Given sample vote "full" is:
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
           "8": 0.5,
           "C": 0.02
         },
         "reputations":[
           {"address": "tV5zFXK46bFTWM3ra8UcCWzCRsc12mg3as", "weight": 10},
           {"address": "tHpbt9ZTgWfGxYDoukHj1P3AMsuvKm5XtG", "weight": -2}
         ],
         "signerreward":{
           "count": 10,
           "amount": 20.0
         },
         "assets":[
           {"assetid": "BTC", "confirmations": 6, "reqsigners": 2, "totalsigners": 3, "maxtrade": 20.0, "mintrade": 0.001, "unitexponent": 8},
           {"assetid": "LTC", "confirmations": 60, "reqsigners": 2, "totalsigners": 3, "maxtrade": 50000.0, "mintrade": 0.01, "unitexponent": 4}
         ]
      }
      """
    And sample vote "another full" is:
      """
      {
         "custodians":[
            {"address":"cMJxKcoyfwD3jY8MhhstQmvmhf9oDka4ML", "amount":100.00000000}
         ],
         "parkrates":[
            {
               "unit":"C",
               "rates":[
                  {"blocks":8192, "rate":0.00040000},
                  {"blocks":16384, "rate":0.00080000}
               ]
            }
         ],
         "motions":[
            "3f786850e387550fdab836ed7e6dc881de23001b",
            "1111111111111111111111111111111111111111"
         ],
         "fees": {
           "8": 2.0,
           "C": 0.01
         },
         "reputations":[
           {"address": "tUgyRr1JHgw1AqMc1Zq9VzFEbukpYBKAX2", "weight": 1}
         ],
         "signerreward":{
           "count": 5,
           "amount": 12.0512
         },
         "assets":[
           {"assetid": "BTC", "confirmations": 10, "reqsigners": 3, "totalsigners": 5, "maxtrade": 20.0, "mintrade": 0.001, "unitexponent": 8 }
         ]
      }
      """
    And sample vote "blank" is:
      """
      {
         "custodians":[],
         "parkrates":[],
         "motions":[],
         "fees": {},
         "reputations":[],
         "signerreward":{"count": null, "amount": null},
         "assets":[]
      }
      """

  Scenario: An user sets a data feed
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be sample vote "full"

  Scenario: The data feed updates
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "blank"
    When node "Alice" sets her data feed to the URL of "Bob"
    And the vote of node "Alice" is sample vote "blank"
    And the data feed "Bob" returns sample vote "full"
    And 60 seconds pass
    Then the vote of node "Alice" should become sample vote "full"

  Scenario: A data feed is set and the client is restarted
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    When node "Alice" sets her data feed to the URL of "Bob"
    And node "Alice" restarts
    Then node "Alice" should use the data feed "Bob"

  Scenario: A data feed is set with signature and the client is restarted
    Given a network with node "Alice" able to mint
    And a node "Bob"
    And node "Bob" generates a BKS address "bob"
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    When node "Alice" sets her data feed to the URL of "Bob" with address "bob"
    And node "Alice" restarts
    Then node "Alice" should use the data feed "Bob" with address "bob"

  Scenario: A data feed sends too many data
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full" padded with spaces to 10241 bytes
    When node "Alice" sets her data feed to the URL of "Bob"
    And the vote of node "Alice" is sample vote "blank"
    And the error on node "Alice" should be "Data feed size exceeds limit (10240 bytes)"

  Scenario: A data feed sends incomplete vote
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    And node "Alice" sets her vote to sample vote "full"
    And the data feed "Bob" returns:
      """
      {
         "motions":[
            "1111111111111111111111111111111111111111"
         ]
      }
      """
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be:
      """
      {
         "custodians":[],
         "parkrates":[],
         "motions":[
            "1111111111111111111111111111111111111111"
         ],
         "fees": {},
         "reputations":[],
         "signerreward":{"count": null, "amount": null},
         "assets":[]
      }
      """

  Scenario: A data feed is down
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    And data feed "Bob" shuts down
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be sample vote "blank"
    And the error on node "Alice" should be "Data feed failed: Couldn't connect to server"

  Scenario: A data feed returns invalid JSON
    Given a network with node "Alice" able to mint
    And node "Alice" sets her vote to sample vote "full"
    And a data feed "Bob"
    And the data feed "Bob" returns:
      """
      <html><body>Hello</body></html>
      """
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be sample vote "full"
    And the error on node "Alice" should be "Data feed returned invalid JSON data"

  Scenario: A data feed returns non 200 code
    Given a network with node "Alice" able to mint
    And node "Alice" sets her vote to sample vote "full"
    And a data feed "Bob"
    And the data feed "Bob" returns a status 500 with sample vote "another full"
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be sample vote "full"
    And the error on node "Alice" should be "Data feed failed: Response code 500"

  Scenario: An user sets a data feed with an address and gets a signed vote
    Given a network with node "Alice" able to mint
    And a node "Bob"
    And node "Bob" generates a BKS address "bob"
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    And the data feed "Bob" is signed by node "Bob" with address "bob"
    When node "Alice" sets her data feed to the URL of "Bob" with address "bob"
    Then the vote of node "Alice" should be sample vote "full"

  Scenario: An user sets a data feed with an address and gets an unsigned vote
    Given a network with node "Alice" able to mint
    And a node "Bob"
    And node "Bob" generates a BKS address "bob"
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    When node "Alice" sets her data feed to the URL of "Bob" with address "bob"
    Then the vote of node "Alice" should be sample vote "blank"

  Scenario: An user sets a data feed with an address and gets an incorrectly signed vote
    Given a network with node "Alice" able to mint
    And a node "Bob"
    And node "Bob" generates a BKS address "bob"
    And node "Bob" generates a BKS address "wrong"
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    And the data feed "Bob" is signed by node "Bob" with address "wrong"
    When node "Alice" sets her data feed to the URL of "Bob" with address "bob"
    Then the vote of node "Alice" should be sample vote "blank"

  Scenario Outline: Set data feed to only specific parts
    Given a network with node "Alice" able to mint
    And a data feed "Bob"
    When the data feed "Bob" returns sample vote "full"
    And node "Alice" sets her vote to sample vote "another full"
    And node "Alice" sets her data feed to the URL of "Bob" only on <parts>
    Then the vote of node "Alice" should be sample vote "another full" with <parts> replaced from sample "full"

    Examples:
      | parts                  |
      | custodians             |
      | custodians and motions |
      | parkrates              |
      | motions                |
      | fees                   |
      | reputations            |
      | signerreward           |
      | assets                 |

  Scenario: An user sets a data feed through a proxy
    Given a proxy
    And a node "Alice" connected through the proxy
    And a data feed "Bob"
    And the data feed "Bob" returns sample vote "full"
    When node "Alice" sets her data feed to the URL of "Bob"
    Then the vote of node "Alice" should be sample vote "full"

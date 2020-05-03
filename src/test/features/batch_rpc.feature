Feature: Ability to send multiple RPC requests in one HTTP request
  Scenario:
    Given a network with node "Alice" able to mint
    When this RPC request is sent to node "Alice":
      """
      [{"params": [], "method": "getinfo", "id": 0}, {"params": [], "method": "getinfo", "id": 0}]
      """
    Then the result should contain 2 getinfo answers

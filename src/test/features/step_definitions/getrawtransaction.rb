
When(/^the (\d+)\S+ transaction of block "(.*?)" on node "(.*?)" is named "(.*?)"$/) do |arg1, arg2, arg3, arg4|
  n = arg1.to_i - 1
  block = @blocks[arg2]
  node = @nodes[arg3]
  name = arg4

  info = node.rpc("getblock", block)
  transactions = info["tx"]
  @tx[name] = transactions[n]
end

Then(/^getrawtransaction of "(.*?)" should work on all nodes$/) do |arg1|
  tx = @tx[arg1]
  result = @nodes.map do |name, node|
    begin
      node.rpc("getrawtransaction", tx, 1)
      [name, true]
    rescue => e
      [name, e]
    end
  end
  expect(result).to eq(@nodes.map { |name, node| [name, true] })
end

Then(/^getrawtransaction of "(.*?)" should work on nodes (.*?)$/) do |arg1, arg2|
  tx = @tx[arg1]
  nodes = arg2.scan(/"(.*?)"/).map { |name,| [name, @nodes[name]] }
  result = nodes.map do |name, node|
    begin
      node.rpc("getrawtransaction", tx, 1)
      [name, true]
    rescue => e
      [name, e]
    end
  end
  expect(result).to eq(nodes.map { |name, node| [name, true] })
end


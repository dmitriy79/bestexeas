Then(/^the "(.*?)" supply should be between "(.*?)" and "(.*?)"$/) do |arg1, arg2, arg3|
  supply = money_supply(arg1)
  expect(supply).to be >= parse_number(arg2)
  expect(supply).to be <= parse_number(arg3)
end

Then(/^the "(.*?)" supply should be "(.*?)"$/) do |arg1, arg2|
  supply = money_supply(arg1)
  expect(supply).to be == parse_number(arg2)
end

Then(/^the "(.*?)" supply for "(.*?)" should be "(.*?)"$/) do |arg1, node, arg2|
  supply = money_supply(arg1, @nodes[node])
  expect(supply).to be == parse_number(arg2)
end

When(/^the current BlockShare supply on "(.*?)" is recorded$/) do |arg1|
  @nushare_supply = @nodes[arg1].unit_rpc('8', 'getinfo')["moneysupply"]
end

Then(/^the "(.*?)" supply for "(.*?)" should have increased by "(.*?)"$/) do |arg1, arg2, arg3|
  expect(@nodes[arg2].unit_rpc(unit(arg1), 'getinfo')["moneysupply"]).to be_with(0.0001).of(@nushare_supply + parse_number(arg3))
end


Then(/^"(.*?)" money supply on node "(.*?)" should increase by "(.*?)" when node "(.*?)" finds a block$/) do |arg1, arg2, arg3, arg4|
  unit_name = arg1
  checked_node = @nodes[arg2]
  increase = arg3.to_f
  minter = @nodes[arg4]
  initial_supply = money_supply(unit_name, checked_node)
  minter.generate_stake
  wait_for do
    checked_node.block_count == minter.block_count
  end
  wait_for do
    expect(money_supply(unit_name, checked_node)).to be_within(0.0001).of(initial_supply + increase)
  end
end

Then(/^the amount minted on the last block on "(.*?)" should be "(.*?)"$/) do |arg1, arg2|
  node = @nodes[arg1]
  expected = parse_number(arg2)
  height = node.rpc("getblockcount")
  block = node.rpc("getblockhash", height)
  expect(node.rpc("getblock", block)["mint"]).to eq(expected)
end

When(/^node "(.*?)" finds enough blocks for her park rate vote to become the median park rate$/) do |arg1|
  step "node \"#{arg1}\" finds 3 blocks received by all nodes"
end

When(/^node "(.*?)" finds enough blocks for the voted park rate to become effective$/) do |arg1|
  node = @nodes[arg1]
  protocol = node.info["protocolversion"]
  if protocol >= PROTOCOL_V2_0
    step "node \"#{arg1}\" finds 12 blocks received by all nodes"
  end
end

Then(/^the expected premium on node "(.*?)" for "(.*?)" BlockCredits parked for (\d+) blocks should be "(.*?)"$/) do |arg1, arg2, arg3, arg4|
  node = @nodes[arg1]
  amount = parse_number(arg2)
  blocks = arg3.to_i
  expected_premium = parse_number(arg4)

  actual_premium = parse_number(node.unit_rpc('B', 'getpremium', amount, blocks))
  expect(actual_premium).to eq(expected_premium)
end

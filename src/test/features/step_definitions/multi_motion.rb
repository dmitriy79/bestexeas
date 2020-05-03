
When(/^node "(.*?)" votes for the motions:$/) do |arg1, table|
  node = @nodes[arg1]
  vote = node.rpc("getvote")
  vote["motions"] = table.raw.map(&:first)
  node.rpc("setvote", vote)
end

Then(/^motion "(.*?)" should have (\d+) blocks?$/) do |arg1, arg2|
  node = @nodes.values.first
  motions = node.rpc("getmotions")
  expect(motions[arg1]["blocks"]).to eq(arg2.to_i)
end

Then(/^motion "(.*?)" should not have any vote$/) do |arg1|
  node = @nodes.values.first
  motions = node.rpc("getmotions")
  expect(motions[arg1]).to be_nil
end

Then(/^the vote of block "(.*?)" on node "(.*?)" should return$/) do |arg1, arg2, string|
  node = @nodes[arg2]
  block = @blocks[arg1]
  expected = JSON.parse(string).to_json
  actual = node.rpc("getblock", block)["vote"].to_json
  expect(actual).to eq(expected)
end

When(/^node "(.*?)" votes for the motion hash "(.*?)"$/) do |arg1, arg2|
  node = @nodes[arg1]
  vote = node.rpc("getvote")
  vote["motionhash"] = arg2
  node.rpc("setvote", vote)
end


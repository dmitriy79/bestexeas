When(/^node "(.*?)" votes to reward (\d+) signers with "(.*?)" BKS$/) do |arg1, arg2, arg3|
  node = @nodes[arg1]

  vote = node.rpc("getvote")
  vote["signerreward"] = {
    "count" => arg2.to_i,
    "amount" => parse_number(arg3),
  }

  expect(node.rpc("setvote", vote)).to eq(vote)
  expect(node.rpc("getvote")["signerreward"]).to eq(vote["signerreward"])
end

When(/^node "(.*?)" finds enough block for the signer reward vote to become effective$/) do |arg1|
  step %Q{node "#{arg1}" finds 3 blocks received by all other nodes}
end

Then(/^the effective signer reward on node "(.*?)" should be$/) do |arg1, string|
  expect(@nodes[arg1].rpc("getsignerreward")).to eq(JSON.parse(string))
end

Then(/^the signer reward vote details on node "(.*?)" (\d) blocks ago should be$/) do |arg1, arg2, string|
  expect(@nodes[arg1].rpc("getsignerrewardvotes", -arg2.to_i)).to eq(JSON.parse(string))
end

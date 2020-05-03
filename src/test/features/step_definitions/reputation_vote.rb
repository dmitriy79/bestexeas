When(/^node "(.*?)" votes for the following signers:$/) do |arg1, table|
  node = @nodes[arg1]

  vote = node.rpc("getvote")
  vote["reputations"] = table.hashes.map do |row|
    {
      address: @addresses.fetch(row["Address"], row["Address"]),
      weight: row["Number"].to_i,
    }
  end

  node.rpc("setvote", vote)
  expect(node.rpc("getvote")["reputation"]).to eq(vote["reputation"])
end

Then(/^block "(.*?)" on node "(.*?)" should contain (\d+) reputed signer votes among these:$/) do |arg1, arg2, arg3, table|
  block = @blocks[arg1]
  node = @nodes[arg2]

  info = node.rpc("getblock", block)
  vote = info["vote"]
  reputation_votes = vote["reputations"]
  expect(reputation_votes.size).to eq(arg3.to_i)

  expected_reputation_votes = table.hashes.map do |row|
    {
      "address" => @addresses.fetch(row["Address"], row["Address"]),
      "weight" => row["Number"].to_i,
    }
  end

  reputation_votes.each do |reputation_vote|
    expect(expected_reputation_votes).to include(reputation_vote)
  end
end

Then(/^block "(.*?)" on node "(.*?)" should contain (\d+) reputed signer votes$/) do |arg1, arg2, arg3|
  block = @blocks[arg1]
  node = @nodes[arg2]

  info = node.rpc("getblock", block)
  vote = info["vote"]
  reputation_votes = vote["reputations"]
  begin
    expect(reputation_votes.size).to eq(arg3.to_i)
  rescue Exception
    p reputation_votes
    raise
  end
end

When(/^node "(.*?)" (up|down)votes "(.*?)" for (\d+) blocks$/) do |arg1, direction, arg2, arg3|
  node = @nodes[arg1]
  vote = node.rpc("getvote")
  vote["reputations"] = [
    {
      address: @addresses.fetch(arg2, arg2),
      weight: (direction == "up" ? 1 : -1),
    },
  ]
  node.rpc("setvote", vote)

  step %Q{node "#{arg1}" finds #{arg3} blocks}
  step %Q{all nodes reach the same height}
end

When(/^node "(.*?)" finds enough block for the voted reputation to become effective$/) do |arg1|
  step %Q{node "#{arg1}" finds 3 blocks}
end

Then(/^the reputation of "(.*?)" on node "(.*?)" should be "(.*?)"$/) do |arg1, arg2, arg3|
  node = @nodes[arg2]
  result = node.rpc("getreputations")
  address = @addresses.fetch(arg1, arg1)
  expect(result[address].to_d).to eq(arg3.to_d)
end

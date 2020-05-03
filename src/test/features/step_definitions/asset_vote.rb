When(/^node "(.*?)" votes for the following assets:$/) do |arg1, table|
  node = @nodes[arg1]

  vote = {
    "assets" => table.hashes.map do |row|
      {
        "assetid" => row["AssetId"],
        "confirmations" => row["Confirmations"].to_i,
        "reqsigners" => row["M"].to_i,
        "totalsigners" => row["N"].to_i,
        "maxtrade" => row["MaxTrade"].to_f,
        "mintrade" => row["MinTrade"].to_f,
        "unitexponent" => row["UnitExponent"].to_i,
      }
    end
  }

  node.rpc("setvote", vote)

  asset_votes = node.rpc("getvote")["assets"]

  expect(asset_votes).to eq(vote["assets"])
end

Then(/^block "(.*?)" on node "(.*?)" should contain the following asset votes:$/) do |arg1, arg2, table|
  block = @blocks[arg1]
  node = @nodes[arg2]

  info = node.rpc("getblock", block)
  vote = info["vote"]
  asset_votes = vote["assets"]
  expect(asset_votes.size).to eq(table.hashes.length)

  expected_asset_votes = table.hashes.map do |row|
    {
      "assetid" => row["AssetId"],
      "confirmations" => row["Confirmations"].to_i,
      "reqsigners" => row["M"].to_i,
      "totalsigners" => row["N"].to_i,
      "maxtrade" => row["MaxTrade"].to_f,
      "mintrade" => row["MinTrade"].to_f,
      "unitexponent" => row["UnitExponent"].to_i,
    }
  end

  expected_asset_votes.each do |expected_vote|
    expect(asset_votes).to include(expected_vote)
  end
end

Then(/^block "(.*?)" on node "(.*?)" should contain no asset votes$/) do |arg1, arg2|
  block = @blocks[arg1]
  node = @nodes[arg2]

  asset_votes = node.rpc("getblock", block)["vote"]["assets"]
  expect(asset_votes).to be_empty
end

Then(/^the active assets in node "(.*?)" are empty$/) do |arg1|
  node = @nodes[arg1]
  expect(node.rpc("getassets")).to be_empty
end

Then(/^the active assets in node "(.*?)" are:$/) do |arg1, table|
  node = @nodes[arg1]
  assets = node.rpc("getassets")
  expect(assets.size).to eq(table.hashes.length)

  expected_assets = table.hashes.map do |row|
    {
      "assetid" => row["AssetId"],
      "confirmations" => row["Confirmations"].to_i,
      "reqsigners" => row["M"].to_i,
      "totalsigners" => row["N"].to_i,
      "maxtrade" => row["MaxTrade"].to_f,
      "mintrade" => row["MinTrade"].to_f,
      "unitexponent" => row["UnitExponent"].to_i,
    }
  end

  expected_assets.each do |expected_asset|
    expect(assets).to include(expected_asset)
  end
end

When(/^node "(.*?)" votes for no assets$/) do |arg1|
  node = @nodes[arg1]
  node.rpc("setvote", {})
  asset_votes = node.rpc("getvote")["assets"]
  expect(asset_votes).to eq([])
end
When(/^node "(.*?)" sends a liquidity of "(.*?)" buy and "(.*?)" sell on unit "(.*?)" from address "(.*?)" with identifier "(.*?)"$/) do |arg1, arg2, arg3, arg4, arg5, arg6|
  node = @nodes[arg1]
  buy = parse_number(arg2)
  sell = parse_number(arg3)
  unit = arg4
  address = @addresses[arg5]
  identifier = arg6
  node.rpc("liquidityinfo", unit, buy, sell, address, identifier)
end

Then(/^sending from node "(.*?)" a liquidity of "(.*?)" buy and "(.*?)" sell on unit "(.*?)" from address "(.*?)" with identifier "(.*?)" should fail$/) do |arg1, arg2, arg3, arg4, arg5, arg6|
  expect {
    step %Q{node "#{arg1}" sends a liquidity of "#{arg2}" buy and "#{arg3}" sell on unit "#{arg4}" from address "#{arg5}" with identifier "#{arg6}"}
  }.to raise_error
end

Then(/^node "(.*?)" should reach a total liquidity info of "(.*?)" buy and "(.*?)" sell on unit "(.*?)"$/) do |arg1, arg2, arg3, arg4|
  node = @nodes[arg1]
  buy = parse_number(arg2)
  sell = parse_number(arg3)
  unit = arg4
  wait_for do
    info = node.rpc("getliquidityinfo", unit)
    total = info["total"]
    expect(total["buy"]).to eq(buy)
    expect(total["sell"]).to eq(sell)
  end
end

Given(/^the maximum identifier per custodian is (\d+)$/) do |arg1|
  max = File.read(File.expand_path("../../../../liquidityinfo.cpp", __FILE__)).scan(/#define MAX_LIQUIDITY_INFO_PER_CUSTODIAN (\d+)/).first.first
  expect(max).to eq(arg1)
end

When(/^node "(.*?)" sends these liquidity info from address "(.*?)" on unit "(.*?)" at (\d+) second interval:$/) do |arg1, arg2, arg3, arg4, table|
  node = @nodes[arg1]
  address = @addresses[arg2]
  unit = arg3
  interval = arg4.to_i
  table.hashes.each do |row|
    buy = parse_number(row["Buy"])
    sell = parse_number(row["Sell"])
    identifier = row["Identifier"]
    node.rpc("liquidityinfo", unit, buy, sell, address, identifier)
    time_travel(interval)
  end
end

Then(/^node "(.*?)" should have these liquidity info from custodian "(.*?)" on unit "(.*?)":$/) do |arg1, arg2, arg3, table|
  node = @nodes[arg1]
  address = @addresses[arg2]
  unit = arg3
  wait_for do
    infos = node.rpc('getliquiditydetails', unit)
    infos = infos[address]
    expect(infos).not_to be_nil
    infos = infos.map do |identifier, info|
      {
        "Identifier" => identifier,
        "Buy" => info["buy"],
        "Sell" => info["sell"],
      }
    end
    expected = table.hashes.map do |row|
      {
        "Identifier" => row["Identifier"],
        "Buy" => parse_number(row["Buy"]),
        "Sell" => parse_number(row["Sell"]),
      }
    end
    expect(infos).to eq(expected)
  end
end

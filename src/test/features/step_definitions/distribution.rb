When(/^node "(.*?)" distributes "(.*?)" Bitcoins$/) do |arg1, arg2|
  #distribute <cutoff timestamp> <amount> [<proceed>]
  node = @nodes[arg1]
  time = Time.parse(node.top_block["time"])
  @distribution = @nodes[arg1].rpc("distribute", time.to_i, parse_number(arg2))
  @supply = node.info["moneysupply"]
end

Then(/^the distribution should send "(.*?)" Bitcoins to "(.*?)", adjusted by the real BlockShares supply$/) do |arg1, arg2|
  address_distribution = @distribution["distributions"].find { |d| d["nu_address"] == @addresses[arg2] }
  raise "No distribution found for address #{arg2.inspect} (#{@addresses[arg2]})" unless address_distribution
  adjusted = parse_number(arg1) / @supply * 252e3
  expect(address_distribution["dividends"]).to be_within(0.01).of(adjusted)
end

Then(/^the distribution should not send anything to "(.*?)"$/) do |arg1|
  address_distribution = @distribution["distributions"].find { |d| d["nu_address"] == @addresses[arg1] }
  expect(address_distribution).to be_nil
end


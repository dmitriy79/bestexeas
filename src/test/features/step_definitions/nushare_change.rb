Then(/^the total unspent amount of all the Bitcoin keys on node "(.*?)" should be "(.*?)"$/) do |arg1, arg2|
  expected = parse_number(arg2)
  node = @nodes[arg1]
  addresses = node.rpc("getdividendaddresses", "").keys
  unspent_amounts = addresses.map do |address|
    unspent = node.rpc("listunspent", 0, 999999, [address])
    unspent.map { |u| u["amount"] }
  end
  total_unspent = unspent_amounts.flatten.inject(:+)
  expect(total_unspent).to eq(expected)
end

Then(/^the total unspent amount of all the Bitcoin keys on node "(.*?)" should be the same as her balance$/) do |arg1|
  balance = @nodes[arg1].rpc("getbalance")
  step %Q(the total unspent amount of all the Bitcoin keys on node "#{arg1}" should be "#{balance}")
end

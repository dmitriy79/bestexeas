require 'bigdecimal'
require 'bigdecimal/util'

Given(/^the network has switched to dynamic fees$/) do
  step 'the network is at protocol 2.0'
end

When(/^node "(.*?)" votes for an (\w+) fee of ([\d.,]+)$/) do |arg1, unit_name, arg2|
  node = @nodes[arg1]
  fee = parse_number(arg2)
  vote = {
    "fees" => {
      unit(unit_name) => fee,
    },
  }

  node.rpc("setvote", vote)
  expect(node.rpc("getvote")["fees"]).to eq(vote["fees"])
end

When(/^node "(.*?)" finds blocks until the BKS fee is ([\d.,]+)$/) do |arg1, arg2|
  node = @nodes[arg1]
  fee = parse_number(arg2)
  begin
    wait_for do
      loop do
        time_travel(10*60)
        node.rpc("generatestake")
        break if node.info["paytxfee"] == fee
      end
      true
    end
  rescue
    p node.info["paytxfee"]
    raise
  end
end

When(/^node "(.*?)" finds enough blocks for a fee vote to pass$/) do |arg1|
  node = @nodes[arg1]
  3.times do
    time_travel(10*60)
    node.rpc("generatestake")
  end
end

When(/^node "(.*?)" finds enough blocks for the voted fee to be effective$/) do |arg1|
  node = @nodes[arg1]
  3.times do
    time_travel(10*60)
    node.rpc("generatestake")
  end
end

Then(/^the (\w+) fee should be ([\d.]+)$/) do |unit_name, arg1|
  wait_for do
    @nodes.all? do |name, node|
      expect(node.unit_rpc(unit(unit_name), "getinfo")["paytxfee"]).to eq(parse_number(arg1))
    end
  end
end

Then(/^transaction "(.*?)" on node "(.*?)" should have a fee of ([\d.,]+) per 1000 bytes$/) do |arg1, arg2, arg3|
  node = @nodes[arg2]
  tx = @tx[arg1]
  expected_fee_per_kilobyte = parse_number(arg3)
  wait_for do
    info = node.rpc("getrawtransaction", tx, 1)
    bytes = info["hex"].size / 2
    expected_fee = (expected_fee_per_kilobyte.to_d / 1000 * bytes).ceil(4)
    total_out = info["vout"].map { |output| output["value"].to_d }.inject(&:+)
    total_in = info["vin"].map do |input|
      in_info = node.rpc("getrawtransaction", input["txid"], 1)
      in_info["vout"][input["vout"]]["value"].to_d
    end.inject(&:+)
    fee = total_in - total_out
    expect(fee.to_d).to be_within(0.001).of(expected_fee)
  end
end


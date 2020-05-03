When(/^the public key of address "(.*?)" is retreived from node "(.*?)"$/) do |arg1, arg2|
  address_name = arg1
  address = @addresses[address_name]
  node = @nodes[arg2]
  @pubkeys[address] = node.unit_rpc(@unit[address], "validateaddress", address)["pubkey"]
  raise "Public key not found" unless @pubkeys[address]
end

When(/^node "(.*?)" creates a multisig address "(.*?)" requiring (\d+) keys? from the public keys (.*?)$/) do |arg1, arg2, arg3, arg4|
  node = @nodes[arg1]
  address_name = arg2
  required = arg3.to_i
  addresses = arg4.scan(/"(.*?)"/).map(&:first).map { |name| @addresses[name] }
  pubkeys = addresses.map { |address| @pubkeys[address] }

  result = node.rpc("createmultisig", required, pubkeys)
  @addresses[address_name] = result["address"]
end

When(/^node "(.*?)" adds a multisig address "(.*?)" requiring (\d+) keys? from the public keys (.*?)$/) do |arg1, arg2, arg3, arg4|
  node = @nodes[arg1]
  address_name = arg2
  required = arg3.to_i
  addresses = arg4.scan(/"(.*?)"/).map(&:first).map { |name| @addresses[name] }
  pubkeys = addresses.map { |address| @pubkeys[address] }

  result = node.rpc("addmultisigaddress", required, pubkeys)
  @addresses[address_name] = result
end

When(/^node "(.*?)" adds a BlockCredit multisig address "(.*?)" requiring (\d+) keys? from the public keys (.*?)$/) do |arg1, arg2, arg3, arg4|
  node = @nodes[arg1]
  address_name = arg2
  required = arg3.to_i
  addresses = arg4.scan(/"(.*?)"/).map(&:first).map { |name| @addresses[name] }
  pubkeys = addresses.map { |address| @pubkeys[address] }

  result = node.unit_rpc('C', "addmultisigaddress", required, pubkeys)
  @addresses[address_name] = result
end

When(/^node "(.*?)" generates a raw (BlockCredit |)transaction "(.*?)" to send the amount sent to address "(.*?)" in transaction "(.*?)" to:$/) do |arg1, unit_name, arg2, arg3, arg4, table|
  node = @nodes[arg1]
  raw_transaction_name = arg2
  address = @addresses[arg3]
  tx = @tx[arg4]
  unit = unit(unit_name)

  outputs = []
  source_tx = node.unit_rpc(unit, "getrawtransaction", tx, 1)
  source_tx["vout"].each do |output|
    if output["scriptPubKey"]["addresses"] == [address]
      outputs << {"txid" => tx, "vout" => output["n"]}
    end
  end
  raise "No output found to address #{address} in transaction #{tx}" if outputs.empty?

  recipients = {}
  table.hashes.each do |hash|
    address = @addresses[hash["Address"]]
    value = parse_number(hash["Value"])
    recipients[address] = value
  end

  result = node.unit_rpc(unit, "createrawtransaction", outputs, recipients)
  @raw_tx[raw_transaction_name] = result
end

When(/^node "(.*?)" signs the raw (BlockCredit |)transaction "(.*?)"$/) do |arg1, unit_name, arg2|
  node = @nodes[arg1]
  raw_tx = @raw_tx[arg2]
  result = node.unit_rpc(unit(unit_name), "signrawtransaction", raw_tx)
  signed_raw_tx = result["hex"]
  raise "Raw transaction was not modified" if signed_raw_tx == raw_tx
  @raw_tx[arg2] = signed_raw_tx
  @raw_tx_complete[arg2] = result["complete"]
end

Then(/^the raw transaction "(.*?)" should be complete$/) do |arg1|
  expect(@raw_tx_complete[arg1]).to eq(true)
end

When(/^node "(.*?)" sends the raw transaction "(.*?)"$/) do |arg1, arg2|
  node = @nodes[arg1]
  raw_tx = @raw_tx[arg2]
  node.rpc("sendrawtransaction", raw_tx, 1)
end

Then(/^node "(.*?)" should reach an unspent amount of "(.*?)" on address "(.*?)"$/) do |arg1, arg2, arg3|
  node = @nodes[arg1]
  expected_amount = parse_number(arg2)
  address = @addresses[arg3]
  wait_for do
    unspent = node.rpc("listunspent", 0, 999999, [address])
    amount = unspent.map { |u| u["amount"] }.inject(0, :+)
    expect(amount).to eq(expected_amount)
  end
end

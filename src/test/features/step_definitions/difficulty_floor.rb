Then(/^the difficulty should be below ([\d.]+)$/) do |arg1|
  node = @nodes.values.first
  expect(node.info["difficulty"]).to be < arg1.to_f
end

When(/^node "(.*?)" finds (\d+) blocks at (\d+) days interval$/) do |arg1, arg2, arg3|
  arg2.to_i.times do
    @nodes[arg1].generate_stake
    @nodes.values.each do |node|
      node.rpc("timetravel", arg3.to_i * 24 * 60 * 60)
    end
  end
end

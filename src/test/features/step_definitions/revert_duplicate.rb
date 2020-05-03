When(/^node "(.*?)" sends a duplicate "([^"]*?)" of block "([^"]*?)"$/) do |node, duplicate, original|
  @blocks[duplicate] = @nodes[node].rpc("duplicateblock", @blocks[original])
end

When(/^node "(.*?)" sends a duplicate "([^"]*?)" of block "([^"]*?)" not received by node "(.*?)"$/) do |node, duplicate, original, other|
  @nodes[other].rpc("ignorenextblock")
  @blocks[duplicate] = @nodes[node].rpc("duplicateblock", @blocks[original])
end

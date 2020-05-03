
When(/^this RPC request is sent to node "(.*?)":$/) do |arg1, string|
  @result = @nodes[arg1].raw_rpc('8', string)
end

Then(/^the result should contain (\d+) getinfo answers$/) do |arg1|
  expect(@result.class).to eq(Array)
  expect(@result.size).to eq(2)
  expect(@result[0]["result"]["difficulty"]).not_to be_nil
  expect(@result[1]["result"]["difficulty"]).not_to be_nil
end


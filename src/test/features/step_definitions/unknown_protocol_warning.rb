Given(/^node "(.*?)" votes for a protocol version above the current protocol$/) do |arg1|
  @nodes[arg1].rpc("setversionvote", 99999999)
end

Then(/^node "(.*?)" should have no error$/) do |arg1|
  expect(@nodes[arg1].info["errors"]).to eq("")
end

Then(/^node "(.*?)" should have a warning about the unknown protocol$/) do |arg1|
  expect(@nodes[arg1].info["errors"]).to eq("Unknown protocol vote received. You may need to upgrade your client.")
end


When(/^node "(.*?)" imports wallet "(.*?)"$/) do |arg1, arg2|
    node = @nodes[arg1]
    FileUtils.cp arg2, node.shared_path(File.basename(arg2))
    node.rpc "importnusharewallet", node.shared_path_in_container(File.basename(arg2))
end

When(/^node "(.*?)" imports encrypted wallet "(.*?)" with password "(.*?)"$/) do |arg1, arg2, arg3|
    node = @nodes[arg1]
    FileUtils.cp arg2, node.shared_path(File.basename(arg2))
    node.rpc "importnusharewallet", node.shared_path_in_container(File.basename(arg2)), arg3
end

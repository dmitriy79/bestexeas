
Given(/^a node "(.*?)" started with a votenotify script$/) do |arg1|
  name = arg1
  options = {
    image: "bcenet/empty",
    links: @nodes.values.map(&:name),
    args: {
      debug: true,
      timetravel: timeshift,
      votenotify: "/shared/votenotify.sh",
    },
  }
  node = CoinContainer.new(options)
  @nodes[name] = node
  node.wait_for_boot
end

Given(/^the votenotify script of node "(.*?)" is written to dump the vote and sign it with address "(.*?)"$/) do |arg1, arg2|
  node = @nodes[arg1]
  script = <<-EOF.gsub(/^    /, "")
    #!/bin/bash

    set -x

    DAEMON=/code/bcexchanged
    ARGS="--datadir=/root/.bcexchange"
    VOTE_PATH=#{node.shared_path_in_container("vote.json")}
    SIGNATURE_PATH=#{node.shared_path_in_container("vote.json.signature")}
    ADDRESS=#{@addresses[arg2]}

    $DAEMON $ARGS getvote >$VOTE_PATH
    $DAEMON $ARGS signmessage $ADDRESS <$VOTE_PATH >$SIGNATURE_PATH
    chmod 666 $VOTE_PATH $SIGNATURE_PATH
  EOF
  script_path = node.shared_path("votenotify.sh")
  File.open(script_path, "w") { |f| f.write(script) }
  File.chmod 0755, script_path
end

When(/^the data feed "(.*?)" returns the content of the dumped vote and signature on node "(.*?)"$/) do |arg1, arg2|
  data_feed = @data_feeds[arg1]
  node = @nodes[arg2]
  vote_path = node.shared_path("vote.json")
  signature_path = node.shared_path("vote.json.signature")

  wait_for do
    raise "no vote" unless File.exist?(vote_path)
    vote = File.read(vote_path)
    raise "no signature" unless File.exist?(signature_path)
    signature = File.read(signature_path)
    raise "empty vote" if vote.empty?
    raise "empty signature" if signature.empty?
    data_feed.set(vote)
    data_feed.set_signature(signature)
    true
  end
end


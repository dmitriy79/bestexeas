
Before do
  @data_feeds = {}
end

class DataFeedContainer
  def initialize(options = {})
    default_options = {
      image: "bcexchange/data_feed",
    }

    options = default_options.merge(options)

    bash_cmds = []

    bash_cmds += ["ruby /data_feed.rb -o 0.0.0.0"]

    command = [
      "stdbuf", "-oL", "-eL",
      '/bin/bash', '-c',
      bash_cmds.join("; "),
    ]

    create_options = {
      'Image' => options[:image],
      'WorkingDir' => '/',
      'Tty' => true,
      'Cmd' => command,
      'ExposedPorts' => {
        "4567/tcp" => {},
      },
    }
    @container = Docker::Container.create(create_options)

    start_options = {
      'PortBindings' => {
        "4567/tcp" => [{}],
      },
    }

    sleep 0.1
    @container.start(start_options)

    @json = @container.json
    @name = @json["Name"]

    retries = 0
    begin
      ports = @container.json["NetworkSettings"]["Ports"]
      if ports.nil?
        raise "Unable to get port. Usualy this means the daemon process failed to start. Container was #{@name}"
      end
    rescue
      if retries >= 3
        raise
      else
        sleep 0.1
        retries += 1
        retry
      end
    end

    @port = ports["4567/tcp"].first["HostPort"].to_i
    @host_ip = @container.json["NetworkSettings"]["Gateway"]
  end

  def url(path = '')
    "http://#@host_ip:#@port/#{path}"
  end

  def wait_for_boot
    require 'httparty'
    Timeout.timeout(10) do
      begin
        response = HTTParty.get(url)
        raise unless response.code == 200
      rescue Errno::ECONNREFUSED, RuntimeError
        sleep 0.1
        retry
      end
    end
  end

  def stop
    return unless @container

    @container.delete(force: true)
    @container = nil
  end

  def set(value)
    response = HTTParty.post(url('vote.json'), body: value)
    raise "Set vote failed" unless response.code == 200
    raise "Set vote mismatch" unless get == value
  end

  def get
    response = HTTParty.get(url('vote.json'))
    raise "Unable to retreive set vote" unless response.code == 200
    response.body
  end

  def set_status(value)
    response = HTTParty.post(url('status'), body: value)
    raise "Set status failed" unless response.code == 200
  end

  def set_signature(value)
    response = HTTParty.post(url('vote.json.signature'), body: value)
    raise "Set status failed" unless response.code == 200
  end
end

Given(/^a data feed "(.*?)"$/) do |arg1|
  @data_feeds[arg1] = data_feed = DataFeedContainer.new
  data_feed.wait_for_boot
end

After do
  @data_feeds.values.each(&:stop)
end

Given(/^the data feed "(.*?)" returns:$/) do |arg1, string|
  @data_feeds[arg1].set(string)
end

Given(/^sample vote "(.*?)" is:$/) do |arg1, string|
  @sample_votes ||= {}
  @sample_votes[arg1] = string
end

Given(/^the data feed "(.*?)" returns sample vote "(.*?)"$/) do |arg1, arg2|
  @data_feeds[arg1].set @sample_votes[arg2]
end

Given(/^the data feed "(.*?)" returns a status (\d+) with the body:$/) do |arg1, arg2, string|
  @data_feeds[arg1].set(string)
  @data_feeds[arg1].set_status(arg2)
end

Given(/^the data feed "(.*?)" returns sample vote "(.*?)" padded with spaces to (\d+) bytes$/) do |arg1, arg2, arg3|
  data_feed = @data_feeds[arg1]
  vote = @sample_votes[arg2]
  size = arg3.to_i

  vote += " " * (size - vote.size)
  data_feed.set(vote)
end

When(/^node "(.*?)" sets (?:her|his) data feed to the URL of "([^"]*?)"$/) do |arg1, arg2|
  @nodes[arg1].rpc("setdatafeed", @data_feeds[arg2].url("vote.json"))
end

When(/^node "(.*?)" sets (?:her|his) data feed to the URL of "(.*?)" with address "(.*?)"$/) do |arg1, arg2, arg3|
  @nodes[arg1].rpc("setdatafeed", @data_feeds[arg2].url("vote.json"), @data_feeds[arg2].url("vote.json.signature"), @addresses[arg3])
end

When(/^node "(.*?)" sets (?:her|his) data feed to the URL of "([^"]*?)" only on (.+)$/) do |arg1, arg2, parts|
  parts = parts.split(/,|and/).map(&:strip)
  @nodes[arg1].rpc("setdatafeed", @data_feeds[arg2].url("vote.json"), "", "", parts.join(","))
end

Then(/^the vote of node "(.*?)" (?:should be|is|should become):$/) do |arg1, string|
  begin
    wait_for do
      expect(JSON.pretty_generate(vote_without_version(@nodes[arg1].rpc("getvote")))).to eq(JSON.pretty_generate(JSON.parse(string)))
    end
  rescue
    errors = @nodes[arg1].info["errors"]
    puts errors unless errors.nil?
    raise
  end
end

Then(/^the vote of node "(.*?)" should be sample vote "([^"]*?)"$/) do |arg1, arg2|
  step "the vote of node \"#{arg1}\" should be:", @sample_votes.fetch(arg2)
end

Then(/^the vote of node "(.*?)" should be sample vote "([^"]*?)" with (.+) replaced from sample "(.*?)"$/) do |arg1, arg2, arg3, arg4|
  expected_vote = JSON.parse(@sample_votes.fetch(arg2))
  parts = arg3.split(/,|and/).map(&:strip)
  replacement_vote = JSON.parse(@sample_votes.fetch(arg4))

  parts.each do |part|
    expected_vote[part] = replacement_vote[part]
  end

  step "the vote of node \"#{arg1}\" should be:", expected_vote.to_json
end

Then(/^node "(.*?)" should use the data feed "([^"]*?)"$/) do |arg1, arg2|
  result = @nodes[arg1].rpc("getdatafeed")
  expect(result["url"]).to eq(@data_feeds[arg2].url("vote.json"))
end

Then(/^node "(.*?)" should use the data feed "(.*?)" with address "(.*?)"$/) do |arg1, arg2, arg3|
  result = @nodes[arg1].rpc("getdatafeed")
  expect(result["url"]).to eq(@data_feeds[arg2].url("vote.json"))
  expect(result["signatureurl"]).to eq(@data_feeds[arg2].url("vote.json.signature"))
  expect(result["signatureaddress"]).to eq(@addresses[arg3])
end

Given(/^data feed "(.*?)" shuts down$/) do |arg1|
  @data_feeds[arg1].stop
end

When(/^the vote of node "(.*?)" (?:is|should become) sample vote "(.*?)"$/) do |arg1, arg2|
  step "the vote of node \"#{arg1}\" is:", @sample_votes[arg2]
end

Given(/^node "(.*?)" sets (?:his|her) vote to sample vote "(.*?)"$/) do |arg1, arg2|
  step "node \"#{arg1}\" sets his vote to:", @sample_votes[arg2]
end

Given(/^the data feed "(.*?)" returns a status (\d+) with sample vote "(.*?)"$/) do |arg1, arg2, arg3|
  step "the data feed \"#{arg1}\" returns a status #{arg2} with the body:", @sample_votes[arg3]
end

Given(/^the data feed "(.*?)" is signed by node "(.*?)" with address "(.*?)"$/) do |arg1, arg2, arg3|
  data_feed = @data_feeds[arg1]
  node = @nodes[arg2]
  address = @addresses[arg3]

  vote = data_feed.get
  signature = node.rpc("signmessage", address, vote)
  @data_feeds[arg1].set_signature(signature)
end

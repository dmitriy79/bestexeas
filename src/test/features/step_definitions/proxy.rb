class ProxyContainer
  def initialize(options = {})
    default_options = {
      image: "bcexchange/proxy",
    }

    options = default_options.merge(options)

    create_options = {
      'Image' => options[:image],
      'Tty' => true,
      'ExposedPorts' => {
        "1080/tcp" => {},
      },
    }
    @container = Docker::Container.create(create_options)

    start_options = {
      'PortBindings' => {
        "1080/tcp" => [{}],
      },
    }

    @container.start(start_options)

    @json = @container.json
    @name = @json["Name"]

    ports = @container.json["NetworkSettings"]["Ports"]
    @port = ports["1080/tcp"].first["HostPort"].to_i
    @ip = @container.json["NetworkSettings"]["Gateway"]
  end

  attr_reader :ip, :port

  def shutdown
    @container.kill
  end
end

Given(/^a proxy$/) do
  @proxy = ProxyContainer.new
end

After do
  @proxy.shutdown if @proxy
end

Given(/^a node "(.*?)" connected through the proxy$/) do |arg1|
  name = arg1
  options = {
    image: "bcenet/empty",
    links: @nodes.values.map(&:name),
    args: {
      debug: true,
      timetravel: timeshift,
      proxy: [@proxy.ip, @proxy.port].join(":"),
    },
    netadmin: true,
    before_start_commands: [
      "iptables -A OUTPUT -m state --state ESTABLISHED -j ACCEPT",
      "iptables -A OUTPUT -d #{@proxy.ip} -p tcp --dport #{@proxy.port} -j ACCEPT",
      "iptables -A OUTPUT -j REJECT",
    ],
  }
  node = CoinContainer.new(options)
  @nodes[name] = node
  node.wait_for_boot
end

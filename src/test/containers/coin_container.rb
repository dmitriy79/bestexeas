require 'docker'
require 'httparty'

class CoinContainer
  def initialize(options = {})
    default_options = {
      image: "bcexchange/base",
      bind_code: true,
      shutdown_at_exit: true,
      delete_at_exit: false,
      remove_addr_after_shutdown: true,
      remove_wallet_after_shutdown: false,
      remove_wallet_before_startup: false,
      before_start_commands: [],
    }

    options = default_options.merge(options)
    @options = options

    links = options[:links]
    case links
    when Hash
      links = links.map { |link_name, alias_name| [link_name, alias_name] }
    when Array
      links = links.map do |n|
        name = case n
               when String then n
               when CoinContainer then n.name
               else raise "Unknown link: #{n.inspect}"
               end
        [name, name.sub(/^\//, '')]
      end
    when nil
      links = []
    else
      raise "Invalid links"
    end
    @links = links

    name = options[:name]

    if options[:link_with_connect]
      connect_method = "connect"
    else
      connect_method = options[:connect_method] || "addnode"
    end
    connects = links.map do |linked_name, alias_name|
      upname = alias_name.upcase
      "-#{connect_method}=$#{upname}_PORT_12239_TCP_ADDR:$#{upname}_PORT_12239_TCP_PORT"
    end

    default_args = {
      datadir: "/root/.bcexchange",
      # testnet, user and password are already set in bcexchange.conf
      printtoconsole: true,
      rpcallowip: '*.*.*.*',
      logtimestamps: true,
      keypool: 1,
      stakegen: false,
      unpark: false,
      checkblocks: -1,
    }

    args = default_args.merge(options[:args] || {})

    cmd_args = args.map do |key, value|
      case value
      when true
        "-#{key}"
      when false
        "-#{key}=0"
      when Numeric
        "-#{key}=#{value}"
      else
        "-#{key}=\"#{value}\""
      end
    end
    cmd_args += connects

    bash_cmds = []

    bash_cmds += ["echo Node name: #{name}"] if name

    bash_cmds += ["set -x"]

    if options[:show_environment]
      bash_cmds += ["echo Environment:", "env"]
    end

    if options[:remove_wallet_before_startup]
      bash_cmds += ["rm -f /root/.bcexchange/testnet/wallet*.dat"]
    end

    bash_cmds += options[:before_start_commands]

    bash_cmds += ["./bcexchanged " + cmd_args.join(" ")]

    if options[:remove_addr_after_shutdown]
      bash_cmds += ["rm /root/.bcexchange/testnet/addr.dat"]
    end

    if options[:remove_wallet_after_shutdown]
      bash_cmds += ["rm /root/.bcexchange/testnet/wallet*.dat"]
    end

    command = [
      "stdbuf", "-oL", "-eL",
      '/bin/bash', '-c',
      bash_cmds.join("; "),
    ]

    create_options = {
      'Image' => options[:image],
      'WorkingDir' => '/code',
      'Tty' => true,
      'Cmd' => command,
      'ExposedPorts' => {
        "12239/tcp" => {},
        "12240/tcp" => {},
        "12241/tcp" => {},
      },
    }
    node_container = Docker::Container.create(create_options)
    @container = node_container
    @json = @container.json

    sleep 0.1
    start
  end

  def start
    node_container = @container
    options = @options
    links = @links

    if options[:shutdown_at_exit]
      at_exit do
        shutdown rescue nil
      end
    end
    if options[:delete_at_exit]
      at_exit do
        container.delete(force: true)
      end
    end

    start_options = {
      'PortBindings' => {
        "12240/tcp" => [{}],
        "12241/tcp" => [{}],
        "12239/tcp" => [{}],
      },
      'Links' => links.map { |link_name, alias_name| "#{link_name}:#{alias_name}" },
    }
    if options[:netadmin]
      start_options["CapAdd"] ||= []
      start_options["CapAdd"] << "NET_ADMIN"
    end

    start_options['Binds'] = []
    if options[:bind_code]
      start_options['Binds'] << "#{File.expand_path('../../..', __FILE__)}:/code"
    end
    start_options['Binds'] << "#{shared_root}:/shared"

    node_container.start(start_options)

    @json = @container.json
    @name = @json["Name"]

    retries = 0
    begin
      ports = node_container.json["NetworkSettings"]["Ports"]
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

    @rpc_ports = {
      '8' => ports["12240/tcp"].first["HostPort"].to_i,
      'C' => ports["12241/tcp"].first["HostPort"].to_i,
    }
    @rpc_port = @rpc_ports['8']
    @port= ports["12239/tcp"].first["HostPort"].to_i
  end

  attr_reader :rpc_port, :port, :container, :name

  def json
    container.json
  end

  def id
    @json["Id"]
  end

  def rpc(method, *params)
    unit_rpc('8', method, *params)
  end

  def unit_rpc(unit, method, *params)
    data = {
      method: method,
      params: params,
      id: 'jsonrpc',
    }
    result = raw_rpc(unit, data.to_json)
    raise result.inspect if result["error"]
    result["result"]
  end

  def raw_rpc(unit, body)
    rpc_port = @rpc_ports.fetch(unit)
    url = "http://localhost:#{rpc_port}/"
    auth = {
      username: "user",
      password: "pass",
    }
    response = HTTParty.post url, body: body, headers: { 'Content-Type' => 'application/json' }, basic_auth: auth
    JSON.parse(response.body)
  end

  def wait_for_boot
    begin
      rpc("getinfo")
    rescue Errno::ECONNREFUSED, Errno::ECONNRESET, EOFError, Errno::EPIPE
      sleep 0.1
      retry
    end
  end

  def commit(repo)
    image = container.commit
    image.tag 'repo' => repo, 'force' => true
  end

  def shutdown
    rpc("shutdown")
  end

  def wait_for_shutdown
    container.wait
  end

  def restart
    @container.restart
  end

  def block_count
    rpc("getblockcount").to_i
  end

  def generate_stake(parent = nil)
    rpc("generatestake", *[parent].compact)
  end

  def top_hash
    rpc("getblockhash", rpc("getblockcount"))
  end

  def top_block
    rpc("getblock", top_hash)
  end

  def connection_count
    rpc("getconnectioncount").to_i
  end

  def info
    rpc "getinfo"
  end

  def new_address(account = "")
    rpc "getnewaddress", account
  end

  def shared_path(filename)
    File.join(shared_root, filename)
  end

  def shared_path_in_container(filename)
    File.join("/shared", filename)
  end

  def shared_root
    shared_root = File.expand_path("../tmp", __FILE__)
    Dir.mkdir(shared_root) unless File.exist?(shared_root)
    container_shared_root = File.join(shared_root, self.id)
    unless File.exist?(container_shared_root)
      Dir.mkdir(container_shared_root)
      at_exit do
        begin
          Dir.rmdir(container_shared_root)
        rescue Errno::ENOTEMPTY
        end
      end
    end
    container_shared_root
  end
end

# A little ruby library which can allow client code to programattically drive service runner.
# Great for unit tests!

# if we're talkin' ruby 1.9, we'll use built in json, otherwise use
# the pure ruby library sittin' here
$:.push(File.dirname(__FILE__))
begin
  require 'json'
rescue LoadError
  require "json/json.rb"
end

module BrowserPlus
  module ProcessController
    # perform a blocking read.  the third parameter is a magic duck:
    # 1. if it evaluates to false, we'll block the full timeo
    # 2. if it is a pattern, we'll  block until either timeo expires OR
    #    we get output which matches the pattern
    def mypread(pio, timeo, lookFor = false)
      output = String.new
      while nil != select( [ pio ], nil, nil, timeo )  
        output += pio.sysread(1024) 
        break if output.length && lookFor && output =~ lookFor
      end
      output
    end

    def mypread_raise(pio, timeo, lookFor = false)
      str = mypread(pio, timeo, lookFor)
      raise "read failed" if lookFor != false && str !~ lookFor
      str
    end
  end

  class Service
    def initialize path
      sr = findServiceRunner
      raise "can't execute ServiceRunner: #{sr}" if !File.executable? sr
      @srp = IO.popen("#{sr} #{path}", "w+")
      str = mypread_raise(@srp, 0.5, /service initialized/)
      @instance = nil
    end

    # allocate a new instance
    def allocate
      @srp.syswrite "allocate\n"
      str = mypread_raise(@srp, 3.0, /allocated/)
      num = str.match(/^allocated: (\d+)/)[1]
      Instance.new(@srp, num)
    end

    # invoke a function on an automatically allocated instance of the service
    def invoke f, a = nil
      @instance = allocate if @instance == nil
      @instance.invoke f, a
    end

    def method_missing func, *args
      invoke func, args[0]
    end

    def shutdown
      if @instance != nil
        @instance.destroy
        @instance = nil
      end
      @srp.close
      @srp = nil 
    end

    private


    # attempt to find the ServiceRunner binary, a part of the BrowserPlus
    # SDK. (http://browserplus.yahoo.com)
    def findServiceRunner
      # first, try relative to this repo 
      srBase = File.join(File.dirname(__FILE__), "..", "..", "bpsdk", "bin")
      candidates = [
                    File.join(srBase, "ServiceRunner.exe"),
                    File.join(srBase, "ServiceRunner"),             
                   ]
      
      # now use BPSDK_PATH env var if present
      if ENV.has_key? 'BPSDK_PATH'
        candidates.push File.join(ENV['BPSDK_PATH'], "bin", "ServiceRunner.exe")
        candidates.push File.join(ENV['BPSDK_PATH'], "bin", "ServiceRunner")
      end
      
      if ENV.has_key? 'SERVICERUNNER_PATH'
        candidates.push(ENV['SERVICERUNNER_PATH'])
      end

      candidates.each { |p|
        return p if File.executable? p
      }
      nil
    end

    include ProcessController
  end
  
  class Instance
    # private!!
    def initialize p, n
      @iid = n
      @srp = p
    end

    def invoke func, args
      args = JSON.generate(args).gsub("'", "\\'")
      cmd = "inv #{func.to_s}"
      cmd += " '#{args}'" if args != "null"
      cmd += "\n"
      # always select the current instance
      @srp.syswrite "select #{@iid}\n"
      @srp.syswrite cmd
      str = mypread(@srp, 10.0, /^error:|^(?:".*")\n|^}$|^(?:no such function: #{func.to_s})$/m)
      raise str if str =~ /no such function: #{func.to_s}/ || str =~ /^error:/
      JSON.parse("[#{str}]")[0]
    end

    def destroy
      @srp.syswrite "destroy #{@iid}\n"
    end

    def method_missing func, *args
      invoke func, args[0]
    end
    private
    include ProcessController
  end

  def BrowserPlus.run path, &block
    s = BrowserPlus::Service.new(path)
    block.call(s)
    s.shutdown
  end
end

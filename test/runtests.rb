#!/usr/bin/env ruby
require 'logger'

require File.join( File.dirname(File.expand_path(__FILE__) ), 'util.rb' )

if File.exist?('log.txt')
  File.delete('log.txt')
end

$log = Logger.new('log.txt')

#BrowserPlus.FileAccess API Level Testing
#bugs can be found at bugs.browserplus.org

#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'
$log.info("log this")

# FileAccess
# -> Access the contents of files that the user has selected.
class TestFileAccess < Test::Unit::TestCase
  
  #SETUP
  def setup
    subdir = 'build/FileAccess'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    
    curDir = File.dirname(__FILE__)
    pathToService = File.join(curDir, "..", "build", "FileAccess")
    
    @s = BrowserPlus::Service.new(pathToService)

  end
  
  #TEARDOWN
  def teardown
    @s.shutdown
  end

  def test_aIntro
    puts "Test results are found in \"log.txt\"\n"
  end


  #BrowserPlus.FileAccess.chunk({params}, function{}())
  #Get a vector of objects that result from chunking a file. 
  #The return value will be an ordered list of file handles with each successive file representing a different chunk
  def test_chunk
    
    Dir.glob(File.join(File.dirname(__FILE__), "cases_chunk", "*.json")).each do |f|
      json = JSON.parse(File.read(f))
      x = String.new(f)
      
      @textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "testFiles", json["file"] )
      @textfile_uri = "path:" + @textfile_path
      
      @chunksize = json["chunkSize"]
      
      #testid: 1, chunnksize
      @testid = 1
      @allchunks = @s.chunk( { 'file' => @textfile_uri, 'chunkSize' => @chunksize  } )
      
      @iter = (File.size(@textfile_path)/@chunksize)
      
      for i in 0..@iter
        got = open(@allchunks[i]) { |f| f.read() }
        want = File.open(@textfile_path, "rb") { |f| f.read() }[i*@chunksize, @chunksize]
        assert_log(want, got, $log, x, @testid)
      end
      
      
      #Chunksize => 0 should not cause invocation error/crash ServiceRunner, should cause Runtime error? <----------------- BUG 213
      #@allchunks = @s.chunk({ 'file' => @textfile_uri, 'chunkSize'=>0    }  )
      
      
      #testid: 2, Chunksize => bigger than File.size()
      @testid+=1
      @allchunks = @s.chunk({ 'file' => @textfile_uri, 'chunkSize'=>File.size(@textfile_path)+1    }  )
      got = open(@allchunks[0]) { |f| f.read() }
      want = File.open(@textfile_path, "rb") { |f| f.read() }[0, File.size(@textfile_path)+1]
      assert_log(want, got, $log, x, @testid)
      
      @testid+=1 
      #testid: 3 Negative chunksize returns whole file. <------------------------ BUG 211
      @allchunks = @s.chunk({ 'file' => @textfile_uri, 'chunkSize'=> -5000 })
      got = open(@allchunks[0]) { |f| f.read() }
      want = File.open(@textfile_path, "rb") { |f| f.read() }[0, 5000]
      assert_log(want, got, $log, x, @testid)

    end #testcases
  end
  
  #BrowserPlus.FileAccess.getURL({params}, function{}())
  #Get a localhost url that can be used to attain the full contents of a file on disk. 
  def test_geturl
    Dir.glob(File.join(File.dirname(__FILE__), "cases_geturl", "*.json")).each do |f|
      json = JSON.parse(File.read(f))
      x = String.new(f)     
      @textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "testFiles", json["file"] )
      @textfile_uri = (( @textfile_path[0] == "/") ? "path:" : "path:/" ) + @textfile_path

      @testid = 1
      #testid: 1, use geturl to read entire file
      want = File.open(@textfile_path, "rb") { |f| f.read }
      url = @s.getURL({ 'file' => @textfile_uri })
      got = open(url) { |f| f.read }
      assert_log( want.gsub("\r\n","\n"), got.gsub("\r\n","\n"), $log, x, @testid)
    end #testcases
  end
  
  #BrowserPlus.FileAccess.read({params}, function{}())
  #Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read
    #attempt to softcode
    Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
      json = JSON.parse(File.read(f))
      x = String.new(f)     
      @textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "testFiles", json["file"] )
      @textfile_uri = "path:" + @textfile_path
      
      @binfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "testFiles", json["binaryFile"] )
      @binfile_uri = "path:" + @binfile_path
      
      @size = json["size"]
      @offset = json["offset"]
      @negoffset = json["negoffset"]
      @testid=1
      
      # XXX bug in servicerunner?  large file is not correctly outputed XXX
      # testid: 1, a simple test of the read() function, read a text file 
      # want = File.open(@textfile_path, "rb") { |f| f.read }
      # got = @s.read({ 'file' => @textfile_uri })
      # assert_log(want, got, $log, x, @testid)
      
      @testid+=1
      # testid: 2, read() doesn't support binary data!  assert an exception is raised
      begin
        got = @s.read({ 'file' => @binfile_uri })
        assert_runTime(got, false, $log, x, @testid)
      rescue RuntimeError
        assert_runTime(got, true, $log, x, @testid)
      end


      @testid+=1
      # testid: 3, partial read
      want = File.open(@textfile_path, "rb") { |f| f.read(@size) } 
      got = @s.read({ 'file' => @textfile_uri, 'size' => @size })
      assert_log(want, got, $log, x, @testid)


      @testid+=1
      # testid; 4, partial read with offset
      want = File.open(@textfile_path, "rb") { |f| f.read() }[@offset, @size]
      got = @s.read({ 'file' => @textfile_uri, 'size' => @size, 'offset' => @offset })
      assert_log(want, got, $log, x, @testid)

      
      @testid+=1
      # testid: 5, ensure out of range errors are raised properly 
      begin
        got = @s.read({ 'file' => @textfile_uri, 'offset' => File.size(@textfile_path)+1 })
        assert_runTime(got, false, $log, x, @testid)
      rescue RuntimeError
        assert_runTime(got, true, $log, x, @testid)
      end

      
      @testid+=1
      # testid: 6, read with offset set at last byte of file
      want = ""
      got = @s.read({ 'file' => @textfile_uri, 'offset' => File.size(@textfile_path) }) 
      assert_log(want, got, $log, x, @testid)

      
      @testid+=1
      # testid: 7, ensure errors are raised properly for negetive offset
      begin
        got = @s.read({ 'file' => @textfile_uri, 'size' => @size, 'offset' => @negoffset })
        assert_runTime(got, false, $log, x, @testid)
      rescue RuntimeError
        assert_runTime(got, true, $log, x, @testid)
      end
      

      @testid+=1
      ##### testid: 8, read with size 0 <---------------------------------- BUG 208
      #got = @s.read({ 'file' => @textfile_uri, 'size' => 0, 'offset' => 0 })
      #want = ""
      #assert_log(want, got, $log, x, @testid)
      
    end # testcases read
    
  end
  
  #BrowserPlus.FileAccess.slice({params}, function{}())
  #Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice
    Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
      json = JSON.parse(File.read(f))
      x = String.new(f)     
      @textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "testFiles", json["file"] )
      @textfile_uri = "path:" + @textfile_path
      
      @size = json["size"]
      @offset = json["offset"]

      @testid = 1
      # testid: 1, read entire file, no offset or size
      want = File.open(@textfile_path, "rb") { |f| f.read }
      got = @s.slice({ 'file' => @textfile_uri})
      got = open(got) { |f| f.read }
      assert_log(want, got, $log, x, @testid)

      @testid+=1
      # testid: 2, slice with offset and size 
      want = File.open(@textfile_path, "rb") { |f| f.read() }[@offset, @size]
      got = @s.slice({ 'file' => @textfile_uri, 'offset' => @offset, 'size' => @size})
      got = open(got) { |f| f.read() }
      assert_log(want, got, $log, x, @testid)

      @testid+=1
      # testid: 3, slice a binary file --- should raise Runtime error? <------------------- BUG 213
      #assert_raise(RuntimeError) 
      # got = @s.slice({ 'file' => @binfile_uri, 'offset' => 5, 'size' => 20 }) 
      # puts got

      @testid+=1
      #testid: 4, Why is out-of-range runtime error not occurring in @s.slice as does in @s.read <------------------- BUG 209
      #assert_raise(RuntimeError) { @s.slice({ 'file' => @textfile_uri, 'offset' => 1024000 }) }
      
      @testid+=1    
      # testid: 5, offset set at last byte, should return nothing ("") 
      want = ""
      got = @s.slice({ 'file' => @textfile_uri, 'offset' => File.size(@textfile_path) })
      got = open(got) { |f| f.read() }
      assert_log(want, got, $log, x, @testid)
      
    end # testcases read

  end

  
  

  # XXX: test chunk and slice
end



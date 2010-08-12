#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'

class TestFileAccess < Test::Unit::TestCase
  def setup
    subdir = 'build/FileAccess'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    @cwd = File.dirname(File.expand_path(__FILE__))
    @service = File.join(@cwd, "../#{subdir}")

    curDir = File.dirname(__FILE__)
    @binfile_path = File.expand_path(File.join(curDir, "service.bin"))
    #@binfile_uri = (( @binfile_path[0] == "/") ? "file://" : "file:///" ) + @binfile_path
    @binfile_uri = "path:" + @binfile_path

    @textfile_path = File.expand_path(File.join(curDir, "services.txt"))
    #@textfile_uri = (( @textfile_path[0] == "/") ? "file://" : "file:///" ) + @textfile_path
    @textfile_uri = "path:" + @textfile_path
  end
  
  def teardown
  end

  def test_load_service
    BrowserPlus.run(@service) { |s|
    }
  end

  def test_geturl
    BrowserPlus.run(@service) { |s|
      want = File.open(@textfile_path, "rb") { |f| f.read }
      url = s.getURL({ 'file' => @textfile_uri })
      got = open(url) { |f| f.read }
      assert_equal want.gsub("\r\n","\n"), got.gsub("\r\n","\n")

      # yeah, same thing a second time
      want = File.open(@textfile_path, "rb") { |f| f.read }
      url = s.getURL({ 'file' => @textfile_uri })
      got = open(url) { |f| f.read }
      assert_equal want.gsub("\r\n","\n"), got.gsub("\r\n","\n")

      #want = File.read(@binfile_path)
      #url = s.getURL({ 'file' => @binfile_uri })
      #got = open(url) { |f| f.read }
      #assert_equal want, got
    }
  end

  # NEEDSWORK!!!  s.read() currently deadlocks.
  def test_read
    BrowserPlus.run(@service) { |s|
      # a simple test of the read() function, read a text file and a binary file
      want = File.open(@textfile_path, "rb") { |f| f.read }
    #  got = s.read({ 'file' => @textfile_uri })
    #  assert_equal want, got

    #  # read() doesn't support binary data!  assert an exception is raised
    #  assert_raise(RuntimeError) { got = s.read({ 'file' => @binfile_uri }) }

    #  # partial read
    #  want = File.open(@textfile_path, "rb") { |f| f.read(25) }
    #  got = s.read({ 'file' => @textfile_uri, 'size' => 25 })
    #  assert_equal want, got

    #  # partial read with offset
    #  want = File.open(@textfile_path, "rb") { |f| f.read(25) }[5, 20]
    #  got = s.read({ 'file' => @textfile_uri, 'size' => 20, 'offset' => 5 })
    #  assert_equal want, got

    #  # ensure out of range errors are raised properly 
    #  assert_raise(RuntimeError) { s.read({ 'file' => @textfile_uri, 'offset' => 1024000 }) }
    
    #  # read with offset set at last byte of file
    #  want = ""
    #  got = s.read({ 'file' => @textfile_uri, 'offset' => File.size(@textfile_path) }) 
    #  assert_equal want, got
    }
  end

  # XXX: test chunk and slice
end

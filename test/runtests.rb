#!/usr/bin/env ruby

require File.join(File.dirname(__FILE__), 'bp_service_runner')
require 'uri'
require 'test/unit'
require 'open-uri'

class TestFileAccess < Test::Unit::TestCase
  def setup
    curDir = File.dirname(__FILE__)
    pathToService = File.join(curDir, "..", "src", "build", "FileAccess")
    @s = BrowserPlus::Service.new(pathToService)

    @binfile_path = File.expand_path(File.join(curDir, "service.bin"))
    @binfile_uri = (( @binfile_path[0] == "/") ? "file://" : "file:///" ) + @binfile_path

    @textfile_path = File.expand_path(File.join(curDir, "services.txt"))
    @textfile_uri = (( @textfile_path[0] == "/") ? "file://" : "file:///" ) + @textfile_path
  end
  
  def teardown
    @s.shutdown
  end

  def test_read
    # a simple test of the read() function, read a text file and a binary file
    want = File.read @textfile_path
    got = @s.read({ 'file' => @textfile_uri })
    assert_equal want, got

    # read() doesn't support binary data!  assert an exception is raised
    assert_raise(RuntimeError) { got = @s.read({ 'file' => @binfile_uri }) }

    # partial read
    want = File.read(@textfile_path, 25)
    got = @s.read({ 'file' => @textfile_uri, 'size' => 25 })
    assert_equal want, got

    # partial read with offset
    want = File.read(@textfile_path, 25)[5, 20]
    got = @s.read({ 'file' => @textfile_uri, 'size' => 20, 'offset' => 5 })
    assert_equal want, got

    # ensure out of range errors are raised properly 
    assert_raise(RuntimeError) { @s.read({ 'file' => @textfile_uri, 'offset' => 1024000 }) }
    
    # read with offset set at last byte of file
    want = ""
    got = @s.read({ 'file' => @textfile_uri, 'offset' => File.size(@textfile_path) }) 
    assert_equal want, got
  end

  def test_geturl
    want = File.read(@textfile_path)
    url = @s.getURL({ 'file' => @textfile_uri })
    got = open(url) { |f| f.read }
    assert_equal want, got

    # yeah, same thing a second time
    want = File.read(@textfile_path)
    url = @s.getURL({ 'file' => @textfile_uri })
    got = open(url) { |f| f.read }
    assert_equal want, got

    #want = File.read(@binfile_path)
    #url = @s.getURL({ 'file' => @binfile_uri })
    #got = open(url) { |f| f.read }
    #assert_equal want, got
  end

  # XXX: test chunk and slice
end

#!/usr/bin/env ruby

require File.join(File.dirname(__FILE__), 'bp_service_runner')
require 'uri'
require "test/unit"


class TestFileAccess < Test::Unit::TestCase
  def setup
    pathToService = File.join(File.dirname(__FILE__), "..", "src", "build", "FileAccess")
    curDir = File.dirname(__FILE__)
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
    want = File.open(@binfile_path, "rb"){ |f| f.read }
    assert_raise(RuntimeError) { got = @s.read({ 'file' => @binfile_uri }) }

    # partial read
    want = File.read(@textfile_path, 25)
    got = @s.read({ 'file' => @textfile_uri, 'size' => 25 })
    assert_equal want, got

    # partial read with offset
    want = File.read(@textfile_path, 25)[5, 20]
    got = @s.read({ 'file' => @textfile_uri, 'size' => 20, 'offset' => 5 })
    assert_equal want, got
  end
end

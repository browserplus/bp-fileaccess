#!/usr/bin/env ruby

require File.join(File.dirname(__FILE__), 'bp_service_runner')
require 'uri'
require "test/unit"


class TestFileAccess < Test::Unit::TestCase
  def setup
    pathToService = File.join(File.dirname(__FILE__), "..", "src", "build", "FileAccess")
    curDir = File.dirname(__FILE__)
    @s = BrowserPlus::Service.new(pathToService)

    @binfile_path = File.join(File.dirname(__FILE__), "service.bin")
    p = File.expand_path(@binfile_path)
    @binfile_uri = ((p[0] == "/") ? "file://" : "file:///" ) + p

    @textfile_path = File.join(File.dirname(__FILE__), "services.txt")
    @textfile_uri = ((p[0] == "/") ? "file://" : "file:///" ) + File.expand_path(@textfile_path)
  end
  
  def teardown
    @s.shutdown
  end

  def test_read
    # a simple test of the read() function, read a text file and a binary file
    want = File.read @textfile_path
    got = @s.read({ 'file' => @textfile_uri })
    assert_equal want, got
  end
end

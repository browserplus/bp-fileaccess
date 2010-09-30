#!/usr/bin/env ruby

require File.join(File.dirname(File.dirname(File.expand_path(__FILE__))),
                  'external/dist/share/service_testing/bp_service_runner.rb')
require 'uri'
require 'test/unit'
require 'open-uri'
require 'rbconfig'
include Config

class TestFileAccess < Test::Unit::TestCase
  def setup
    subdir = 'build/FileAccess'
    if ENV.key?('BP_OUTPUT_DIR')
      subdir = ENV['BP_OUTPUT_DIR']
    end
    @cwd = File.dirname(File.expand_path(__FILE__))
    @service = File.join(@cwd, "../#{subdir}")
  end
  
  def teardown
  end

  def test_load_service
    BrowserPlus.run(@service) { |s|
    }
  end

  # BrowserPlus.FileAccess.chunk({params}, function{}())
  # Get a vector of objects that result from chunking a file.
  # The return value will be an ordered list of file handles with each successive file representing a different chunk
  def test_chunk
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_chunk", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"])
        file_uri = "path:" + file_path

        chunksize = json["chunkSize"]
        allchunks = s.chunk({ 'file' => file_uri, 'chunkSize' => chunksize })
        iter = (File.size(file_path) / chunksize)
        for i in 0..iter
          allchunks[i].slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
          got = open(allchunks[i], "rb") { |f| f.read() }
          want = File.open(file_path, "rb") { |f| f.read() }[i * chunksize, chunksize]
          assert_equal(want, got)
        end
      end
    }
  end

  # BrowserPlus.FileAccess.chunk({params}, function{}())
  # Get a vector of objects that result from chunking a file.
  # The return value will be an ordered list of file handles with each successive file representing a different chunk
  def test_chunk_bigger_than_filesize
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_chunk", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        want = File.open(file_path, "rb") { |f| f.read() }[0, File.size(file_path) + 1]
        chunksize = json["chunkSize"]
        allchunks = s.chunk({ 'file' => file_uri, 'chunkSize' => File.size(file_path) + 1} )
        allchunks[0].slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(allchunks[0], "rb") { |f| f.read() }
        assert_equal(want, got)

        # Negative chunksize returns whole file.
        allchunks = s.chunk({ 'file' => file_uri, 'chunkSize' => -5000} )
        allchunks[0].slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(allchunks[0], "rb") { |f| f.read() }
        want = File.open(file_path, "rb") { |f| f.read() }
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.getURL({params}, function{}())
  # Get a localhost url that can be used to attain the full contents of a file on disk.
  def test_geturl
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_geturl", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        # Use geturl to read entire file.
        want = File.open(file_path, "rb") { |f| f.read }
        url = s.getURL({ 'file' => file_uri })
        got = open(url, "rb") { |f| f.read }
        assert_equal(want.gsub("\r\n","\n"), got.gsub("\r\n","\n"))
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_text
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        # A simple test of the read() function, read a text file.
        want = File.open(textfile_path, "rb") { |f| f.read }
        got = s.read({ 'file' => textfile_uri })
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_binary
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        binfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["binaryFile"] )
        binfile_uri = "path:" + binfile_path

        # read() doesn't support binary data!  assert an exception is raised.
        assert_raise(RuntimeError) { s.read({ 'file' => binfile_uri })}
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_partial
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        size = json["size"]

        # Partial read.
        want = File.open(textfile_path, "rb") { |f| f.read(size) }
        got = s.read({ 'file' => textfile_uri, 'size' => size })
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_partial_offset
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        size = json["size"]
        offset = json["offset"]

        # Partial read with offset.
        want = File.open(textfile_path, "rb") { |f| f.read() }[offset, size]
        got = s.read({ 'file' => textfile_uri, 'size' => size, 'offset' => offset })
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_outofrange
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        # Ensure out of range errors are raised properly.
        assert_raise(RuntimeError) { s.read({ 'file' => textfile_uri, 'offset' => File.size(textfile_path) + 1}) }
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_offset_lastbyte
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        # Read with offset set at last byte of file.
        want = ""
        got = s.read({ 'file' => textfile_uri, 'offset' => File.size(textfile_path) })
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_negative_offset
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        binfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["binaryFile"] )
        binfile_uri = "path:" + binfile_path

        size = json["size"]
        offset = json["offset"]
        negoffset = json["negoffset"]

        # Ensure errors are raised properly for negetive offset.
        assert_raise(RuntimeError) { s.read({ 'file' => textfile_uri, 'size' => size, 'offset' => negoffset }) }
      end
    }
  end

  # BrowserPlus.FileAccess.read({params}, function{}())
  # Read the contents of a file on disk returning a string. If the file contains binary data an error will be returned
  def test_read_sizezero
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_read", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        textfile_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        textfile_uri = "path:" + textfile_path

        size = json["size"]
        offset = json["offset"]

        want = ""
        got = s.read({ 'file' => textfile_uri, 'size' => 0, 'offset' => 0 })
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.slice({params}, function{}())
  # Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice_text
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        size = json["size"]
        offset = json["offset"]

        # Read entire file, no offset or size.
        want = File.open(file_path, "rb") { |f| f.read }
        got = s.slice({ 'file' => file_uri})
        got.slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(got, "rb") { |f| f.read }
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.slice({params}, function{}())
  # Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice_text_offset_and_size
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        size = json["size"]
        offset = json["offset"]

        # Slice with offset and size.
        want = File.open(file_path, "rb") { |f| f.read() }[offset, size]
        got = s.slice({ 'file' => file_uri, 'offset' => offset, 'size' => size})
        got.slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(got, "rb") { |f| f.read() }
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.slice({params}, function{}())
  # Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice_binary
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        size = 5
        offset = 20

        want = File.open(file_path, "rb") { |f| f.read() }[offset, size]
        got = s.slice({ 'file' => file_uri, 'offset' => offset, 'size' => size})
        got.slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(got, "rb") { |f| f.read() }
        assert_equal(want, got)
      end
    }
  end

  # BrowserPlus.FileAccess.slice({params}, function{}())
  # Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice_1
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        size = json["size"]
        offset = File.size(file_path) + 5

        assert_raise(RuntimeError) { s.slice({ 'file' => file_uri, 'offset' => offset }) }
      end
    }
  end

  # BrowserPlus.FileAccess.slice({params}, function{}())
  # Given a file and an optional offset and size, return a new file whose contents are a subset of the first.
  def test_slice_2
    BrowserPlus.run(@service) { |s|
      Dir.glob(File.join(File.dirname(__FILE__), "cases_slice", "*.json")).each do |f|
        json = JSON.parse(File.read(f))
        file_path = File.join(File.dirname(File.expand_path(__FILE__)), "test_files", json["file"] )
        file_uri = "path:" + file_path

        size = json["size"]
        offset = json["offset"]

        # Offset set at last byte, should return nothing ("").
        want = ""
        got = s.slice({ 'file' => file_uri, 'offset' => File.size(file_path) })
        got.slice!(0..0) if CONFIG['arch'] =~ /mswin|mingw/
        got = open(got, "rb") { |f| f.read() }
        assert_equal(want, got)
      end
    }
  end
end

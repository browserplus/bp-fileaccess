#!/usr/bin/env ruby

require 'rbconfig'
require 'fileutils'
require 'pathname'
require 'open-uri'
require 'digest'
require 'digest/md5'
include Config

$pkg="mongoose"
$md5="01f70759e257764a007c81a2d80b9d76"
$tarball = "mongoose-2.4.tgz"
$url = "http://mongoose.googlecode.com/files/#{$tarball}"
$patches = [ "static_lib.patch" ]

if CONFIG['arch'] =~ /mswin/
    $platform = "Windows"
else
    $platform = "Darwin"
end

topDir = File.dirname(File.expand_path(__FILE__))
pkgDir = File.join(topDir, $pkg)
buildDir = File.join(topDir, "#{$pkg}_built")

if File.exist? buildDir
  puts "it looks like #{$pkg} is already built, sheepishly refusing to "
  puts "blow away your build output directory (#{buildDir})"
  puts ""
  puts "sometimes the best thing to do, is nothing."
  exit 0
end

puts "removing previous build artifacts..."
FileUtils.rm_rf(pkgDir)
FileUtils.rm_f("#{$pkg}.tar")
FileUtils.rm_rf(buildDir)

if !File.exist?($tarball)
    puts "fetching tarball from #{$url}"
    perms = $platform == "Windows" ? "wb" : "w"
    totalSize = 0
    lastPercent = 0
    interval = 5
    f = File.new($tarball, perms)
    f.write(open($url,
                 :content_length_proc => lambda {|t|
                     if (t && t > 0)
                         totalSize = t
                         STDOUT.printf("expect %d bytes, percent downloaded: ",
                                       totalSize)
                         STDOUT.flush
                     else 
                         STDOUT.print("unknown size to download: ")
                     end
                 },
                 :progress_proc => lambda {|s|
                     if (totalSize > 0)
                         percent = ((s.to_f / totalSize) * 100).to_i
                         if (percent/interval > lastPercent/interval)
                             lastPercent = percent
                             STDOUT.printf("%d ", percent)
                             STDOUT.printf("\n") if (percent == 100)
                         end
                     else
                         STDOUT.printf(".")
                     end
                     STDOUT.flush
                 }).read)
    f.close()
    s = File.size($tarball)
    if (s == 0 || (totalSize > 0 && s != totalSize))
        puts "download failed"
        FileUtils.rm_f($tarball)
        exit 1
    end
end

# now let's check the md5 sum
calculated_md5 = Digest::MD5.hexdigest(File.open($tarball, "rb") {
                                         |f| f.read
                                       })
if calculated_md5 != $md5
  puts "md5 mismatch, tarball is bogus, delete and retry"
  puts "(got #{calculated_md5}, wanted #{$md5})"
  exit 1
else
  puts "md5 validated! (#{calculated_md5} == #{$md5})"
end

# unpack the bugger
puts "unpacking tarball..."
if $platform == "Windows"
  throw "oopsie, implement me please"
else
  puts "unpacking #{$tarball}..."
  system("tar xzf #{$tarball}")

  # configure & build
  Dir.chdir(pkgDir) do 
    puts "patching #{$pkg}..."
    $patches.each { |p|
      system("patch -p1 < ../#{p}")
    }

    # we want these bits to work on tiger and leopard regardless of
    # where they're 
    # XXX: must figger out how to disable the pkg's own isysrootin'
#    ENV['CFLAGS'] = '-mmacosx-version-min=10.4 -isysroot /Developer/SDKs/MacOSX10.4u.sdk'
#    ENV['LDFLAGS'] = '-isysroot /Developer/SDKs/MacOSX10.4u.sdk'

    # make & install locally (see configure --prefix arg)
    puts "building #{$pkg}..."
    system("make bsd")

    # plunk it in builddir
    FileUtils.mkdir_p(File.join(buildDir, "include"))
    FileUtils.mkdir_p(File.join(buildDir, "lib"))
    FileUtils.install("libmongoose.a", File.join(buildDir, "lib"))
    FileUtils.install("mongoose.h", File.join(buildDir, "include"))
  end
end

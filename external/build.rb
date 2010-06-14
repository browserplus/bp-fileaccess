#!/usr/bin/env ruby

topDir = File.dirname(__FILE__)
require File.join(topDir, "bakery/ports/bakery")

$order = {
  :output_dir => File.join(File.dirname(__FILE__), "built"),
  :packages => [
                "mongoose",
                "boost",
                "bp-file",
                "service_testing"
               ],
  :verbose => true,
  :use_source => {
    "bp-file"=>File.join(topDir, "bp-file")
  },
  :use_recipe => {
    "bp-file"=>File.join(topDir, "bp-file", "recipe.rb")
  }
}

b = Bakery.new $order
b.build

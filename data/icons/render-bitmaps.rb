#!/usr/bin/env ruby

require "rexml/document"
require "ftools"
include REXML
INKSCAPE = '/usr/bin/env inkscape'
SRC = "#{Dir.pwd}/svg/gpm-batteries.svg"
OUT = "#{Dir.pwd}/png"

def renderit
  svg = Document.new(File.new(SRC, 'r'))
  svg.root.each_element("//g[contains(@inkscape:label,'plate')]") do |icon|
    if icon.attributes['inkscape:groupmode']=='layer' #only look inside layers, there may be pasted groups
      icon.each_element("rect") do |box|
        outfile = "#{box.attributes['inkscape:label']}.png"
        dir = "#{OUT}/" + outfile.gsub(/\/[^\/]+$/,'')
        cmd = "#{INKSCAPE} -i #{box.attributes['id']} -e \"#{OUT}/#{outfile}\" \"#{SRC}\" > /dev/null 2>&1"
        File.makedirs(dir) unless File.exists?(dir)
        system(cmd) unless File.exists?("#{OUT}/#{outfile}")
        print "."
      end
      puts ''
    end
  end
end

renderit unless !File.exists?(SRC)

#!/usr/bin/env perl

use strict;
use warnings;


mkdir 'docs';
my $text = '';

my $titles = {
    'Plugins:-Demuxer' => 'Demuxer Plugins',
    'Plugins:-Decoder' => 'Decoder Plugins',
    'Plugins:-Encoder' => 'Encoder Plugins',
    'Plugins:-Filter' => 'Filter Plugins',
    'Plugins:-Input' => 'Input Plugins',
    'Plugins:-Muxer' => 'Muxer Plugins',
    'Plugins:-Output' => 'Output Plugins',
    'Misc:-Ogg-Chaining-With-FLAC' => 'Notes: Ogg Chaining with FLAC',
    'Configuration Reference' => 'Configuration File Reference',
};

my $sections = [
  'Documentation',
  'Configuration Reference',
  'Plugins:-Input',
  'Plugins:-Demuxer',
  'Plugins:-Decoder',
  'Plugins:-Filter',
  'Plugins:-Encoder',
  'Plugins:-Muxer',
  'Plugins:-Output',
  'Misc:-Ogg-Chaining-With-FLAC',
];

foreach my $file (@$sections) {
  if(exists($titles->{$file})) {
      $text .= '# ' . $titles->{$file}."\n\n";
  }
  open(my $fh, '<', 'wiki/'.$file.'.md') or die "Unable to open $file: $!";
  $text .= join('',<$fh>);
  close($fh);
}

# fix-up internal links
$text =~ s/\(Plugins%3A-Input\)/(#input-plugins)/xmg;
$text =~ s/\(Plugins%3A-Input\#/(#/xmg;
$text =~ s/\(Plugins%3A-Demuxer\)/(#demuxer-plugins)/xmg;
$text =~ s/\(Plugins%3A-Demuxer\#/(#/xmg;
$text =~ s/\(Plugins%3A-Decoder\)/(#decoder-plugins)/xmg;
$text =~ s/\(Plugins%3A-Decoder\#/(#/xmg;
$text =~ s/\(Plugins%3A-Filter\)/(#filter-plugins)/xmg;
$text =~ s/\(Plugins%3A-Filter\#/(#/xmg;
$text =~ s/\(Plugins%3A-Encoder\)/(#encoder-plugins)/xmg;
$text =~ s/\(Plugins%3A-Encoder\#/(#/xmg;
$text =~ s/\(Plugins%3A-Muxer\)/(#muxer-plugins)/xmg;
$text =~ s/\(Plugins%3A-Muxer\#/(#/xmg;
$text =~ s/\(Plugins%3A-Output\)/(#output-plugins)/xmg;
$text =~ s/\(Plugins%3A-Output\#/(#/xmg;
$text =~ s/\(Misc%3A-Ogg-Chaining-With-FLAC\)/(#notes-ogg-chaining-with-flac)/xmg;
$text =~ s/\(Configuration%20Reference\)/(#configuration-file-reference)/xmg;
open(my $pandoc_fh, '|-', 'pandoc', '--toc-depth=2', '--toc=true', '-s', '--metadata','title=icecast-hls', '-f','markdown','-t','html','-o','docs/index.html');
print $pandoc_fh $text;
close($pandoc_fh);

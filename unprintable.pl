#!/usr/bin/perl
use strict;
use warnings;

while (<>)
{
  chomp;

  s/\e(\[[0-9;]*.)?/defined $1 ? "\e[0;32m\\e$1\e[0m" : "\e[0;31m\\e\e[0m"/ge;

  s/\x08/\e[0;34m<BS>\e[0m/g;
  s/\x0d/\e[0;34m<CR>\e[0m/g;
  s/\x0e/\e[0;34m<DEC>\e[0m/g;
  s/\x0f/\e[0;34m<\/DEC>\e[0m/g;

  s/[\x00-\x1a\x1c-\x1F\x80-\xFF]/"\e[0;34m<0x".sprintf("%02x",ord $&).">\e[0m"/ge;
  print "$_\n";
}


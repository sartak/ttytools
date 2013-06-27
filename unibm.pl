#!/usr/bin/perl
use strict;
use warnings;
use Term::TtyRec::Filter qw/filter_ttyrec/;

sub DEBUG { 1 }

my @translate = map {$_->[0] = qr/$_->[0]/; $_}
(
  [chr(176) => '#'], # corridor
  [chr(179) => '|'], # vertical wall
  [chr(191) => '-'], # ne corner
  [chr(192) => '-'], # sw corner
  [chr(196) => '-'], # horizontal wall
  [chr(217) => '-'], # se corner
  [chr(218) => '-'], # nw corner
  [chr(250) => '.'], # empty room
  [chr(254) => '-'], # open door
  [chr(244) => '}'], # pool of water
  [chr(177) => '#'], # lit corridor
  [chr(194) => '-'], # some wall junction
  [chr(180) => '|'], # some wall junction
  [chr(193) => '-'], # some wall junction
  [chr(195) => '|'], # some wall junction
  [chr(197) => '|'], # four-way junction
);

my %unk_encountered;

sub unibm
{
  my ($data_ref, $time_ref, $prev_ref) = @_;

  foreach (@translate)
  {
    $$data_ref =~ s/$_->[0]/$_->[1]/g;
  }

  if (DEBUG)
  {
    foreach (split //, $$data_ref)
    {
      if (ord($_) > 127 && !exists $unk_encountered{$_})
      {
        $unk_encountered{$_} = 1;
        warn "Unknown character encountered: '$&' == chr(".ord($&).")";
      }
    }
  }
}

for (@ARGV)
{
  filter_ttyrec(\&unibm, $_, "$_.unibm");
}

#!/usr/bin/perl
use strict;
use warnings;

die "usage: $0 file(s)\n" if @ARGV == 0;
my $handle;

FILE: foreach my $file (@ARGV)
{
  my $timestamp;
  my $frame = 0;

  open($handle, '<', $file) or die "unable to open $file: $!";
  
  FRAME: while ((my $hgot = read $handle, my $hdr, 12) > 0)
  {
    ++$frame;

    if ($hgot != 12)
    {
      warn "Unable to parse frame $frame of $file: expected 12-byte header, got $hgot.\n";
      next FILE;
    }

    my @hdr = unpack "VVV", $hdr;
    $timestamp = $hdr[0] + $hdr[1] / 1_000_000;

    my $dgot = read $handle, (my $data), $hdr[2];
    if ($dgot != $hdr[2])
    {
      warn "Unable to parse frame $frame of $file: expected $hdr[2]-byte frame, got $dgot.\n";
      next FILE;
    }
  }

  my ($seconds, $minutes, $hours, $day_of_month, $month, $year, 
      $wday, $yday, $isdst) = gmtime($timestamp);
  printf "%s%s (%02d:%02d:%02d GMT)\n", @ARGV > 1 ? "$file:" : "", $timestamp, $hours, $minutes, $seconds;
}
continue
{
  close $handle;
}

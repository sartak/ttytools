#!/usr/bin/perl
use strict;
use warnings;
use Term::TtyRec::Plus;

die "$0 infile framenum" if @ARGV < 2;
my $infile = shift;
my $framenum = shift;

my $ttp = Term::TtyRec::Plus->new(infile => $infile);
my $curframe = 0;
my $frame;

while (my $frame_data = $ttp->next_frame())
{
  ++$curframe;
  if ($curframe == $framenum)
  {
    $frame = $frame_data;
    last;
  }
}

die "Frame out of range (ttyrec ends at frame $curframe)" unless defined $frame;
#$frame->{data} =~ s/\e/\\e/g;
#$frame->{data} =~ s//\e[0;31m<\/DEC>\e[0m/g;
#$frame->{data} =~ s//\e[0;31m<DEC>\e[0m/g;
print $frame->{data};


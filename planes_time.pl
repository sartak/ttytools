#!/usr/bin/perl
use strict;
use warnings;
use Term::TtyRec::Plus;

my $start_planes = qr/But now thou must face the final Test/;
my $start_astral = qr/You activated a magic portal!/;
my $end_planes   = qr/You offer the Amulet of Yendor to /;

FILE: for my $file (@ARGV)
{
  my $ttp = Term::TtyRec::Plus->new(infile => $file);
  my ($turn, $dlvl, $time, $frame) = ('?', '?', 0, 0);
  my ($start_turn, $start_ast, $end_turn);

  FRAME: while (my $frame_ref = $ttp->next_frame())
  {
    $time += $frame_ref->{diff};
    ++$frame;

    if ($frame_ref->{data} =~ / T:(\d+)/)
    {
      $turn = $1;
    }

    if ($frame_ref->{data} =~ /(Dlvl:\d+|Home \d+|End Game|Astral Plane)/)
    {
      $dlvl = $1;
      $dlvl =~ s/://g;
    }

    if (not defined $start_turn)
    {
      if ($frame_ref->{data} =~ $start_planes)
      {
        $start_turn = $turn;
      }
      next FRAME;
    }

    if ($frame_ref->{data} =~ $start_astral and $dlvl ne 'Astral Plane')
    {
      $start_ast = $turn;
    }

    if ($frame_ref->{data} =~ $end_planes)
    {
      print "$file: Planes:$start_turn->$turn (".($turn-$start_turn).")";
      print "  Astral:$start_ast->$turn (".($turn-$start_ast).")\n";
      next FILE;
    }
  }
}


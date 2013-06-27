#!/usr/bin/perl
use strict;
use warnings;
use Term::TtyRec::Filter qw/filter_ttyrec/;

my $START_DEC = '';
my $END_DEC   = '';

my %undec =
(
  '`'    => '}',
  '~'    => '.',
  'a'    => '-',
  'j'    => '-',
  'k'    => '-',
  'l'    => '-',
  'm'    => '-',
  'n'    => '-',
  'o'    => '-',
  'q'    => '-',
  's'    => '-',
  't'    => '|',
  'u'    => '|',
  'v'    => '-',
  'w'    => '-',
  'x'    => '|',
  'y'    => '<',
  'z'    => '>',
  $START_DEC => '',
  $END_DEC   => '',
  chr(128) => '',
);

my $verbose = 0;
my $very_verbose = 0;

my $frame = 0;
my $dec_level = 0;

sub undec2
{
  my ($data_ref, $time_ref, $prev_ref) = @_;

  if ($very_verbose)
  {
    print "\b" x (length ($frame == 0 ? '' : $frame));
    ++$frame;
    print $frame;
  }
  else
  {
    ++$frame;
  }

  my @in = split //, $$data_ref;
  my @out;

  CHAR: while (@in)
  {
    my $char = shift @in;

    if ($char eq $START_DEC)
    {
      $dec_level = 1;
      next CHAR;
    }
    if ($char eq $END_DEC)
    {
      $dec_level = 0;
      next CHAR;
    }

    if ($dec_level == 0)
    {
      push @out, $char;
      next CHAR;
    }

    # now we know we're in DEC mode
    if ($char eq "\e")
    {
      # we don't apply DEC filtering to escape sequences
      push @out, $char;
      # TODO: handle an escape sequence being broken across frames
      while (@in && $in[0] =~ /[\[0-9;]/)
      {
        push @out, shift @in;
      }
      push @out, shift @in unless @in == 0;
      next CHAR;
    }
    push @out, exists $undec{$char} ? $undec{$char} : $char;
  }

  $$data_ref = join '', @out;
}

# this code isn't used, but it's the way it used to look
# it might be more impressive if it actually worked
my $keys = join '', keys %undec;
my $escape_code = qr/^(\e\[[\d;]*[$keys])/;
my $in_dec = 0;
sub undec
{
  my ($data_ref, $time_ref, $prev_ref) = @_;

  ++$frame;
  $$data_ref = "$$data_ref" if $in_dec;

  $$data_ref =~ 
    s{([^]+?)}
    {
      my $out = '';
      my @in = split //, $1;

      if ($in[-1] eq "")
      {
        pop @in;
      }
      else # frame ends with no </DEC>
      {
        $in_dec = 1;
      }

      for (my $idx = 0; $idx < @in; ++$idx)
      {
        local $_ = $in[$idx];
        if ($_ eq "\e")
        {
          my $str = join '', @in[$idx..$#in];
          if ($str =~ $escape_code)
          {
            $idx += length($1) - 1;
            $out .= $1;
            next;
          }
        }

        #      $out .= exists $undec{$_} ? $undec{$_} : die "Character (in frame $frame) not in \$undec{$_ => ord(".ord($_).")}";
        $out .= exists $undec{$_} ? $undec{$_} : $_;
      }
      $out;
    }eg;

  $$data_ref =~ s///g;
}

@ARGV = grep {$_ eq "-v" ? do { $verbose = 1; 0 } : 1}
        grep {$_ eq "-vv" ? do { $very_verbose = $verbose = 1; 0 } : 1}
        @ARGV;

$|++ if $very_verbose;
for (@ARGV)
{
  $frame = 0;
  $dec_level = 0;

  if ($verbose and not $very_verbose)
  {
    print "$_\n";
  }
  elsif ($very_verbose)
  {
    print "$_ - ";
  }

  filter_ttyrec(\&undec2, $_, "$_.undec");

  if ($very_verbose)
  {
    print "\b" x length($frame);
    print "ok\n";
  }
}

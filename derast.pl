#!/usr/bin/perl
use strict;
use warnings;
use Term::TtyRec::Filter qw/filter_ttyrec/;

my @default_objects = split //, '])[="(%!?+/$*`0_.';
my @rast_objects = qw{
168 251 091 061 034 123 000 000 000 000
000 000 000 048 048 046 046 
};
 
my @default_dungeon = split //, " |--------||.-|++##.##<><>_|\\#{}.}..## #}";
my @rast_dungeon = qw{
249 179 196 218 191 192 217 197 193 194
180 195 250 254 254 043 043 240 241 250
176 177 243 242 243 242 241 239 092 035
244 247 250 247 250 250 035 035 032 035
247
};

my @subs;
for my $el (0..$#rast_dungeon)
{
  push @subs, [chr($rast_dungeon[$el]), $default_dungeon[$el]];
}

#for my $el (0..$#rast_objects)
#{
#  push @subs, [chr($rast_objects[$el]), $default_objects[$el]];
#}

my $subs;

sub derast
{
  my ($data_ref, $time_ref, $prev_ref) = @_;
  foreach (@subs)
  {
    $subs += $$data_ref =~ s/\Q$_->[0]/$_->[1]/g;
  }
}

for (@ARGV)
{
  $subs = 0;
  filter_ttyrec(\&derast, $_, "$_.derast");
  print "$subs subs made in $_\n";
}

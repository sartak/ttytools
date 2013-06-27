#!/usr/bin/perl
use strict;
use warnings;
use WWW::Mechanize;
use LWP::Simple;

my $mech = new WWW::Mechanize;
my $dude = $ARGV[0];
$dude =~ y/a-zA-Z0-9//cd;
mkdir("$dude/");

foreach my $URL ("http://alt.org/nethack/oldttyrec/$dude", "http://alt.org/nethack/ttyrec/$dude")
{
  $mech->get($URL);
  next unless defined $mech->content;
  foreach my $link ($mech->links)
  {
    my $url = $URL . substr($link->url, 1);
    next unless $url =~ /\.ttyrec(?:\.bz2)?$/;
    print "Downloading $url\n";
    my $ttyrec = get($url);
    $url =~ s{.*/}{};
    open(TTYREC, '>', "$dude/$url") or die "Unable to open '$dude/$url': $!";
    binmode TTYREC;
    print TTYREC $ttyrec;
    close TTYREC;
  }
}


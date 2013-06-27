#!/usr/bin/perl
use strict;
use warnings;

# Takes ttyrec(s) on stdin, prints mail and responses to stdout.
# Invoke with: perl ttymail.pl a.ttyrec b.ttyrec etc.

my $sender = '';
my $msg = '';
my $resp = '';

$/ = "\e[";

while (<>)
{
  $sender = $1 if /This message is from '([^)]+)'\./;
  $msg = $1 if /It reads: "(.*)"\./;
  $resp = $1 if /[a-zA-Z](.*): unknown extended command\./;

  if ($sender && $msg)
  {
    print "$sender: $msg\n";
    $sender = $msg = '';
    next;
  }
  if ($resp)
  {
    print "(Response): $resp\n";
    $resp = '';
    next;
  }
}


#!/usr/bin/env perl

###
## @(#) nickprune.pl        falcon@icb.net        11/23/98
##
## find (and perhaps delete) all database entries for nicks past
## a certain age
###

use strict;
use warnings;

use Getopt::Std;
require "timelocal.pl";

$DELETE = "/usr/local/lib/icbd/deldb.pl";

%months =
(
    'Jan',    0,
    'Feb',    1,
    'Mar',    2,
    'Apr',    3,
    'May',    4,
    'Jun',    5,
    'Jul',    6,
    'Aug',    7,
    'Sep',    8,
    'Oct',    9,
    'Nov',    10,
    'Dec',    11,
);

$AGE = (60 * 60 * 24 * 365);    ## one year

$| = 1;

do getopts ('yhd:q');

if ( defined ($opt_h) )
{
    print <<_EOF_;
Usage: $0 [-yh] [-d days]
  -h       display this help screen
  -y       default to "yes" for queries "Delete this nick?"
  -d days  delete nicks > this many days old (default=365)
  -q       quiet mode; don't display deleted nicks
_EOF_
    exit (0);
}

if ( defined ($opt_d) )
{
    $AGE = (60 * 60 * 24 * $opt_d);
}

if ( !defined ($opt_q) )
{
    printf("Running with %d days expiry.\n", $AGE / (60 * 60 * 24));
}

dbmopen(my %DB, "./icbdb", 0666);

while (my ($key, $val) = each (%DB) )
{
    if ( $key =~ /^(.+).nick$/ )
    {
        my $rootkey = $1;
        my $so = $rootkey. ".signoff";

        if ( $DB{$key} eq "server" || $DB{$key} eq "admin" )
        {
            next;
        }

        ## if there is no signoff, try signon
        if ( $DB{$so} eq "" )
        {
            $so = $rootkey . ".signon";
        }

        # signoff:  6-Jan-1997 21:27 PST
        if ( $DB{$so} =~ /\s*(\d+)-([^-]+)-(\d+) .*/ )
        {
            my $then = &timelocal (0, 0, 0, $1, $months{$2}, $3);
            my $now = time;

            if ( $now - $then > $AGE )
            {
                if ( !defined ($opt_q) )
                {
                    printf("%s is %d days old (%s)\n",
                        $DB{$key},
                        ($now - $then) / ( 60 * 60 * 24),
                        $DB{$so});
                }

                if ( !defined ($opt_y) )
                {
                    printf("Delete %s? [n] ", $rootkey);
                    my $yn = <STDIN>;
                    chop ($yn);
                    if ( $yn eq "" ) { $yn = "no"; }
                    if ( $yn =~ /^[qQ]/ ) { last; }
                    if ( $yn !~ /^[yY]/ ) { next; }
                }

                if ( ! defined ($opt_q) )
                {
                    printf("  deleting....");
                }

                if ( ($pid = fork()) == 0 )
                {
                    if ( defined ($opt_q) )
                    {
                        exec $DELETE, "-q", '--', $rootkey;
                    }
                    else
                    {
                        exec $DELETE, '--', $rootkey;
                    }

                    printf("Failed to exec %s\n", $DELETE);
                    exit 1;
                }
                elsif ( $pid > 0 )
                {
                    wait;
                }
            }
        }
    }
}

dbmclose(%DB);

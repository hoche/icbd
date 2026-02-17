#!/usr/bin/env perl

###
## @(#) deldb.pl        falcon@icb.net        2/1/98
##
## delete nick entries from an icbd database
###

use strict;
use warnings;

use Getopt::Std;

@suffi = (
    'addr',
    'email',
    'home',
    'nick',
    'nummsg',
    'password',
    'phone',
    'realname',
    'secure',
    'signoff',
    'signon',
    'text',
    'www',
);

do getopts ('q');

dbmopen(my %DB, "./icbdb", 0666);

foreach $nick (@ARGV)
{
    my $num = 0;

    my $k = "${nick}.nick";
    if ( $DB{$k} eq "" )
    {
        my $nick =~ tr/A-Z/a-z/;
        $k = "${nick}.nick";
        printf("Trying with lower-cased nick %s\n", $nick);
    }

    if ( $DB{$k} !~ /^$nick$/i )
    {
        printf("Warning: Nick %s doesn't match %s (%s).\n",
            $nick, "\$DB{$k}", $DB{$k});
    #    next;
    }

    $k = "${nick}.nummsg";
    if ( $DB{$k} > 0 )
        $num = $DB{$k};
    }

    foreach $s (@suffi)
    {
        my $k2 = "${nick}.${s}";
        delete $DB{$k2};
    }

    if ( $num > 0 )
    {
        local($i);

        for ( $i = 1; $i <= $num; $i++ )
        {
            my $h = "${nick}.header${i}";
            my $f = "${nick}.from${i}";
            my $m = "${nick}.message${i}";

            delete $DB{$h};
            delete $DB{$f};
            delete $DB{$m};
        }
    }

    if ( !defined ($opt_q) )
    {
        printf("%s deleted.\n", $nick);
    }
}

dbmclose(%DB);

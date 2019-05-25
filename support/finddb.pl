#!/usr/bin/env perl

###
## @(#) finddb.pl  falcon@icb.net  2/1/98
## Updated by hoche@grok.com       5/24/19
##
## find all related database fields for a nickname
###

use strict;
use warnings;

my @suffi = (
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

dbmopen(my %DB, "./icbdb", 0666);

#foreach $i (@ARGV) {
#    print "$i = \"$DB{$i}\"\n";
#}

foreach my $nick (@ARGV)
{
    my $k = "${nick}.nick";
    if ( !defined ($DB{$k}) )
    {
        print "$nick not registered.\n";
        next;
    }

    if ( $DB{$k} !~ /^$nick$/i )
    {
        printf "Warning: Nick $nick doesn't match \$DB{$k} ($DB{$k}).\n";
        next;
    }

    printf "  %10s %s\n", "NICKNAME:", $nick;
    foreach my $s (@suffi)
    {
        my $k2 = "${nick}.${s}";
        printf "  %10s %s\n",
            "$s:", $DB{$k2};
    }

    $k = "${nick}.nummsg";
    if ( $DB{$k} > 0 )
    {
        my $num = $DB{$k};
        for (my $i = 1; $i <= $num; $i++ )
        {
            my $h = "${nick}.header${i}";
            my $f = "${nick}.from${i}";
            my $m = "${nick}.message${i}";

            printf "  %10s %s\n", "message:", $i; 
            printf "  %10s $DB{$h}\n", "";
            printf "  %10s $DB{$f}\n", "";
            printf "  %10s $DB{$m}\n", "";
        }
    }

    print "\n";
}

dbmclose(%DB);

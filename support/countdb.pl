#!/usr/bin/env perl

###
## @(#) countdb		falcon@icb.net
## Updated by hoche@grok.com       5/24/19
##
## count how many nicks there are. though effective, this is slow & inefficient
###

$num = 0;

dbmopen(my %DB, "./icbdb", 0666);
while (my ($key, $val) = each(%DB) )
{
    if ( $key =~ /^(.+).nick$/ )
    {
        $num++;
    }
}

printf("%d nicknames listed.\n", $num);

dbmclose(%DB);

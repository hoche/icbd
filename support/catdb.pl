#!/usr/bin/env perl

###
## @(#) catdb.pl    falcon@icb.net  2/1/98
## Updated by hoche@grok.com       5/24/19
##
## just cat the entire damn database to stdout
###

dbmopen(my %DB, "./icbdb", 0666);

while (my ($key,$val) = each %DB) {
    print $key, ' = ', $val, "\n";
}

dbmclose(%DB);

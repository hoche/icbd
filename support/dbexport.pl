#!/usr/bin/env perl

###
## @(#) dbexport		falcon@icb.net
## Updated by hoche@grok.com       5/24/19
##
## this exports the database in a format that can be used by
## dbimport. it's not hammered on since it's only been used once.
## primary use is when migrating from one machine to another which
## has a different binary format for its dbm files so you can't just
## copy them over.
###

use strict;
use warnings;

dbmopen(my %DB, "./icbdb", 0666);

while (my ($key,$val) = each(%DB)) {
    printf("%s|%s\n", $key, $val);
}

dbmclose(%DB);

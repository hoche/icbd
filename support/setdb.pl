#!/usr/bin/env perl

###
## @(#) setdb		falcon@icb.net
## Updated by hoche@grok.com       5/24/19
##
## assign a specific database value. useful for resetting passwords for users
## who forget them.
### 

use strict;
use warnings;

dbmopen(my %DB, "./icbdb", 0666);

my ($var, $val) = (@ARGV);
printf("VAR[%s] VAL[%s]\n", $var, $val);
$DB{$var} = $val;
dbmclose(%DB);

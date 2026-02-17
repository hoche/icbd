#!/usr/bin/env perl

###
## @(#) setdb		falcon@icb.net
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

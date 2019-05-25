#!/usr/bin/env perl

###
## @(#) dbimport		falcon@icb.net
## Updated by hoche@grok.com       5/24/19
##
## this builds a database by importing data created by dbexport
## it's not hammered on since it's only been used once
###

dbmopen(my %DB, "./icbdb", 0666);

while (my $line = <STDIN>)
{
	if ( $line =~ /([^|]+)\|(.*)/ )
	{
		printf("%s=%s\n", $1, $2);
		$DB{$1} = $2;
	}
}

dbmclose(%DB);

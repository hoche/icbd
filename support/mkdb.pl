#!/usr/bin/env perl

###
## @(#) mkdb.pl    falcon@icb.net  2/1/98
## Updated by hoche@grok.com       5/24/19
##
## initialize the user database with the required "server" nickname
###

use strict;
use warnings;

sub promptUser {
   my ($promptString, $defaultValue) = @_;

   if ($defaultValue) {
      print $promptString, " [", $defaultValue, "]: ";
   } else {
      print $promptString, ": ";
   }

   $| = 1; # force flush
   $_ = <STDIN>;

   chomp;

   if ("$defaultValue") {
      return $_ ? $_ : $defaultValue;
   } else {
      return $_;
   }
}

my $REALNAME = &promptUser("The server admin's name", "Server Admin");
my $HOME = &promptUser("The server address", "icb\@yourdomain.com");
my $EMAIL = &promptUser("The server admin's email", "icbadmin\@yourdomain.com");

dbmopen(my %DB, "./icbdb", 0666);
$DB{ "server.realname"} = $REALNAME;
$DB{ "server.nick"} = "server";
$DB{ "server.home"} = $HOME;
$DB{ "server.email"} = $EMAIL;
$DB{ "server.text"} = "Here to server you!";
$DB{ "server.nummsg"} = "0";
$DB{ "server.www"} = "http://www.icb.net/";

dbmclose(%DB);

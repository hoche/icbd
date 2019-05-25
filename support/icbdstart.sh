#!/bin/sh

###
## @(#) icbdstart		falcon@icb.net
##
## start up icbd. typically run from system startup rc scripts as root
###

###
## set the following to suit your local configuration. two examples
## are listed which you can choose from, or make your own
###

## set to the user you'd like icbd to run as. this is assuming you
## are running this script from your system startup as root
#USER="icbadmin"
USER="nobody"

## this is for a server running on just one of many interfaces available
#DIR="/usr/local/lib/icbd/chime"
#ARGS="-b chime.icb.net"

## this is for a server that binds to all available ports. if the server
## has been defined with BIND_HOSTNAME, it will only bind to that one, however
DIR="/usr/local/lib/icbd"
ARGS=""

#
# start up icbd
#
if [ -x "$DIR/icbd" ]
then
    echo -n " icbd"
    su $USER -c "(unlimit descriptors; cd $DIR; ./icbd $ARGS)"
fi

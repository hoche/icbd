#!/bin/sh

###
## @(#) icbd.sh
##
## start up icbd. typically run from system startup rc scripts as root
## 
## Originally written for sunos, then freebsd.
##
## For linux with systemd, use the icbd.server script
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


NAME="icbd"
PROG="/usr/local/bin/icbd"
PIDFILE="${DIR}/icbd.pid"


CMD=${1:-start}

case ${CMD} in
start)
    if [ -x ${PROG} ]; then
        if [ -x "${DIR}/icbd" ]
            echo "Starting ${NAME}."
            su ${USER} -c "(unlimit descriptors; cd ${DIR}; ${PROG} ${ARGS})"
        fi
    fi
    ;;
stop)
    if [ -x ${PROG} -a -f ${PIDFILE} ]; then
        echo "Stopping ${NAME}."
        kill `cat ${PIDFILE}`
    fi
    ;;
restart)
    ( $0 stop )
    sleep 5
    $0 start
    ;;
*)
    echo "usage: $0 start|stop|restart"
esac
exit 0


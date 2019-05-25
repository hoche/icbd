#!/bin/sh

###
## @(#) mkcert		hoche@grok.com
##
## make a cert with openssl
###

openssl req -new -x509 -nodes -out icbd.pem -keyout icbd.pem -days 365
#openssl req -new -x509 -nodes -out icbd.pem -keyout icbd.pem -config ./openssl.cnf -days 365

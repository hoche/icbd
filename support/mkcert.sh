#!/bin/sh

###
## @(#) mkcert		hoche@grok.com
##
## make a cert with openssl
###

openssl req -newkey rsa:2048 -new -x509 -nodes -days 365 -out icbd.pem -keyout icbd.pem

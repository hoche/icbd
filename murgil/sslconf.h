
/* Copyright (c) 2001 Michel Hoche-Mong, All rights reserved. */


#ifdef HAVE_SSL

#ifndef SSLCONF_H
#define SSLCONF_H

#include <openssl/ssl.h>

int create_ssl_context(char *pem, SSL_CTX **ctx);

#endif /* SSLCONF_H */

#endif /* HAVE_SSL */


/* Copyright (c) 2001 Michel Hoche-Mong, All rights reserved. */

#pragma once

#include "../config.h"

#ifdef HAVE_SSL

#include <openssl/ssl.h>

int create_ssl_context(char *pem, SSL_CTX **ctx);

#endif /* HAVE_SSL */

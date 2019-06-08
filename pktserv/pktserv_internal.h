/*
 * globals.h
 *
 * globals for SSL-enabled select-based server.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong.
 * All rights reserved.
 *
 * Based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 */

#pragma once

#ifdef PKTSERV_INTERNAL

#include "../config.h"

#include <sys/types.h>
#include <sys/time.h> /* for struct timeval and struct itimerval */

#include "pktbuffers.h"

extern cbuf_t *cbufs;        /* user packet buffers. */

#ifdef HAVE_SSL
#include <openssl/ssl.h>
extern SSL_CTX *ctx;
#endif


int add_pollfd(int fd);
void delete_pollfd(int fd);
int _newconnect(int s, int is_ssl);
int _readpacket(cbuf_t *cbuf);
int _writepacket(cbuf_t* cbuf);


#endif /* PKTSERV_INTERNAL */



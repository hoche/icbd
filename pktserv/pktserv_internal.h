/*
 * pktserv_internal.h
 *
 * internal globals for SSL-enabled poll-based network server.
 *
 * Author: Michel Hoche-Mong
 * Copyright (c) 2001-2026 Michel Hoche-Mong
 * All rights reserved.
 *
 * Based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 */

#pragma once

#ifdef PKTSERV_INTERNAL

#include "config.h"

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



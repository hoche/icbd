/*
 * globals.h
 *
 * globals for SSL-enabled select-based server.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong and jon r. luini
 *
 * Released under the GPL license.
 *
 * Based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 *
 * $Id: globals.h,v 1.7 2001/08/04 11:03:06 hoche Exp $
 *
 */

#pragma once

#include "../config.h"

#include <sys/types.h>
#include <sys/time.h> /* for struct timeval and struct itimerval */

#include "cbuf.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

extern struct cbuf_t cbufs[MAX_USERS];		/* user packet buffers (YECCH!) */

extern fd_set rfdset;			/* fd set to poll for reads */
extern fd_set wfdset;			/* fds to poll for writes */
extern fd_set ignorefdset;		/* fds that are not to be polled */

extern int port_fd;			/* plaintext listen port */
#ifdef HAVE_SSL
extern int sslport;			/* ssl listen port */
extern int sslport_fd;			/* ssl listen port's file descriptor */
extern SSL_CTX *ctx;			/* SSL context object */
#endif

extern int highestfd;

/* timeout value in usec for select() */
extern struct timeval *polltimeout;
extern struct itimerval *polldelay;


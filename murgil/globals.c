/*
 * globals.c
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
 * $Id: globals.c,v 1.10 2001/08/04 11:03:06 hoche Exp $
 *
 */


#include "config.h"

#include <sys/types.h>

#include "cbuf.h"
#include "globals.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif


struct cbuf_t cbufs[MAX_USERS];	/* array of user packet buffers */

fd_set rfdset;			/* fd set to poll for reads */
fd_set wfdset;			/* fd set to poll for writes */
fd_set ignorefdset;		/* fds that are not to be polled */

int port_fd;			/* plaintext listen port */
#ifdef HAVE_SSL
int sslport;			/* ssl listen port */
int sslport_fd;         	/* ssl listen port's file descriptor  */
SSL_CTX *ctx;
#endif
int highestfd = 0;

/* timeout value in usec for select() */
struct timeval *polltimeout = (struct timeval *)0;
struct itimerval *polldelay = (struct itimerval *)0;


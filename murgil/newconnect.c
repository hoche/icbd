/*
 * serverserve.c
 *
 * main event loop for an SSL-enabled select-based server.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong and jon r. luini
 *
 * Released under the GPL license.
 *
 * Based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 *
 * $Id: newconnect.c,v 1.11 2001/08/04 11:03:06 hoche Exp $
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#ifdef HAVE_SSL
#include <openssl/err.h>
#include <openssl/ssl.h>
#include "sslconf.h"
#endif

#include "globals.h"
#include "server/mdb.h"

/* accept and initialize a new client connection */

/* s is the listen socket
 * is_ssl indicates whether this is the plaintext listen socket or the
 *        SSL one.
 * returns:
 * the resultant socket file descriptor or
 *  0 if the operation would block or
 * -1 if the operation failed
 */
int _newconnect(int s, int is_ssl)
{
  int ns;  /* new socket - the socket of the accepted client */
  int one = 1;
  int flags;
  struct linger nolinger;
  struct cbuf_t *cbuf;

  /* point to our socket's cbuf. note that if this is a new connect,
   * this will point to the listen socket's cbuf. however, the 
   * want_ssl_accept flag should never be set on that, so we should
   * just fall through to the plain accept() and we'll set the cbuf
   * pointer to point to the new client's socket below.
   */
  cbuf = &cbufs[s];

  if (!cbuf->want_ssl_accept) {

    /* accept the connection */
    if ((ns = accept(s, (struct sockaddr *) NULL, NULL)) < 0) {
      if (errno == EWOULDBLOCK)
	return(0);
      else {
	vmdb(MSG_WARN, "_newconnect::accept() - %s", strerror(errno));
	return(-1);
      }
    }

    /* ok, got a new socket. point the cbuf to the new one's */
    cbuf = &cbufs[ns];

    /* force occasional connection check */
    if (setsockopt(ns, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, sizeof(one)) < 0)
    {
      vmdb(MSG_WARN, "_newconnect::setsockopt(SO_KEEPALIVE) - %s", 
	  strerror(errno));
    }

    /* disable linger - close() returns immediately and the kernel
     * handles graceful shutdown in the background. previously this
     * was set to l_onoff=1, l_linger=0 which caused close() to send
     * a RST instead of a FIN, discarding any data still in flight
     * (including final error/exit messages to the client).
     */
    nolinger.l_onoff = 0;
    nolinger.l_linger = 0;
    if (setsockopt(ns, SOL_SOCKET, SO_LINGER,
      (char *)&nolinger, sizeof(nolinger)) < 0) {
	vmdb(MSG_WARN, "_newconnect::setsockopt(SO_LINGER) - %s", 
	    strerror(errno));
    }

    /* XXX this shouldn't be necessary anymore, since we have send queues
     */
    one = 24576;
    if (setsockopt(ns, SOL_SOCKET, SO_SNDBUF, (char *)&one, sizeof(one)) < 0) {
	vmdb(MSG_WARN, "_newconnect::setsockopt(SO_SNDBUF) - %s", 
	    strerror(errno));
    }

    /* make the socket non-blocking */
    if (fcntl(ns, F_SETFL, FNDELAY) < 0) {
	vmdb(MSG_WARN, "_newconnect::fcntl(FNDELAY) - %s", strerror(errno));
	return(-1);
    }

    /* Don't close on exec */
    flags = fcntl(ns, F_GETFD, 0);
    flags = flags & ~ FD_CLOEXEC;
    fcntl(ns, F_SETFD, flags);

    /* clear out the socket's cbuf */
    memset(cbuf, 0, sizeof(struct cbuf_t));

    /* we're starting with a new command */
    cbuf->fd = ns;
    cbuf->newmsg = 1;
    TAILQ_INIT(&(cbuf->wlist));

  }


#ifdef HAVE_SSL
  if (is_ssl) {
    int result, ssl_error;

    /* don't try to reinitialize if we're in the middle of accepting already */
    if (!cbuf->want_ssl_accept) {
      if ( (cbuf->ssl_con = SSL_new(ctx)) == NULL) {
	  vmdb(MSG_ERR, "_newconnect: fs#%d: could't create ssl_con.");
	  return -1;
      }
      SSL_set_fd(cbuf->ssl_con, ns);
    }

    result = SSL_accept(cbuf->ssl_con);
    if (result <= 0) {
      ssl_error = SSL_get_error(cbuf->ssl_con, result);
      switch (ssl_error) {

	case SSL_ERROR_WANT_READ:
	  vmdb(MSG_INFO, "SSL_accept on fd%d: SSL_ERROR_WANT_READ", ns);
	  cbuf->want_ssl_accept = 1;
	  FD_SET(ns, &wfdset); /* add to wfdset. we add to rfdset below */
	  break;

	case SSL_ERROR_WANT_WRITE:
	  vmdb(MSG_INFO, "SSL_accept on fd%d: SSL_ERROR_WANT_WRITE", ns);
	  cbuf->want_ssl_accept = 1;
	  FD_SET(ns, &wfdset); /* add to wfdset. we add to rfdset below */
	  break;

	case SSL_ERROR_ZERO_RETURN:
	  vmdb(MSG_ERR, "fs#%d: ssl conn marked closed on SSL_accept", ns);
	  SSL_free(cbuf->ssl_con);
	  return -1;

	case SSL_ERROR_WANT_X509_LOOKUP:
	  vmdb(MSG_ERR, "fs#%d: ssl conn wants X509 lookup.", ns);
	  SSL_free(cbuf->ssl_con);
	  return -1;

	case SSL_ERROR_SYSCALL:
	  if (result == 0) {
	    vmdb(MSG_ERR, "SSL_accept on fs%d: SSL_ERROR_SYSCALL: got EOF", ns);
	  } else {
	    vmdb(MSG_ERR, "SSL_accept on fs%d: SSL_ERROR_SYSCALL: %s", ns,
	      strerror(errno));
	  }
	  SSL_free(cbuf->ssl_con);
	  return -1;

	case SSL_ERROR_SSL:
	  ssl_error = ERR_get_error();
	  if (ssl_error == 0) {
	    if (result == 0) {
	      vmdb(MSG_ERR, "fs#%d, SSL_accept got bad EOF", ns);
	    } else {
	      vmdb(MSG_ERR, "fs#%d, SSL_accept got socket I/O error", ns);
	    }
	  } else {
	    char err_buf[256];
	    ERR_error_string(ssl_error, &err_buf[0]);
	    vmdb(MSG_ERR, "fs#%d, SSL_accept error: %s", ns, err_buf);
	  }
	  SSL_free(cbuf->ssl_con);
	  return -1;
      }
    } else {
      vmdb(MSG_INFO, "fs#%d: SSL connection accepted.", ns);
      cbuf->want_ssl_accept = 0;
      FD_CLR(ns, &wfdset);

      /* XXX -- do X509 validation stuff here */
    }

  }
#endif

  /* add to the read set */
  FD_SET(ns, &rfdset);

  if (ns > highestfd)
    highestfd = ns;

  return(ns);
}

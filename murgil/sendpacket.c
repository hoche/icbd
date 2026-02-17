/*
 * writepacket.c
 *
 * packet writing routines
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong and jon r. luini
 *
 * Released under the GPL license.
 *
 * $Id: sendpacket.c,v 1.11 2002/03/17 01:03:00 keithr Exp $
 *
 */


#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>

#include "cbuf.h"
#include "globals.h"
#include "murgil.h" /* for disconnect() */
#include "../server/mdb.h"

#ifdef HAVE_SSL 
#include <openssl/ssl.h>
#include <openssl/err.h>
#endif

/*
 * write a packet to socket file descriptor 
 *
 * returns -1 on connection gone, too many retries have been attempted,
 *    or there was some other serious problem.
 * returns 0 if write or partial write succeeded.
 *
 * Note: this currently tries to write all an fd's packets at once.
 *   it could just as easily write one packet, and wait until the
 *   next time through the select loop to come back here and write
 *   more.
 */
int _writepacket(struct cbuf_t* cbuf)
{
  int result;
  size_t remain;
  struct msgbuf_t *msgbuf;

  vmdb(MSG_VERBOSE, "writepacket: fd%d has %d packets in queue.", 
    cbuf->fd, cbuf->wlist_size);

  if (cbuf->retries > MAX_SENDPACKET_RETRIES) {
     return -1;
  }

  while (!TAILQ_EMPTY(&(cbuf->wlist))) {
    msgbuf = TAILQ_FIRST(&(cbuf->wlist));

    remain = msgbuf->len - (msgbuf->pos - msgbuf->data);

    vmdb(MSG_VERBOSE, "writepacket: fd%d sending %d bytes....", 
      cbuf->fd, remain);

#ifdef HAVE_SSL
    if (cbuf->ssl_con == NULL) {
      result = write(cbuf->fd, msgbuf->pos, remain);
    } else {
      /* if it's SSL, we have to keep calling it with the whole buffer.
       * not just the remaining part.
       */
      result = SSL_write(cbuf->ssl_con, msgbuf->data, msgbuf->len);
      if (result <= 0) {
	int ssl_error;
	ssl_error = SSL_get_error(cbuf->ssl_con, result);
	switch (ssl_error) {
	  case SSL_ERROR_WANT_READ:
	    vmdb(MSG_INFO, "_writepacket: fd%d: SSL_ERROR_WANT_READ", cbuf->fd);
	    result = 0; /* retry the write later */
	    break;

	  case SSL_ERROR_WANT_WRITE:
	    vmdb(MSG_INFO, "_writepacket: fd%d: SSL_ERROR_WANT_WRITE", cbuf->fd);
	    result = 0; /* retry the write later */
	    break;

	  case SSL_ERROR_ZERO_RETURN:
	    vmdb(MSG_ERR, 
	      "_writepacket: fd%d: SSL conn maked closed", cbuf->fd);
	    return -1;

	  case SSL_ERROR_WANT_X509_LOOKUP:
	    vmdb(MSG_ERR, 
	      "_writepacket: fd%d: SSL conn wants X509 lookup.", cbuf->fd);
	    return -1;

	  case SSL_ERROR_SYSCALL:
	    vmdb(MSG_ERR, 
	      "_writepacket: fd%d: SSL_ERROR_SYSCALL", cbuf->fd);
	    return -1;

	  case SSL_ERROR_SSL:
	    ssl_error = ERR_get_error();
	    if (ssl_error == 0) {
	      if (result == 0) {
		  vmdb(MSG_ERR, 
		    "_writepacket: fd%d, SSL_write got EOF", cbuf->fd);
	      } else {
		  vmdb(MSG_ERR, 
		    "_writepacket: fd%d, SSL_write got I/O error", cbuf->fd);
	      }
	    } else {
	      char err_buf[256];
	      ERR_error_string(ssl_error, &err_buf[0]);
	      vmdb(MSG_ERR, 
		"_writepacket: fd%d, SSL conn error: %s", cbuf->fd, err_buf);
	    }
	    return -1;
	}
      }
    }
#else
    result = write(cbuf->fd, msgbuf->pos, remain);
#endif

    if ( result < 0 ) {
      if (errno != EWOULDBLOCK) {
	vmdb(MSG_ERR, 
	  "fd%d: Couldn't write packet: %s", cbuf->fd, strerror(errno));
	return -1;
      }
      FD_SET(cbuf->fd, &wfdset); /* try again later with the full amount */
      vmdb(MSG_VERBOSE, 
	"writepacket: fd%d got EWOULDBLOCK. will retry sending packet later.",
        cbuf->fd);
      cbuf->retries++;
      return 0;
    }
    
    if (result < remain) {
      /* try again later with the remaining amount */
      msgbuf->pos += result;
      FD_SET(cbuf->fd, &wfdset);
      vmdb(MSG_VERBOSE, 
	"writepacket: fd%d sent partial packet (%d bytes). will retry sending later.",
        cbuf->fd, result);
      cbuf->retries++;
      return 0;
    } 
    

    /* SUCCESS! pop the msgbuf off the send stack and delete it */
    TAILQ_REMOVE(&(cbuf->wlist), msgbuf, entries);
    cbuf->wlist_size--;

    free(msgbuf->data);
    free(msgbuf);

    /* also reset the retries */
    cbuf->retries = 0;

    vmdb(MSG_VERBOSE, "writepacket: fd%d sent packet. queue is now %d.",
      cbuf->fd, cbuf->wlist_size);
  }

  /* we've sent everything, so let's remove this fd */
  FD_CLR(cbuf->fd, &wfdset);
  return 0;
}


/* public callback to send a packet to a client. adds a packet to a client's
 * list of packets that need to be sent, and then tries to call _sendpacket
 * to send 'em.
 *
 * if the client already has MAX_SENDPACKET_QUEUE packets waiting to go,
 * return -1.
 *
 */
int sendpacket(int s, char *pkt, size_t len)
{
    struct cbuf_t *cbuf;
    struct msgbuf_t *msgbuf;

    cbuf = &(cbufs[s]);

    vmdb(MSG_VERBOSE, "sendpacket: len=%d, pktlen=%d, pkt=\"%s\"", 
      len, (unsigned char)*pkt, pkt+1);

    /* if we already have too many unwritten messages, try to drain first */
    if (cbuf->wlist_size >= MAX_SENDPACKET_QUEUE) {
	if (_writepacket(cbuf) < 0) {
	    vmdb(MSG_WARN,
	      "sendpacket: fd%d, error draining queue. disconnecting user.", s);
	    disconnectuser(s);
	    return -1;
	}
	/* if still full after draining, drop the message */
	if (cbuf->wlist_size >= MAX_SENDPACKET_QUEUE) {
	    vmdb(MSG_ERR,
	      "sendpacket: fd%d already has %d pending writes.", s, cbuf->wlist_size);
	    return -1;
	}
    }

    /* allocate a write buffer */
    msgbuf = _alloc_msgbuf(NULL, len);
    if (!msgbuf)
      return -1;

    memcpy(msgbuf->data, pkt, len);
    msgbuf->len = len;
    msgbuf->pos = msgbuf->data;

    /*
     * Do not remove the braces around these if and else bodies; the
     * linux TAILQ macros choke gcc without them.
     */
    if (TAILQ_EMPTY(&(cbuf->wlist))) {
      TAILQ_INSERT_HEAD(&(cbuf->wlist), msgbuf, entries);
    } else {
      TAILQ_INSERT_TAIL(&(cbuf->wlist), msgbuf, entries);
    }
    cbuf->wlist_size++;

    /* try a write */
    if (_writepacket(cbuf) < 0) {
	vmdb(MSG_WARN, 
	  "sendpacket: fd%d, error sending packet. disconnecting user.", s);
	disconnectuser(s);
	return -1;
    }

    return 0;
}

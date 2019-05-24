/*
 * cbuf.h
 *
 * message and client buffers for a generic select-based server
 *
 * Original Author: Michel Hoche-Mong
 * Copyright (c) 2001, Michel Hoche-Mong, hoche@grok.com
 *
 * Released under the GPL license.
 *
 * $Id: cbuf.h,v 1.6 2002/03/17 01:02:10 keithr Exp $
 *
 */


#pragma once

#include "../config.h"

#include <sys/queue.h>

/*
 * The linux queue.h doesn't define the macros for accessing the
 * queue; just for building it.  Luckily, these (the BSD macros)
 * work (at least in the current version) because the field names
 * are all the same.
 */
#ifndef TAILQ_FIRST
# define	TAILQ_FIRST(head) ((head)->tqh_first)
#endif

#ifndef TAILQ_EMPTY
# define	TAILQ_EMPTY(head) ((head)->tqh_first == NULL)
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

struct msgbuf_t;


/* msg buffer */
struct msgbuf_t {
  TAILQ_ENTRY(msgbuf_t)	entries;  /* message buffer list entries */

  size_t sz;		/* allocated size of the data buffer */

  size_t len;		/* length of the data in the buffer */
  char *pos;		/* ptr to the current read/write position */

  char *data; 		/* data buffer */
};



/* client buffer */
struct cbuf_t {
  int fd;		/* the client's file descriptor */

  /* read buffer */
  struct msgbuf_t *rbuf;	/* read buffer. just one of these */
  char newmsg;		/* set to TRUE if next read is start of new packet */
			/*        FALSE if a packet is partially read */

  /* write buffers */
  TAILQ_HEAD(mblisthead, msgbuf_t) wlist; /* write list */
  int wlist_size;	/* number of pending packets in list */
  int retries;		/* number of send retries attempted */

#ifdef HAVE_SSL
  SSL *ssl_con;		/* this will be NULL if it's a cleartext client */
#endif

  /* the following should NEVER be set on the listen socket(s) */
  int started_ssl_accept;	/* started but didn't finish an SSL_accept() */
  int want_ssl_read;	/* need to retry an SSL_read() */
  /* we don't need a want_ssl_write flag because our writes will
   * automatically retry themselves */
};


struct msgbuf_t *_alloc_msgbuf(struct msgbuf_t *msgbuf, size_t sz);


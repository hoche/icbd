/*
 * readpacket.c
 *
 * Internal packet-reading routine
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong and jon r. luini
 *
 * Released under the GPL license.
 *
 * Loosely based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 *
 * $Id: readpacket.c,v 1.13 2001/08/04 08:51:30 hoche Exp $
 *
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#include "cbuf.h"
#include "globals.h"
#include "murgil.h"
#include "../server/mdb.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#define PACKET_HEADER_LEN	1

/* header validation macro. right now it just returns TRUE. in the 
 * future, it might look for some required signature.
 */
#define VALID_PACKET_HEADER(hdr) 	1
	
/* read a packet of data into a cbuf's rbuf
 *
 * return -2 if read failed because user disconnected (EOF)
 * return -1 if read failed because of some error
 * return  0 if packet is still incomplete
 * return  1 if packet is complete
 */
int _readpacket(int user, struct cbuf_t *p)
{
  register ssize_t ret;
  size_t remain;

  if (p->newmsg) {

    /* starting a new command */


    /* set up our packet buffer */
    if ( (p->rbuf = _alloc_msgbuf(p->rbuf, USER_BUF_SIZE)) == NULL) {
	return -1;
    }

    /* read the packet header */
#ifdef HAVE_SSL
    if (p->ssl_con == NULL) {
      ret = read(user, p->rbuf->data, PACKET_HEADER_LEN);
    } else {
      ret = SSL_read(cbufs[user].ssl_con, p->rbuf->data, PACKET_HEADER_LEN);
    }
#else
    ret = read(user, p->rbuf->data, PACKET_HEADER_LEN);
#endif
    if (ret < 0) {
      /* we haven't gotten all the data they said they
       * were gonna send (incomplete packet)
       */
      if (errno == EWOULDBLOCK) {
	return(0);
      } else if ( errno == EPIPE || errno == ECONNRESET ) {
	return (-2);
      } else {
#ifdef HAVE_SSL
	if (p->ssl_con == NULL) {
	  vmdb(MSG_ERR, "fd%d: Couldn't read command packet.", user);
	} else {
	  vmdb(MSG_ERR, "fd%d (SSL): Couldn't read command packet.", user);
	}
#else
	vmdb(MSG_ERR, "fd%d: Couldn't read command packet.", user);
#endif
	return(-1);
      }
    }

    if (!ret) {
      /* EOF (closed connection) */
      return (-2);
    }

    /* check to see if it's a valid packet header */
    if (!VALID_PACKET_HEADER(p->rbuf->data)) {
      return (-2);
    }

    /* set our need-to-read size and scootch the pointer up 
     * past the length byte 
     */
    p->rbuf->len = (unsigned char)(*(p->rbuf->data)) + 1;
    p->rbuf->pos++;
    p->newmsg = 0;

    /*
     * this is in case you've configured the server with
     * the cbuf buffer size less than the maximum allowed
     * by the size byte (255) in the protocol. not normal,
     * but it's possible. the -1 is for the null we want to
     * tack on at the end.
     */
    if ( p->rbuf->len > (USER_BUF_SIZE-1) )
    {
      vmdb(MSG_ERR, 
          "fd#%d: sent oversized packet (%d>%d); terminating connection...",
	  user, (size_t)(p->rbuf->len), (USER_BUF_SIZE-1));
      /*
       * note: this pops up to sdoinput() which calls
       * disconnect() when we return -2. it's a bit
       * ugly, but somewhat consistent and i'm in a rush
       * to fix this bug
       */
      return (-2);
    }
  }

  /* read as much of the command as we can get */
  remain = p->rbuf->len - (p->rbuf->pos - p->rbuf->data);
#ifdef HAVE_SSL
  if (p->ssl_con == NULL) {
    ret = read(user, p->rbuf->pos, remain);
  } else {
    ret = SSL_read(cbufs[user].ssl_con, p->rbuf->pos, remain);
  }
#else
  ret = read(user, p->rbuf->pos, remain);
#endif
  if ( ret < 0) {
    /* we haven't gotten all the data they said they
     * were gonna send (incomplete packet)
     */
    if (errno == EWOULDBLOCK) {
      return(0);
    } else if (errno == EPIPE || errno == ECONNRESET) {
      return(-2);
    } else {
#ifdef HAVE_SSL
      if (p->ssl_con == NULL) {
	vmdb(MSG_ERR, "fd%d: Couldn't read packet.", user);
      } else {
	vmdb(MSG_ERR, "fd%d (SSL): Couldn't read packet.", user);
      }
#else
      vmdb(MSG_ERR, "fd%d: Couldn't read packet.", user);
#endif
      return(-1);
    }
  }

  if (!ret)
  {
    /* EOF (closed connection) */
    return(-2);
  }

  /* advance read pointer */
  p->rbuf->pos += ret;

  /* see if we read the whole thing */
  if ((remain - ret) == 0) 
  {
    /* yes */

    /* nail down the end of the read string */
    *(p->rbuf->pos) = '\0';

    p->newmsg = 1;

    vmdb(MSG_VERBOSE, "_readpacket: len=%d, pktlen=%d, pkt=\"%s\"", 
      p->rbuf->len, (unsigned char)*(p->rbuf->data), (p->rbuf->data)+1);

    return(1);
  } 
  else
  {
    /* command still incomplete */
    return(0);
  }
}

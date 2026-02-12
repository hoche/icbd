/*
 * pktsocket.c
 *
 * socket handlers
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong.
 * All rights reserved.
 *
 * Originally based on code created by Carrick Sean Casey, as
 * well as code provided by W. Richard Stevens (RIP), but
 * completely rewritten by Michel Hoche-Mong.
 *
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "bsdqueue.h"
#include "server/mdb.h"

#include "pktserv_internal.h"
#include "pktbuffers.h"
#include "pktserv.h"
#include "sslsocket.h"

/* header length. right now it's just the length byte.
 */
#define PACKET_HEADER_LEN    1

/* header validation macro. right now it just returns TRUE. in the 
 * future, it might look for some required signature.
 */
#define VALID_PACKET_HEADER(hdr)     1


/* accept and initialize a new client connection */

/* 
 * returns 0 if the operation was successful or would block.
 *    (sets the listen socket's disposition to BLOCKED if
 *     it would block.)
 * return -1 if the operation failed with a critical error.
 */
int pktsocket_accept(cbuf_t *listen_cbuf)
{
    int ns;  /* new socket - the socket of the accepted client */
    int one = 1;
    int flags;
    struct linger nolinger;
    cbuf_t *cbuf;

    /* accept the connection */
    if ((ns = accept(listen_cbuf->fd, (struct sockaddr *) NULL, NULL)) < 0) {
        if (errno == EWOULDBLOCK) {
            listen_cbuf->disp = BLOCKED;
            return 0;
        } else {
            vmdb(MSG_WARN, "pktsocket_accept::accept() - %s", strerror(errno));
            return -1;
        }
    }
    listen_cbuf->disp = OK;

    /* ok, got a new socket. point the cbuf to the new one's */
    cbuf = &cbufs[ns];

    /* force occasional connection check */
    if (setsockopt(ns, SOL_SOCKET, SO_KEEPALIVE, (char *)&one, sizeof(one)) < 0)
    {
        vmdb(MSG_WARN, "pktsocket_accept::setsockopt(SO_KEEPALIVE) - %s", 
             strerror(errno));
    }

    /* don't allow broken connections to linger */
    nolinger.l_onoff = 1;
    nolinger.l_linger = 0;
    if (setsockopt(ns, SOL_SOCKET, SO_LINGER,
                   (char *)&nolinger, sizeof(nolinger)) < 0) {
        vmdb(MSG_WARN, "pktsocket_accept::setsockopt(SO_LINGER) - %s", 
             strerror(errno));
    }

    /* XXX this shouldn't be necessary anymore, since we have send queues
     */
    one = 24576;
    if (setsockopt(ns, SOL_SOCKET, SO_SNDBUF, (char *)&one, sizeof(one)) < 0) {
        vmdb(MSG_WARN, "pktsocket_accept::setsockopt(SO_SNDBUF) - %s", 
             strerror(errno));
    }

    /* make the socket non-blocking */
    if (fcntl(ns, F_SETFL, FNDELAY) < 0) {
        vmdb(MSG_WARN, "pktsocket_accept::fcntl(FNDELAY) - %s", strerror(errno));
        return -1;
    }

    /* Don't close on exec */
    flags = fcntl(ns, F_GETFD, 0);
    flags = flags & ~ FD_CLOEXEC;
    fcntl(ns, F_SETFD, flags);

    /* clear out the socket's cbuf */
    memset(cbuf, 0, sizeof(cbuf_t));

    /* we're starting with a new command */
    cbuf->fd = ns;
    cbuf->newmsg = 1;

    /* set the new socket's state and disposition */
    if (listen_cbuf->state == LISTEN_SOCKET_SSL) {
        cbuf->state = WANT_SSL_ACCEPT;
        cbuf->is_ssl = 1;
    } else {
        cbuf->state = ACCEPTED;
        cbuf->is_ssl = 0;
    }
    cbuf->disp = OK;

    add_pollfd(cbuf->fd);

    return 0;
}


/* read a packet of data into a cbuf's rbuf
 *
 * return 0 on success or non-critical error (like EWOULDBLOCK)
 * return -1 and set the cbuf state to WANT_DISCONNECT on error
 */
int pktsocket_read(cbuf_t *cbuf)
{
    ssize_t result;
    size_t remain;

    /* try to read the header unless we've already done that */
    if (cbuf->state == WANT_HEADER) {

        /* set up our packet buffer */
        if ( (cbuf->rbuf = _msgbuf_alloc(cbuf->rbuf, MAX_PKT_LEN)) == NULL) {
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }

        /* read the packet header */
        if (cbuf->is_ssl) {
            result = sslsocket_read(cbuf, cbuf->rbuf->data, PACKET_HEADER_LEN);
        } else {
            result = read(cbuf->fd, cbuf->rbuf->data, PACKET_HEADER_LEN);
        }

        /* did we get an error or a partial read? */
        if (result < 0) {
            if (errno == EWOULDBLOCK) {
                /* incomplete read - sslsocket_read() will never return this. */
                return 0;
            }
            vmdb(MSG_ERR, "fd%d%s: Couldn't read header packet.", cbuf->fd, 
                 cbuf->is_ssl ? " (SSL)" : "");
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }

        if (result == 0) {
            if (cbuf->is_ssl) {
                /* need a retry - read_ssl will have set the state */
                return 0;
            } else {
                /* EOF (closed connection) */
                cbuf->state = WANT_DISCONNECT;
                return -1;
            }
        }

        /* check to see if it's a valid packet header */
        if (!VALID_PACKET_HEADER(cbuf->rbuf->data)) {
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }

        /* set our need-to-read size and scootch the pointer up 
         * past the length byte 
         */
        cbuf->rbuf->len = (unsigned char)(*(cbuf->rbuf->data)) + 1;
        cbuf->rbuf->pos++;
        cbuf->newmsg = 0;

        /*
         * this is in case you've configured the server with
         * the cbuf buffer size less than the maximum allowed
         * by the size byte (255) in the protocol. not normal,
         * but it's possible. the -1 is for the null we want to
         * tack on at the end.
         */
        if ( cbuf->rbuf->len > (MAX_PKT_LEN-1) )
        {
            vmdb(MSG_ERR, 
                 "fd#%d: sent oversized packet (%d>%d); terminating connection...",
                 cbuf->fd, (size_t)(cbuf->rbuf->len), (MAX_PKT_LEN-1));
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }
    }


    /* ok, got the header */


    /* read as much of the command packet as we can get */
    remain = cbuf->rbuf->len - (cbuf->rbuf->pos - cbuf->rbuf->data);
    if (cbuf->is_ssl) {
        result = sslsocket_read(cbuf, cbuf->rbuf->pos, remain);
    } else {
        result = read(cbuf->fd, cbuf->rbuf->pos, remain);
    }

    /* did we get an error? */
    if (result < 0) {
        if (errno == EWOULDBLOCK) {
            /* incomplete read - read_ssl() will never return this. */
            cbuf->state = WANT_READ;
            return 0;
        }
        vmdb(MSG_ERR, "fd%d%s: Couldn't read packet body.", cbuf->fd, 
             cbuf->is_ssl ? " (SSL)" : "");
        cbuf->state = WANT_DISCONNECT;
        return -1;
    }

    if (result == 0) {
        if (cbuf->is_ssl) {
            /* need a retry - read_ssl() will have set the state */
            return 0;
        } else {
            /* EOF (closed connection) */
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }
    }


    /* advance read pointer */
    cbuf->rbuf->pos += result;

    if ((remain - result) == 0) {
        cbuf->state = COMPLETE_PACKET;

        /* nail down the end of the read string */
        *(cbuf->rbuf->pos) = '\0';

        vmdb(MSG_VERBOSE, "read_sock: len=%d, pktlen=%d, pkt=\"%s\"", 
             cbuf->rbuf->len, (unsigned char)*(cbuf->rbuf->data), (cbuf->rbuf->data)+1);

    } else {
        /* the packet is still incomplete */
        cbuf->state = WANT_READ;
    }

    return 0;
}



/*
 * write a date from the cbuf's send list to socket file descriptor 
 *
 * returns -1 on connection gone, too many retries have been attempted,
 *    or there was some other serious problem.
 * returns 0 if write or partial write succeeded.
 *
 * Note: this currently tries to write everything from the cbuf's
 *   sendlist at once.
 */
int pktsocket_write(cbuf_t* cbuf)
{
    int result;
    msgbuf_t *msgbuf;
    size_t remain;

    vmdb(MSG_VERBOSE, "writepacket: fd%d has %d packets in queue.", 
         cbuf->fd, cbuf->wlist_size);

    /* XXX set a backoff timer and check it */
    if (cbuf->retries > MAX_SENDPACKET_RETRIES) {
        return -1;
    }

    while (!TAILQ_EMPTY(&(cbuf->wlist))) {
        msgbuf = (msgbuf_t*)TAILQ_FIRST(&(cbuf->wlist));

        remain = msgbuf->len - (msgbuf->pos - msgbuf->data);

        vmdb(MSG_VERBOSE, "writepacket: fd%d sending %d bytes....", 
             cbuf->fd, remain);

        if (cbuf->is_ssl) {
            result = sslsocket_write(cbuf, msgbuf->pos, remain);
        } else {
            result = write(cbuf->fd, msgbuf->pos, remain);
        }


        /* do we need an SSL retry? */
        if (result == 0  && cbuf->is_ssl) {
            cbuf->state = WANT_SSL_WRITE;
            vmdb(MSG_VERBOSE, 
                 "writepacket: fd%d needs an SSL retry.", cbuf->fd);
            cbuf->retries++;
            return 0;
        }

        /* did we have an error? */
        if ( result < 0 ) {
            if (errno == EWOULDBLOCK) {
                /* incomplete write - write_ssl() will never return this. */
                cbuf->state = WANT_WRITE;
                cbuf->retries++;
                return 0;
            }

            vmdb(MSG_ERR, "fd%d%s: Couldn't write packet.", cbuf->fd, 
                 cbuf->is_ssl ? " (SSL)" : "");
            cbuf->state = WANT_DISCONNECT;
            return -1;
        }

        /* did we send the whole packet? */
        if (result < remain) {
            /* try again later with the remaining amount */
            cbuf->state = WANT_WRITE;
            msgbuf->pos += result;
            vmdb(MSG_VERBOSE, 
                 "writepacket: fd%d sent partial packet (%d bytes). will retry sending later.",
                 cbuf->fd, result);
            cbuf->retries++;
            return 0;
        } 


        /* SUCCESS! pop the msgbuf off the stack and delete it */
        TAILQ_REMOVE(&(cbuf->wlist), msgbuf, entries);
        cbuf->wlist_size--;

        free(msgbuf->data);
        free(msgbuf);

        /* also reset the retries */
        cbuf->retries = 0;

        vmdb(MSG_VERBOSE, "writepacket: fd%d sent packet. queue is now %d.",
             cbuf->fd, cbuf->wlist_size);
    }

    cbuf->state = WANT_HEADER;
    return 0;
}

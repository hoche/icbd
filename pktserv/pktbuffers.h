/*
 * pktbuffers.h
 *
 * message and client buffers for a generic packet server
 *
 * Original Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong
 * All rights reserved.
 *
 */

#pragma once

#include "../config.h"

#include "bsdqueue.h"

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

/* msg buffer */
typedef struct msgbuf_st {
    TAILQ_ENTRY(msgbuf_st)    entries;  /* message buffer list entries */

    size_t sz;           /* allocated size of the data buffer */

    size_t len;          /* length of the data in the buffer */
    char *pos;           /* ptr to the current read/write position */

    char *data;          /* data buffer */
} msgbuf_t;


typedef enum {
    DISCONNECTED,        /* disconnected */
    ACCEPTED,            /* accepted, needs initialization by main server */
    IDLE,                /* everything's been handled. nothing to do. */
    WANT_SSL_ACCEPT,     /* need to retry ssl_accept() */
    WANT_SSL_READ,       /* need to retry ssl_read() */
    WANT_SSL_WRITE,      /* need to retry ssl_write() */
    WANT_HEADER,         /* ready for the header of a new packet */
    WANT_READ,           /* in the middle of reading a packet */
    WANT_WRITE,          /* have one or more unsent packets */
    WANT_DISCONNECT,     /* pending disconnect - server needs to be notified */
    WANT_RAW_DISCONNECT, /* pending disconnect - socket level */
    COMPLETE_PACKET,     /* complete packet received. */
    LISTEN_SOCKET,       /* this is a listen socket */
    LISTEN_SOCKET_SSL    /* this is an ssl listen socket */
} SocketState;

typedef enum {
    OK,                  /* ok to work on */
    BLOCKED,             /* waiting for some i/o */
    IGNORE               /* ignore processing this socket */
} SocketDisposition;



/* client buffer */
typedef struct cbuf_st {
    u_int id;               /* this connection's unique identifier */
    int fd;                 /* the client's file descriptor */
    SocketState state;      /* state of the current connection (see above) */
    SocketDisposition disp; /* disposition (see above) */

    /* read buffer */
    msgbuf_t *rbuf;         /* read buffer. just one of these */
    char newmsg;            /* set to TRUE if next read is start of new packet */
    /*        FALSE if a packet is partially read */

    /* write buffers */
    TAILQ_HEAD(mblisthead, msgbuf_st) wlist; /* write list */
    int wlist_size;         /* number of pending packets in list */
    int retries;            /* number of send retries attempted */

    int is_ssl;             /* is this an ssl_connection? */

#ifdef HAVE_SSL
    SSL *ssl_con;           /* this will be NULL if it's a cleartext client */
#endif

} cbuf_t;


msgbuf_t *_msgbuf_alloc(msgbuf_t *msgbuf, size_t sz);
void _free_msgbuf(msgbuf_t *msgbuf);
void cbufs_reset(void);
int cbufs_init(void);


/*
 * pktbuffers.c
 *
 * Buffer handling
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong
 * All rights reserved.
 *
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>

#include "pktserv_internal.h"
#include "pktbuffers.h"


/* if the passed-in msgbuf exists and has sufficient buffer space, clear 
 * it out and reset it, otherwise allocate it.
 * returns the requested msgbuf on if successful, or NULL and sets
 * errno (usually to ENOMEM) on failure.
 * 
 * right now it just mallocs stuff on the fly. probably should pull
 * from a memory pool.
 */
msgbuf_t *
_msgbuf_alloc(msgbuf_t *msgbuf, size_t sz)
{
    if (msgbuf && msgbuf->sz >= sz) {
        memset(msgbuf->data, 0, msgbuf->sz);
        msgbuf->len = 0;
        msgbuf->pos = msgbuf->data;
        return msgbuf;
    }

    if (msgbuf && msgbuf->sz)
        free(msgbuf->data);

    if (!msgbuf) {
        msgbuf = (msgbuf_t*)malloc(sizeof(msgbuf_t));
        if (!msgbuf) {
            errno = ENOMEM;
            return NULL;
        }
    }

    msgbuf->data = (char*)malloc(sz);
    if (!msgbuf->data) {
        free(msgbuf);
        errno = ENOMEM;
        return NULL;
    }

    msgbuf->sz = sz;
    msgbuf->len = 0;
    msgbuf->pos = msgbuf->data;

    return msgbuf;
}

void
_msgbuf_free(msgbuf_t *msgbuf)
{
    if (msgbuf->data) {
        memset(msgbuf->data, 0, msgbuf->sz);
        free(msgbuf->data);
    }
    free(msgbuf);
}

void
cbufs_reset(void)
{
    int i;

    for (i = 0; i < MAX_USERS; i++) {
        cbufs[i].newmsg = 1;
        TAILQ_INIT(&(cbufs[i].wlist));
    }
}

int
cbufs_init(void)
{
    if (cbufs == NULL) {
        cbufs = (cbuf_t*)calloc(sizeof(cbuf_t), MAX_USERS);
    }
    if (!cbufs)
        return -1;
    cbufs_reset();
    return 0;
}

#define socketStateStrCase(x) case x: return #x; break;
const char* socketStateStr(SocketState state)
{
    switch (state) {
        socketStateStrCase(DISCONNECTED);
        socketStateStrCase(ACCEPTED);
        socketStateStrCase(IDLE);
        socketStateStrCase(WANT_SSL_ACCEPT);
        socketStateStrCase(WANT_SSL_READ);
        socketStateStrCase(WANT_SSL_WRITE);
        socketStateStrCase(WANT_READ);
        socketStateStrCase(WANT_WRITE);
        socketStateStrCase(WANT_DISCONNECT);
        socketStateStrCase(WANT_RAW_DISCONNECT);
        socketStateStrCase(COMPLETE_PACKET);
        socketStateStrCase(LISTEN_SOCKET);
        socketStateStrCase(LISTEN_SOCKET_SSL);
    }
    return "UNKNOWN";
};

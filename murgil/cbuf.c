/*
 * buffers.c
 *
 * Buffer handling
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001, Michel Hoche-Mong
 *
 * Released under the GPL license.
 *
 * $Id: cbuf.c,v 1.1 2001/08/04 08:41:13 hoche Exp $
 *
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

#include "cbuf.h"


/* if the passed-in msgbuf exists and has sufficient buffer space, clear 
 * it out and reset it, otherwise allocate it.
 * returns the requested msgbuf on if successful, or NULL and sets
 * errno (usually to ENOMEM) on failure.
 * 
 * right now it just mallocs stuff on the fly. probably should pull
 * from a memory pool.
 */
struct msgbuf_t *_alloc_msgbuf(struct msgbuf_t *msgbuf, size_t sz)
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
        msgbuf = (struct msgbuf_t*)malloc(sizeof(struct msgbuf_t));
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

void _free_msgbuf(struct msgbuf_t *msgbuf)
{
    if (msgbuf->data) {
        memset(msgbuf->data, 0, msgbuf->sz);
        free(msgbuf->data);
    }
    free(msgbuf);
}

/*
 * disconnect.c
 *
 * message and client buffers for a generic select-based server
 *
 * Original Author: Michel Hoche-Mong
 * Copyright (c) 2001, Michel Hoche-Mong, hoche@grok.com
 *
 * Released under the GPL license.
 *
 * $Id: discuser.c,v 1.8 2001/08/04 11:03:06 hoche Exp $
 *
 */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

#include "globals.h"
#include "../server/ipcf.h"  /* for s_lost_user() */

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

/* shut down a connection and take it out of the server list
 * s is the socket fd
 */
void disconnectuser(int s)
{
    int i;
    struct cbuf_t *cbuf;
    struct msgbuf_t *msgbuf;

    cbuf = &(cbufs[s]);

    /* clear out the read buffer */
    if (cbuf->rbuf) {
        if (cbuf->rbuf->data)
            free(cbuf->rbuf->data);
        free(cbuf->rbuf);
        cbuf->rbuf = NULL;
    }


    /* clear out the write buffers */
    while (!TAILQ_EMPTY(&(cbuf->wlist))) {
        msgbuf = TAILQ_FIRST(&(cbuf->wlist));
        TAILQ_REMOVE(&(cbuf->wlist), msgbuf, entries);
        if (msgbuf->data)
            free(msgbuf->data);
        free(msgbuf);
    }

    /* close the fd */
    close(cbuf->fd);

#ifdef HAVE_SSL
    /* clear out the SSL info */
    if (cbuf->ssl_con != NULL) {
        SSL_free(cbuf->ssl_con);
        cbuf->ssl_con = NULL;
    }
#endif

    /* remove the fd from the fdsets and reset the highest fd */
    FD_CLR(cbuf->fd, &rfdset);
    FD_CLR(cbuf->fd, &wfdset);
    FD_CLR(cbuf->fd, &ignorefdset);
    if (cbuf->fd == highestfd) {
        for (i = FD_SETSIZE-1; i > 0 && !FD_ISSET(i, &rfdset); i--);
        highestfd = i;
    }

    /* let main program know user went bye bye */
    /* XXX stupid callback */
    s_lost_user(cbuf->fd);
}

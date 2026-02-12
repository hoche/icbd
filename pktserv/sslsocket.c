/*
 * sslsocket.c
 *
 * wrappers around standard SSL network connection calls, using
 * cbuf_t's.
 *
 * These shouldn't be called directly and instead should be called
 * from the functions in pktsocket.c.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong
 * All rights reserved.
 *
 */

#include "config.h"


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>

#include "pktserv_internal.h"
#include "server/mdb.h"


#ifndef HAVE_SSL

int sslsocket_accept(cbuf_t *cbuf)
{
    mdb(MSG_ERR, "CRITICAL ERROR: sslsocket_accept() called in a non-SSL-enabled server!");
    return -1;
}

int sslsocket_read(cbuf_t *cbuf, void* buf, size_t len)
{
    mdb(MSG_ERR, "CRITICAL ERROR: sslsocket_read() called in a non-SSL-enabled server!");
    return -1;
}

int sslsocket_write(cbuf_t *cbuf, void* buf, size_t len)
{
    mdb(MSG_ERR, "CRITICAL ERROR: sslsocket_write() called in a non-SSL-enabled server!");
    return -1;
}


#else /* HAVE_SSL */

#include <openssl/err.h>
#include <openssl/ssl.h>

/*
 * Why are these functions special?
 *
 * Well, in many cases, the SSL code tries to send as much as possible, but
 * sometimes it can't. Since there's always a two-way conversation taking
 * place, even a seemingly-straightforward SSL_read() can involve needing
 * to write() data, and an SSL_write() can involve needing a read(). The
 * underlying SSL code handles these, but in those cases, we need to do
 * two things: 1) we need to come back and retry the SSL call with
 * *exactly* the same buffer, and 2) we need to notify our main select
 * loop that it needs to watch these sockets for *both* reading and writing.
 * So, when these routines fail in this manner, we have to set the fd's
 * in both the rfdset and the wfdset and then notify the select()'s
 * handler routine to come back here and retry. To handle this second
 * part, there're flags in the cbuf struct we twiddle to let it know
 * which function it needs to call.
 */


/* 
 * do a SSL_accept on a cbuf's socket. the socket should already
 * have been set up by a regular accept() call.
 */
int sslsocket_accept(cbuf_t *cbuf)
{
    int result, ssl_error;

    /* Initialize SSL state once per connection. */
    if (cbuf->ssl_con == NULL) {
        cbuf->ssl_con = SSL_new(ctx);
        if (cbuf->ssl_con == NULL) {
            vmdb(MSG_ERR, "sslsocket_accept: fd%d: couldn't create ssl_con.", cbuf->fd);
            return -1;
        }
        SSL_set_fd(cbuf->ssl_con, cbuf->fd);
    }

    result = SSL_accept(cbuf->ssl_con);
    if (result == 1) {
        vmdb(MSG_INFO,
             "fd%d: SSL connection accepted.", cbuf->fd);
        cbuf->is_ssl = 1;
        cbuf->state = ACCEPTED;

        /* XXX -- do X509 validation stuff here */
        return 0;
    }

    if (result <= 0) {
        ssl_error = SSL_get_error(cbuf->ssl_con, result);
        switch (ssl_error) {

            case SSL_ERROR_WANT_READ:
                cbuf->state = WANT_SSL_ACCEPT;
                vmdb(MSG_INFO,
                     "SSL_accept on fd%d: SSL_ERROR_WANT_READ", cbuf->fd);
                return 0;

            case SSL_ERROR_WANT_WRITE:
                cbuf->state = WANT_SSL_ACCEPT;
                vmdb(MSG_INFO,
                     "SSL_accept on fd%d: SSL_ERROR_WANT_WRITE", cbuf->fd);
                return 0;

            case SSL_ERROR_ZERO_RETURN:
                cbuf->state = WANT_RAW_DISCONNECT;
                vmdb(MSG_ERR,
                     "fd%d: ssl conn marked closed on SSL_accept", cbuf->fd);
                break;

            case SSL_ERROR_WANT_X509_LOOKUP:
                cbuf->state = WANT_RAW_DISCONNECT;
                vmdb(MSG_ERR,
                     "fd%d: ssl conn wants X509 lookup.", cbuf->fd);
                break;

            case SSL_ERROR_SYSCALL:
                cbuf->state = WANT_RAW_DISCONNECT;
                if (result == 0) {
                    vmdb(MSG_ERR,
                         "fd%d: SSL_ERROR_SYSCALL: got EOF", cbuf->fd);
                } else {
                    vmdb(MSG_ERR,
                         "fd%d: SSL_ERROR_SYSCALL: %s", cbuf->fd, strerror(errno));
                }
                break;

            case SSL_ERROR_SSL:
                cbuf->state = WANT_RAW_DISCONNECT;
                ssl_error = ERR_get_error();
                if (ssl_error == 0) {
                    if (result == 0) {
                        vmdb(MSG_ERR,
                             "fd%d, SSL_accept got bad EOF", cbuf->fd);
                    } else {
                        vmdb(MSG_ERR,
                             "fd%d, SSL_accept got socket I/O error", cbuf->fd);
                    }
                } else {
                    char err_buf[256];
                    ERR_error_string(ssl_error, &err_buf[0]);
                    vmdb(MSG_ERR,
                         "fd%d, SSL_accept error: %s", cbuf->fd, err_buf);
                }
                break;
        }
    }

    return -1;
}


/* 
 * Use a cbuf's ssl info to read data from a socket.
 * 
 * returns number of bytes read on success
 * returns 0 if a retry is needed. this also sets the cbuf's
 *           need_ssl_read flag, and may add the socket to the
 *           wfdset (if a write is needed by the retry).
 * returns -1 on critical failure
 */
int sslsocket_read(cbuf_t *cbuf, void *buf, size_t nbytes)
{
    int result, ssl_error;

    result = SSL_read(cbuf->ssl_con, buf, nbytes);
    if (result > 0) {
        /* successful write, but we need to check to see if we need to
         * write more or try a read.
         */
        if (cbuf->wlist_size) {
            cbuf->state = WANT_SSL_WRITE;
        } else if (SSL_pending(cbuf->ssl_con)>0) {
            cbuf->state = WANT_SSL_READ;
        } else {
            cbuf->state = IDLE;
        }
        return result;
    }

    ssl_error = SSL_get_error(cbuf->ssl_con, result);
    switch (ssl_error) {
        case SSL_ERROR_WANT_READ:
            vmdb(MSG_INFO, "sslsocket_read: fd%d: SSL_ERROR_WANT_READ", cbuf->fd);
            cbuf->state = WANT_SSL_READ;
            return 0;

        case SSL_ERROR_WANT_WRITE:
            vmdb(MSG_INFO, "sslsocket_read: fd%d: SSL_ERROR_WANT_WRITE", cbuf->fd);
            cbuf->state = WANT_SSL_WRITE;
            return 0;

        case SSL_ERROR_ZERO_RETURN:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_read: fd%d: SSL conn marked closed", cbuf->fd);
            break;

        case SSL_ERROR_WANT_X509_LOOKUP:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_read: fd%d: SSL conn wants X509 lookup.", cbuf->fd);
            break;

        case SSL_ERROR_SYSCALL:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_read: fd%d: SSL_ERROR_SYSCALL", cbuf->fd);
            break;

        case SSL_ERROR_SSL:
            cbuf->state = WANT_DISCONNECT;
            ssl_error = ERR_get_error();
            if (ssl_error == 0) {
                if (result == 0) {
                    vmdb(MSG_ERR, 
                         "sslsocket_read: fd%d, SSL_write got EOF", cbuf->fd);
                } else {
                    vmdb(MSG_ERR, 
                         "sslsocket_read: fd%d, SSL_write got I/O error", cbuf->fd);
                }
            } else {
                char err_buf[256];
                ERR_error_string(ssl_error, &err_buf[0]);
                vmdb(MSG_ERR, 
                     "sslsocket_read: fd%d, SSL conn error: %s", cbuf->fd, err_buf);
            }
    }

    return -1;

}


/* 
 * Use a cbuf's ssl info to write data to a socket.
 * 
 * returns number of bytes written on success
# returns 0 if a retry is needed. this also sets the cbuf's
 *           need_ssl_write flag, and may add the socket to the
 *           rfdset (if a read is needed by the retry).
 * returns -1 on critical failure
 */
int sslsocket_write(cbuf_t *cbuf, void *buf, size_t nbytes)
{
    int result, ssl_error;

    result = SSL_write(cbuf->ssl_con, buf, nbytes);
    if (result > 0) {
        /* successful write, but we need to check to see if we need to
         * write more or try a read.
         */
        if (cbuf->wlist_size) {
            cbuf->state = WANT_SSL_WRITE;
        } else if (SSL_pending(cbuf->ssl_con)>0) {
            cbuf->state = WANT_SSL_READ;
        } else {
            cbuf->state = IDLE;
        }
        return result;
    }

    ssl_error = SSL_get_error(cbuf->ssl_con, result);
    switch (ssl_error) {
        case SSL_ERROR_WANT_READ:
            vmdb(MSG_INFO, "sslsocket_write: fd%d: SSL_ERROR_WANT_READ", cbuf->fd);
            cbuf->state = WANT_SSL_READ;
            return 0;

        case SSL_ERROR_WANT_WRITE:
            vmdb(MSG_INFO, "sslsocket_write: fd%d: SSL_ERROR_WANT_WRITE", cbuf->fd);
            cbuf->state = WANT_SSL_WRITE;
            return 0;

        case SSL_ERROR_ZERO_RETURN:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_write: fd%d: SSL conn marked closed", cbuf->fd);
            break;

        case SSL_ERROR_WANT_X509_LOOKUP:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_write: fd%d: SSL conn wants X509 lookup.", cbuf->fd);
            break;

        case SSL_ERROR_SYSCALL:
            cbuf->state = WANT_DISCONNECT;
            vmdb(MSG_ERR, 
                 "sslsocket_write: fd%d: SSL_ERROR_SYSCALL", cbuf->fd);
            break;

        case SSL_ERROR_SSL:
            cbuf->state = WANT_DISCONNECT;
            ssl_error = ERR_get_error();
            if (ssl_error == 0) {
                if (result == 0) {
                    vmdb(MSG_ERR, 
                         "sslsocket_write: fd%d, SSL_write got EOF", cbuf->fd);
                } else {
                    vmdb(MSG_ERR, 
                         "sslsocket_write: fd%d, SSL_write got I/O error", cbuf->fd);
                }
            } else {
                char err_buf[256];
                ERR_error_string(ssl_error, &err_buf[0]);
                vmdb(MSG_ERR, 
                     "sslsocket_write: fd%d, SSL conn error: %s", cbuf->fd, err_buf);
            }
    }

    return -1;

}

#endif

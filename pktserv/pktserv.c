/*
 * pktserv.c
 *
 * main event loop for an SSL-enabled poll-based server.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong.
 * All rights reserved.
 *
 * Loosely based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 *
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <string.h>
#include <poll.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else
#include <time.h>
#endif

#include "bsdqueue.h"
#include "server/mdb.h"

#include "pktserv_internal.h"
#include "pktbuffers.h"
#include "pktserv.h"
#include "pktsocket.h"
#include "sslsocket.h"

cbuf_t *cbufs;    /* array of user packet buffers */

int port_fd;    /* plaintext listen port */
int sslport_fd; /* ssl listen port's file descriptor  */


struct pollfd* g_pollset;
int g_pollsetsize = 0;
int g_pollsetmax = 0;

/* private callbacks */
pktserv_cb_t g_pktserv_cb = {0};

/*
 * handle any idle-time chores
 *
 * (this should go away when we have an event queue).
 */
static void
handle_idle(void)
{
    int i;

    if (g_pktserv_cb.idle) {
        g_pktserv_cb.idle(0);
    }

    /* for all users, check ignore/held state */
    for (i = 0; i < MAX_USERS; i++) {
        if (cbufs[i].disp == IGNORE) {
            if ( g_pktserv_cb.ok2read(cbufs[i].fd) == 1 ) {
                if (add_pollfd(cbufs[i].fd) == 0) {
                    cbufs[i].disp = OK;
                }
            }
        }
    }
}

#if 0
/**
 * Hashes a string to produce an unsigned integer, which should be
 * sufficient for most purposes.  In this implementation, key_len
 * is unused.
 *
 * Returns the hashed value.
 *
 * This probably shouldn't go here and I don't remember why I added
 * this in the first place.
 */
static u_int
ht_hash_str(char* key, u_int key_len)
{
    u_int ret_val = 0;
    int i;

    while (*key) { 
        i = (int) *key;
        ret_val ^= i;
        ret_val <<= 1;
        key++;
    } 
    return ret_val;
}
#endif



/* shut down a connection and take it out of the server list
 * s is the socket fd
 */
static void
handle_disconnect(cbuf_t *cbuf)
{
    msgbuf_t *msgbuf;

    /* clear out the read buffer */
    if (cbuf->rbuf) {
        if (cbuf->rbuf->data)
            free(cbuf->rbuf->data);
        free(cbuf->rbuf);
        cbuf->rbuf = NULL;
    }


    /* clear out the write buffers */
    while (!TAILQ_EMPTY(&(cbuf->wlist))) {
        msgbuf = (msgbuf_t*)(TAILQ_FIRST(&(cbuf->wlist)));
        TAILQ_REMOVE(&(cbuf->wlist), msgbuf, entries);
        if (msgbuf->data)
            free(msgbuf->data);
        free(msgbuf);
    }

    cbuf->state = WANT_RAW_DISCONNECT;

}

static void
handle_raw_disconnect(cbuf_t *cbuf)
{
    /* close the fd */
    close(cbuf->fd);

#ifdef HAVE_SSL
    /* clear out the SSL info */
    if (cbuf->ssl_con != NULL) {
        SSL_free(cbuf->ssl_con);
        cbuf->ssl_con = NULL;
    }
#endif

    delete_pollfd(cbuf->fd);

    cbuf->state = DISCONNECTED;
}


/* resize the pollfd array if necessary (we double it) and append our fd to it.
 * we automatically set it to look for both read and write activity.
 *
 * returns 0 on success
 * returns -1 on error (malloc error)
 */
int
add_pollfd(int fd)
{
    int i;

    /* check to see if we already have a pollfd for this fd */
    for (i = 0; i < g_pollsetsize; i++) {
        if ( g_pollset[i].fd == fd ) {
            break;
        }
    }

    /* if there's no pre-existing pollfd for it, add it */
    if (i == g_pollsetsize) {
        /* first time through? Allocate the pollset. */
        if (g_pollsetmax == 0) {
            struct pollfd *newset;
            newset = malloc(sizeof(struct pollfd) * 16);
            if (!newset)
                return -1; 

            g_pollsetmax = 16;
            g_pollset = newset;
        } else if (g_pollsetsize == g_pollsetmax) {
            struct pollfd *newset;
            newset = realloc(g_pollset, sizeof(struct pollfd) * g_pollsetmax * 2);
            if (!newset)
                return -1; 

            g_pollsetmax *= 2;
            g_pollset = newset;
        }
        g_pollset[g_pollsetsize++].fd = fd;
    }

    /* reset the event mask */
    g_pollset[i].events = POLLIN | POLLOUT;
    return 0;
}


/* find the fd in the array, and memcpy everything back one
 * element, being sure to resize the array size. Doesn't effect
 * the actual memory allocated, and hence doesn't change g_pollsetmax.
 *
 */
void 
delete_pollfd(int fd)
{
    int i;
    for (i = 0; i < g_pollsetsize; i++) {
        if ( g_pollset[i].fd == fd ) {
            int move_count = g_pollsetsize - (i + 1);
            if (move_count > 0) {
                memmove(&g_pollset[i],
                        &g_pollset[i+1],
                        (size_t)move_count * sizeof(struct pollfd));
            }
            g_pollsetsize--;
            return;
        }
    }
}


/*
 * State machine to process pollfd's.
 */
static void
pollfd_state_machine(struct pollfd* pollfd)
{
    int writeable;
    int readable;
    cbuf_t *cbuf;

    /* Check bounds: file descriptor must be < MAX_USERS to use as array index */
    if (pollfd->fd >= MAX_USERS) {
        vmdb(MSG_ERR, "pollfd_state_machine: file descriptor %d >= MAX_USERS (%d), ignoring", pollfd->fd, MAX_USERS);
        return;
    }

    cbuf = &cbufs[pollfd->fd];

    /* if we have an exception, disconnect 'em */
    if ( (pollfd->revents & POLLERR) ||
         (pollfd->revents & POLLHUP) ||
         (pollfd->revents & POLLNVAL)) {
        /*
         * A remote close often surfaces as POLLHUP without a prior read that
         * transitions to WANT_DISCONNECT. Notify upper layers here so login/
         * group bookkeeping (e.g. sign-off messages) still runs.
         */
        if (g_pktserv_cb.lost_client &&
            cbuf->state != LISTEN_SOCKET &&
            cbuf->state != LISTEN_SOCKET_SSL) {
            g_pktserv_cb.lost_client(cbuf->fd);
        }
        handle_disconnect(cbuf);
        return;
    }


    readable = pollfd->revents & POLLIN;
    writeable = pollfd->revents & POLLOUT;
    if (readable || writeable) {
        cbuf->disp = OK;
    }

    /* 
     * run through as many states as we can, and only stop when
     * we're either done or would block.
     */
    while (cbuf->disp == OK ) {
        switch (cbuf->state) {
            case LISTEN_SOCKET:
            case LISTEN_SOCKET_SSL:
                pktsocket_accept(cbuf);
                return;

            case ACCEPTED:          /* freshly accepted client. inform the upper level */
                cbuf->state = WANT_HEADER;
                if (g_pktserv_cb.new_client)
                    g_pktserv_cb.new_client(cbuf->fd, cbuf->is_ssl);
                break;

            case WANT_SSL_ACCEPT:   /* need to retry the accept */
                if (readable || writeable)
                    sslsocket_accept(cbuf);
                return;

            case WANT_SSL_READ:     /* need to retry the SSL read */
                if (readable || writeable)
                    pktsocket_read(cbuf);
                return;

            case WANT_SSL_WRITE:    /* need to retry the SSL write */
                if (readable || writeable)
                    pktsocket_write(cbuf);
                return;

            case WANT_WRITE:        /* we have pending writes. This can be a new SSL write. */
                if (writeable) {
                    pktsocket_write(cbuf);
                }
                if (readable && (cbuf->state == WANT_READ || cbuf->state == WANT_HEADER)) {
                    /* pktsocket_write() may have set this to BLOCKED */
                    cbuf->disp = OK;
                }
                break;

            case IDLE:              /* not doing anything. got some network data */
                if (readable) {
                    cbuf->state = WANT_HEADER;
                }
                // fallthrough

            case WANT_HEADER:       /* need to read a new packet header */
            case WANT_READ:         /* in the middle of a read. read more network data. */
                if (readable) {
                    int ok2read = 1;
                    if (g_pktserv_cb.ok2read) {
                        ok2read = g_pktserv_cb.ok2read(cbuf->fd);
                    }
                    if (ok2read) {
                        pktsocket_read(cbuf);
                        if (cbuf->state == COMPLETE_PACKET && g_pktserv_cb.dispatch) {
                            g_pktserv_cb.dispatch(cbuf->fd, cbuf->rbuf->data);
                            _msgbuf_free(cbuf->rbuf);
                            cbuf->rbuf = NULL;
                            /* ready for the next packet */
                            cbuf->state = WANT_HEADER;
                        }
                    }
                }
                if (!TAILQ_EMPTY(&(cbuf->wlist))) {
                    /* cycle back to want write if there's stuff left */
                    cbuf->state = WANT_WRITE;
                }
                return;

            case WANT_DISCONNECT:      /* need to disconnect the user */
                vmdb(MSG_WARN, "%s: fd%d, disconnecting user\n", __FUNCTION__, cbuf->fd);
                /* let main program know client went bye-bye */
                if (g_pktserv_cb.lost_client)
                    g_pktserv_cb.lost_client(cbuf->fd);

                handle_disconnect(cbuf);
                break;

            case WANT_RAW_DISCONNECT:
                handle_raw_disconnect(cbuf);
                break;

            case DISCONNECTED:
                /* Nothing left to do – break out of the while loop so
                 * the main event loop can continue processing other fds.
                 * Without this, disp stays OK and we spin forever. */
                cbuf->disp = BLOCKED;
                break;

            default: 
                break;
        }
    }
}

/* 
 * run all the elements in the pollfd set through the statemachine
 */
void
handle_pollfds(void)
{
    int i;
    int pollsetsize = g_pollsetsize; /* save this because we can resize it in the state machine */
#ifdef DEBUG
    for (i = 0; i < pollsetsize; i++) {
        vmdb(MSG_DEBUG, "%s: {fd=%d, events=%d, revents=%d}", __FUNCTION__,
            g_pollset[i].fd, g_pollset[i].events, g_pollset[i].revents);
    }
#endif
    for (i = 0; i < pollsetsize; i++)
        pollfd_state_machine(&g_pollset[i]);
}


/**************************************************************
 * Entry points
 **************************************************************/


int pktserv_init(char *config, pktserv_cb_t *cb) 
{
    /*
       read_config(config);
     */

    cbufs_init();

    /* XXX set up default dispatch functions */

    if (cb->idle) {
        g_pktserv_cb.idle = cb->idle;
    }
    if (cb->dispatch) {
        g_pktserv_cb.dispatch = cb->dispatch;
    }
    if (cb->new_client) {
        g_pktserv_cb.new_client = cb->new_client;
    }
    if (cb->lost_client) {
        g_pktserv_cb.lost_client = cb->lost_client;
    }
    if (cb->ok2read) {
        g_pktserv_cb.ok2read = cb->ok2read;
    }

    return 0;
}


/* 
 * The main server loop. Polls the set of file descriptors looking for 
 * activity in the pollfdset. If there is, it gets handed off to 
 * handle_pollfds().
 *
 * If there is no activity after a period of time, handle_idle() gets called.
 * This should be replaced by an event queue.
 */
void
pktserv_run(void)
{
    int ret;
    long loopcount = 0;
    int poll_timeout;

    if (POLL_TIMEOUT < 0) 
        poll_timeout = 0;
    else
        poll_timeout = POLL_TIMEOUT * 1000;

#ifdef DEBUG
    poll_timeout = -1; /* block indefinitely */
#endif

    for (;;) {
        loopcount++;

        ret = poll(g_pollset, g_pollsetsize, poll_timeout); // millisec
        if (ret < 0) {
            if (errno == EINTR)
                ret = 0;
            else
                vmdb(MSG_ERR, "%s: %s", __FUNCTION__, strerror(errno));
        }

        if (ret > 0) {
            handle_pollfds();
        }

        if (ret >= 0 || loopcount%10==0 ) {
            loopcount = 0;
            handle_idle();
        }
    }
}

int pktserv_addport(char *host_name, int port_number, int is_ssl)
{
    struct addrinfo hints, *res, *rp;
    char port_str[16];
    int one = 1;
    int s = -1;
    int flags;

    snprintf(port_str, sizeof(port_str), "%d", port_number);

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;      /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;      /* For wildcard bind */

    const char *node = NULL;
    if (host_name != NULL && host_name[0] != '\0') {
        node = host_name;
    }

    int rv = getaddrinfo(node, port_str, &hints, &res);
    if (rv != 0) {
        vmdb(MSG_ERR, "%s: getaddrinfo: %s", __FUNCTION__, gai_strerror(rv));
        return(-1);
    }

    /*
     * Try each returned address until we successfully bind.
     * Prefer IPv6 with dual-stack (IPV6_V6ONLY=0) so that a single
     * socket can accept both IPv4 and IPv6 connections.  Not all
     * systems return IPv6 first from getaddrinfo, so we make two
     * passes: first try AF_INET6, then AF_INET.
     */
    for (int pass = 0; pass < 2 && s < 0; pass++) {
        int want_family = (pass == 0) ? AF_INET6 : AF_INET;
        for (rp = res; rp != NULL; rp = rp->ai_next) {
            if (rp->ai_family != want_family)
                continue;

            s = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (s < 0)
                continue;

            /* For IPv6, enable dual-stack so we also accept IPv4
             * connections via IPv4-mapped IPv6 addresses. */
            if (rp->ai_family == AF_INET6) {
                int v6only = 0;
                if (setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY,
                               &v6only, sizeof(v6only)) < 0) {
                    vmdb(MSG_INFO, "%s: setsockopt(IPV6_V6ONLY=0) failed, "
                         "trying next address", __FUNCTION__);
                    close(s);
                    s = -1;
                    continue;
                }
            }

            /* allow quick rebind after restart */
            if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                           (char *)&one, sizeof(one)) < 0) {
                vmdb(MSG_ERR, "%s: setsockopt(SO_REUSEADDR)", __FUNCTION__);
            }

            if (bind(s, rp->ai_addr, rp->ai_addrlen) == 0)
                break;  /* success */

            vmdb(MSG_INFO, "%s: bind() failed for an address, trying next",
                 __FUNCTION__);
            close(s);
            s = -1;
        }
    }

    freeaddrinfo(res);

    if (s < 0) {
        vmdb(MSG_ERR, "%s: could not bind to any address for port %d",
             __FUNCTION__, port_number);
        return(-1);
    }

    /* start listening for connections */
    listen(s, 5);

    /* make it non-blocking */
    if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
        vmdb(MSG_ERR, "%s: fcntl(O_NONBLOCK)", __FUNCTION__);
        close(s);
        return(-1);
    }

    /* Don't close on exec */
    flags = fcntl(s, F_GETFD, 0);
    flags = flags & ~ FD_CLOEXEC;
    if (fcntl(s, F_SETFD, flags) < 0) {
        vmdb(MSG_ERR, "%s: fcntl(FD_CLOEXEC)", __FUNCTION__);
        exit (-1);
    }

    /* set the send buffer size */
    one = 24576;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&one, sizeof(one)) < 0) {
        vmdb(MSG_ERR, "%s: setsockopt(SO_SNDBUF)", __FUNCTION__);
    }

    /* Check bounds: file descriptor must be < MAX_USERS to use as array index */
    if (s >= MAX_USERS) {
        vmdb(MSG_ERR, "%s: file descriptor %d >= MAX_USERS (%d), cannot add listen port", __FUNCTION__, s, MAX_USERS);
        close(s);
        return -1;
    }

    /* set this socket's state to be a non-blocked, unignored listen socket */
    cbufs[s].disp = OK;
    if (is_ssl) {
        cbufs[s].state = LISTEN_SOCKET_SSL;
    } else {
        cbufs[s].state = LISTEN_SOCKET;
    }

    vmdb(MSG_INFO, "%s: setting cbufs[%d].fd to %d", __FUNCTION__, s, s);
    cbufs[s].fd = s;

    /* XXX error check this */
    add_pollfd(s);

    /* allow us to handle problems gracefully */
    signal(SIGPIPE, SIG_IGN);

    return s;
}

/* Send a packet to a client. adds a packet to a client's list of packets that
 * need to be sent, and then calls the lower networking calls to send them.
 *
 * if the client already has MAX_SENDPACKET_QUEUE packets waiting to go,
 * return -1.
 *
 */
int pktserv_send(int s, char *pkt, size_t len)
{
    cbuf_t *cbuf;
    msgbuf_t *msgbuf;

    /* XXX check this to be sure this socket makes sense */
    cbuf = &(cbufs[s]);

    if (cbuf->state == WANT_DISCONNECT || cbuf->state == WANT_RAW_DISCONNECT) {
        vmdb(MSG_ERR, "%s() called on socket in DISCONNECT state", __FUNCTION__);
        return -1;
    }

    cbuf->state = WANT_WRITE;

    vmdb(MSG_VERBOSE, "sendpacket: len=%d, pktlen=%d, pkt=\"%s\"", 
         len, (unsigned char)*pkt, pkt+1);

    /* if we already have an unwritten message, bomb out */
    if (cbuf->wlist_size >= MAX_SENDPACKET_QUEUE) {
        vmdb(MSG_ERR, "%s: fd%d already has %d pending writes.", __FUNCTION__, cbuf->fd, cbuf->wlist_size);
        return -1;
    }

    /* allocate a write buffer */
    msgbuf = _msgbuf_alloc(NULL, len);
    if (!msgbuf)
        return -1;

    memcpy(msgbuf->data, pkt, len);
    msgbuf->len = len;
    msgbuf->pos = msgbuf->data;

    if (TAILQ_EMPTY(&(cbuf->wlist))) {
        TAILQ_INSERT_HEAD(&(cbuf->wlist), msgbuf, entries);
    } else {
        TAILQ_INSERT_TAIL(&(cbuf->wlist), msgbuf, entries);
    }
    cbuf->wlist_size++;

    cbuf->state = WANT_WRITE;

    if (pktsocket_write(cbuf) < 0) {
        vmdb(MSG_WARN, "%s: fd%d, error sending packet.", __FUNCTION__, s);
        cbuf->state = WANT_DISCONNECT;
        return -1;
    }

    return 0;
}

int pktserv_disconnect(int s) 
{
    cbuf_t *cbuf;

    /* XXX check this to be sure this socket makes sense */
    cbuf = &(cbufs[s]);

    cbuf->state = WANT_DISCONNECT;
    return 0;
}

void pktserv_dumpsockets(FILE *dump)
{
    int i;

    fprintf(dump, "%d\n", g_pollsetsize);
    for (i = 0; i < g_pollsetsize; ++i) {
        fprintf(dump, "%d\n", g_pollset[i].fd);
    }

    fprintf(dump, "%d\n", port_fd);
    fprintf(dump, "%d\n", sslport_fd);
}

/* I'm not sure this works. It'll probably work for regular sockets
 * if everything's flushed for that socket, but who knows. For SSL
 * sockets? It seems the SSL state would be all fubared.
 *
 * The fact that this *ever* worked (going back to 1988) is a complete
 * fluke as far as I know.
 */
void pktserv_loadsockets(FILE *dump)
{
    int i;
    int k;

    fscanf(dump, "%d\n", &k);
    g_pollsetsize = k;

    for (i = 0; i < g_pollsetsize; ++i) {
        fscanf(dump, "%d\n", &k);
        add_pollfd(k);
    }

    fscanf(dump, "%d\n", &port_fd);
    fscanf(dump, "%d\n", &sslport_fd);
}

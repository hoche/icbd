/*
 * getrname.c
 *
 * Utility functions to look up and verify hostnames.
 * Supports both IPv4 and IPv6 using the modern getaddrinfo/getnameinfo APIs.
 *
 * Original Author: Carrick Sean Casey?
 * IPv6 rewrite: 2026
 *
 */

/* return a remote connection's host name, or NULL if it */
/* can't be looked up */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

#ifdef HAVE_LIBRESOLV
#include <arpa/nameser.h>
#include <resolv.h>
#endif

#include "server/mdb.h"

#define RNAME_LEN 256

/*
 * addr_to_numeric - format a sockaddr as a numeric string (e.g. "1.2.3.4"
 * or "2001:db8::1").  Returns 0 on success, -1 on failure.
 */
static int addr_to_numeric(const struct sockaddr *sa, socklen_t salen,
                           char *buf, size_t buflen)
{
    return getnameinfo(sa, salen, buf, buflen, NULL, 0, NI_NUMERICHOST);
}


#ifndef NO_DOUBLE_RES_LOOKUPS
/*
 * double_reverse_check_sa - protocol-independent double-reverse DNS check.
 *
 * Given a sockaddr, resolve it to a hostname via getnameinfo(), then
 * re-resolve that hostname via getaddrinfo() and verify that one of the
 * returned addresses matches the original.
 *
 * On success, copies the verified hostname into 'hostbuf' (up to hostbuflen)
 * and returns 0.  On failure returns -1.
 */
static int double_reverse_check_sa(const struct sockaddr *sa, socklen_t salen,
                                   char *hostbuf, size_t hostbuflen)
{
    char host[NI_MAXHOST];
    struct addrinfo hints, *res, *rp;
    char orig_numeric[NI_MAXHOST];
    char cand_numeric[NI_MAXHOST];

    /* Step 1: reverse lookup (address → hostname) */
    if (getnameinfo(sa, salen, host, sizeof(host), NULL, 0, NI_NAMEREQD) != 0)
        return -1;

    /* Step 2: forward lookup (hostname → addresses) */
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = sa->sa_family;   /* match the original address family */
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, NULL, &hints, &res) != 0)
        return -1;

    /* Get a numeric string for the original address for comparison */
    if (addr_to_numeric(sa, salen, orig_numeric, sizeof(orig_numeric)) != 0) {
        freeaddrinfo(res);
        return -1;
    }

    /* Step 3: check if any forward-resolved address matches the original */
    for (rp = res; rp != NULL; rp = rp->ai_next) {
        if (addr_to_numeric(rp->ai_addr, rp->ai_addrlen,
                            cand_numeric, sizeof(cand_numeric)) != 0)
            continue;

        if (strcmp(orig_numeric, cand_numeric) == 0) {
            /* Match! */
            snprintf(hostbuf, hostbuflen, "%s", host);
            freeaddrinfo(res);
            return 0;
        }
    }

    freeaddrinfo(res);
    return -1;  /* no match – failed double-reverse */
}
#endif  /* !NO_DOUBLE_RES_LOOKUPS */


char *getremotename(int socketfd)
{
    static char rname[RNAME_LEN];
    struct sockaddr_storage rs;
    socklen_t rs_size = sizeof(rs);
    char numerichost[RNAME_LEN - 3]; /* leave room for "[]\0" */

    /* Don't want to waste all our time in resolving stuff... */
#ifdef HAVE_LIBRESOLV
    _res.retrans = 3;
    _res.retry = 2;

#ifdef VERBOSE
    vmdb(MSG_INFO, "_res: retrans = %d, retry = %d, options = %d",
         _res.retrans, _res.retry, _res.options);
#endif
#endif

    memset(rname, 0, sizeof(rname));

    /* get address of remote user */
    if (getpeername(socketfd, (struct sockaddr *)&rs, &rs_size) < 0) {
        vmdb(MSG_ERR, "getpeername failed for socket %d", socketfd);
        return(NULL);
    }

    /* get a numeric representation for logging / fallback */
    if (addr_to_numeric((struct sockaddr *)&rs, rs_size,
                        numerichost, sizeof(numerichost)) != 0) {
        snprintf(numerichost, sizeof(numerichost), "(unknown)");
    }

#ifdef NO_DOUBLE_RES_LOOKUPS
    /* Simple reverse lookup without double-reverse verification */
    if (getnameinfo((struct sockaddr *)&rs, rs_size,
                    rname, sizeof(rname), NULL, 0, NI_NAMEREQD) != 0) {
        snprintf(rname, sizeof(rname), "[%s]", numerichost);
        vmdb(MSG_INFO, "Can not resolve %s", numerichost);
    }
#else
    /* Double-reverse lookup: reverse → forward → verify */
    if (double_reverse_check_sa((struct sockaddr *)&rs, rs_size,
                                rname, sizeof(rname)) != 0) {
        vmdb(MSG_INFO, "%s fails double reverse", numerichost);
        snprintf(rname, sizeof(rname), "[%s]", numerichost);
    }
#endif  /* NO_DOUBLE_RES_LOOKUPS */

    return(rname);
}

char *getlocalname(int socketfd)
{
    static char rname[RNAME_LEN];
    struct sockaddr_storage rs;
    socklen_t rs_size = sizeof(rs);
    char numerichost[RNAME_LEN - 3]; /* leave room for "[]\0" */

#ifdef HAVE_LIBRESOLV
    /* Don't want to waste all our time in resolving stuff... */
    _res.retrans = 3;
    _res.retry = 2;

    vmdb(MSG_VERBOSE, "retrans = %d, retry = %d, options = %d",
         _res.retrans, _res.retry, _res.options);
#endif

    memset(rname, '\0', sizeof(rname));

    /* get address of local interface */
    if (getsockname(socketfd, (struct sockaddr *)&rs, &rs_size) < 0) {
        vmdb(MSG_ERR, "getsockname failed for socket %d", socketfd);
        return(NULL);
    }

    /* get a numeric representation for logging / fallback */
    if (addr_to_numeric((struct sockaddr *)&rs, rs_size,
                        numerichost, sizeof(numerichost)) != 0) {
        snprintf(numerichost, sizeof(numerichost), "(unknown)");
    }

    /* get hostname from table */
    if (getnameinfo((struct sockaddr *)&rs, rs_size,
                    rname, sizeof(rname), NULL, 0, NI_NAMEREQD) != 0) {
        snprintf(rname, sizeof(rname), "[%s]", numerichost);
        vmdb(MSG_INFO, "Can not resolve %s", numerichost);
    }

    return(rname);
}

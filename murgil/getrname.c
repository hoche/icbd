/*
 * getrname.c
 *
 * Utility functions to look up and verify hostnames.
 *
 * Original Author: Carrick Sean Casey? Jon Luini? Daffy Duck?
 *
 * $Id: getrname.c,v 1.8 2001/08/04 08:43:07 hoche Exp $
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
#include <arpa/nameser.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <resolv.h>
#include <string.h>

#include "server/mdb.h"


struct hostent *double_reverse_check(struct in_addr in)
{
    struct hostent *hp;
    static char *save_host = (char *)NULL;

    /* look up the ip address */
    hp = gethostbyaddr ((char *)&in.s_addr, sizeof (in.s_addr), AF_INET);
    if ( hp == (struct hostent *) NULL ) {
        return ( (struct hostent *)NULL );
    }

    /* if we've been called before and have allocated memory, free it up */
    if ( save_host != (char *) NULL )
        free (save_host);

    /* save a copy of static memory which will be overwritten */
    if ( (save_host = (char *) strdup (hp->h_name)) == (char *) NULL )
        return ( (struct hostent *)NULL );

    /* now, look up the host */
    hp = gethostbyname (save_host);

    if ( hp == (struct hostent *)NULL )
    {
        return ( (struct hostent *)NULL );
    }

    while ( *hp->h_addr_list )
    {
        struct in_addr tin;
        memcpy(&tin, *hp->h_addr_list, hp->h_length);
        if ( tin.s_addr == in.s_addr ) 
        {
            /* if hp->h_name isn't the correct hostname, fudge it */
            if ( strcmp(hp->h_name, save_host) != 0 )
                hp->h_name = save_host;

            return (hp);
        }
        (hp->h_addr_list)++;
    }

    return ( (struct hostent *)NULL );
}


char *getremotename(int socketfd)
{
    static char rname[256];
    struct hostent *host;
    struct sockaddr_in rs;
    socklen_t rs_size = sizeof(rs);

    /* Don't want to waste all our time in resolving stuff... */
#ifdef HAVE_LIBRESOLV
    _res.retrans = 3;
    _res.retry = 2;

#ifdef VERBOSE
    vmdb(MSG_INFO, "_res: retrans = %d, retry = %d, options = %d",
         _res.retrans, _res.retry, _res.options);
#endif
#endif

    memset(rname, 0, 256);
    /* get address of remote user */
    if (getpeername(socketfd, (struct sockaddr *)&rs, &rs_size) < 0) {
        vmdb(MSG_ERR, "getpeername failed for socket %d", socketfd);
        return(NULL);
    }

#ifdef NO_DOUBLE_RES_LOOKUPS
    host = gethostbyaddr((char *)&rs.sin_addr.s_addr,
                         (int)sizeof(rs.sin_addr.s_addr), AF_INET);
#else
    if ( (host = double_reverse_check (rs.sin_addr)) == (struct hostent *)NULL )
        vmdb(MSG_INFO, "%s fails double reverse", inet_ntoa(rs.sin_addr));
#endif	/* NO_DOUBLE_RES_LOOKUPS */

    /* get hostname from table */
    if (host == NULL) 
    {
        sprintf(rname, "[%s]", inet_ntoa (rs.sin_addr));
        vmdb(MSG_INFO, "Can not resolve %s: %s", 
             inet_ntoa(rs.sin_addr),
             hstrerror(h_errno));
    }
    else
        strncpy(rname, host->h_name, sizeof (rname)-1);

    return(rname);
}

char *getlocalname(int socketfd)
{
    static char rname[256];
    struct hostent *host;
    struct sockaddr_in rs;
    socklen_t rs_size = sizeof(rs);

#ifdef HAVE_LIBRESOLV
    /* Don't want to wast all our time in resolving stuff... */
    _res.retrans = 3;
    _res.retry = 2;

    vmdb(MSG_VERBOSE, "retrans = %d, retry = %d, options = %d",
         _res.retrans, _res.retry, _res.options);
#endif

    memset(rname, '\0', 256);
    /* get address of local interface */
    if (getsockname(socketfd, (struct sockaddr *)&rs, &rs_size) < 0) {
        vmdb(MSG_ERR, "getsockname failed for socket %d", socketfd);
        return(NULL);
    }

    /* get hostname from table */
    if ((host = gethostbyaddr((char *)&rs.sin_addr.s_addr,
                              (int) sizeof(rs.sin_addr.s_addr), AF_INET)) == 0)
    {
        sprintf(rname, "[%s]", inet_ntoa(rs.sin_addr));
        vmdb(MSG_INFO, "Can not resolve %s: %s", 
             inet_ntoa(rs.sin_addr),
             hstrerror(h_errno));
    }
    else
    {
        strcpy(rname, host->h_name);
    }
    return(rname);
}

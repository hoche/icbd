/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to handle information requests */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include "server.h"
#include "externs.h"
#include "strutil.h"
#include "users.h"
#include "mdb.h"
#include "send.h"

int s_info(int n, int argc)
{
    char TheirName[20];
    char loginid[MAX_IDLEN+1];
    char nodeid[MAX_NODELEN+1];
    int TheirIndex;

    if (argc == 2) 
    {
        memset(TheirName, 0, 20);
        memset(loginid, 0, MAX_IDLEN + 1);
        memset(nodeid, 0, MAX_NODELEN + 1);
        strncpy(TheirName, getword(fields[1]), 19);

        if ( TheirName[0] == '\0' )
            TheirIndex = lpriv_id[n];
        else
            TheirIndex = find_user(TheirName);

        if (TheirIndex < 0)
        {
            sprintf(mbuf, "Nickname not found.");
        } 
        else 
        {
            struct sockaddr_in rs;
            socklen_t rs_size = sizeof (struct sockaddr_in);
            int aw, idle;

            idle = time(NULL) - u_tab[TheirIndex].t_recv;

            aw = (strlen(u_tab[TheirIndex].awaymsg) > 0);
            getpeername (TheirIndex, (struct sockaddr*)&rs, &rs_size);

            sprintf(mbuf, "%-14s %s@%s (%s) %d%s%s",
                    u_tab[TheirIndex].nickname,
                    u_tab[TheirIndex].loginid,
                    u_tab[TheirIndex].nodeid,
                    inet_ntoa (rs.sin_addr),
                    (idle < 60) ? idle :
                    (idle < 3600) ? (idle / 60) :
                    (idle < 86400) ? (idle / 3600) : (idle/86400),
                    (idle < 60) ? "s" :
                    (idle < 3600) ? "m" :
                    (idle < 86400) ? "h" : "d",
                    aw ? " (aw)" : "");
        }
        sends_cmdout(n, mbuf);
    }
    else
    {
        mdb (MSG_INFO, "info: wrong number of parz");
    }

    return 0;
}


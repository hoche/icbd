/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to send a beep */

#include "config.h"

#include "server.h"
#include "externs.h"
#include "strutil.h"
#include "mdb.h"
#include "users.h"
#include "s_commands.h"
#include "send.h"

/* basically just a rewriting of s_person */
int s_beep(int n, int argc)
{
    int dest;
    char * cp;

    if (argc == 2) {
        /* constraints:
           destination nickname is not null
           destination nickname exists */

        cp = getword(fields[1]);
        if (cp == NULL) {
            mdb(MSG_INFO, "Null string in beep");
            return 0;
        }
        if (strlen(cp) == 0) {
            mdb(MSG_INFO, "Null string in beep");
        } else {
            if( (dest = find_user(cp)) < 0) {
                /* error - no such nick */
                sprintf(mbuf, "%s not signed on.", cp);
                senderror(n, mbuf);
            } else {
                /* send a message to that nick */
                sendbeep(n, dest);

                if (u_tab[n].echoback == 2) 
                {
                    char tbuf[MAX_PKT_DATA];
                    snprintf(tbuf, MAX_PKT_DATA, "<*to: %s*> [=Beep=]",
                             u_tab[dest].nickname);
                    sends_cmdout(n, tbuf);
                }

                /* send an away message if need be */
                away_handle (n, dest);
            }
        }

        /* side-effects: none */
    } else {
        mdb(MSG_INFO, "s_beep: wrong number of parz");
    }
    return 0;
}


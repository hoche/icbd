/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* handle a packet from the server */

#include "config.h"

#include "server.h"
#include "externs.h"
#include "send.h"
#include "msgs.h"
#include "mdb.h"

#include "murgil/murgil.h"

/* dispatch()
 *
 * dispatch to the appropriate function based on the type of message
 * (as determined by the first character in the pkt buffer).
 *
 * n = socket on which the message arrived
 * pkt = packet buffer 
 *
 */ 
void dispatch(int n, char *pkt)
{
    switch(*pkt) {

    case ICB_M_LOGIN:
        if(n >= MAX_REAL_USERS) {
            senderror(n, "ICB is full.");
            sendexit(n);
            disconnectuser(n);
        }
        if(loginmsg(n, ++pkt) < 0) {
            /* login failed.  dump them */
            sendexit(n);
            /*
             * we set S_kill since we want to make sure
             * error msgs reach them before the disconnect
             */
            S_kill[n]++;  
        }
        break;

        /*
           this case duplicates the login case label, and so is omitted.

           case ICB_M_LOGINOK:
           senderror(n, "Server doesn't handle ICB_M_LOGINOK packets");
           break;
         */

    case ICB_M_OPEN:
        openmsg(n, ++pkt);
        break;

    case ICB_M_PERSONAL:
        senderror(n, "Server doesn't handle ICB_M_PERSONAL packets");
        break;

    case ICB_M_STATUS:
        senderror(n, "Server doesn't handle ICB_M_STATUS packets");
        break;

    case ICB_M_ERROR:
        senderror(n, "Server doesn't handle ICB_M_ERROR packets");
        break;

    case ICB_M_IMPORTANT:
        senderror(n, "Server doesn't handle ICB_M_IMPORTANT packets");
        break;

    case ICB_M_EXIT:
        senderror(n, "Server doesn't handle ICB_M_EXIT packets");
        break;

    case ICB_M_COMMAND:
        cmdmsg(n, ++pkt);
        break;

    case ICB_M_CMDOUT:
        senderror(n, "Server doesn't handle ICB_M_CMDOUT packets");
        break;

    case ICB_M_PROTO:
        senderror(n, "Server doesn't handle ICB_M_PROTO packets");
        break;

    case ICB_M_BEEP:
        senderror(n, "Server doesn't handle ICB_M_BEEP packets");
        break;

    case ICB_M_PING:
        senderror(n, "Server doesn't handle ICB_M_PING packets");
        break;

    case ICB_M_PONG:
        pong(n, pkt);
        break;

    case ICB_M_NOOP:    /* no-op does nuthin' */
        break;

    default:
        sprintf(mbuf, "%d: Invalid packet type \"%c\" (%s)", n, *pkt, pkt);
        senderror (n, mbuf);
        mdb(MSG_ERR, mbuf);
    }
}


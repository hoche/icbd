/* Original copyright (c) 1991 by John Atwood deVries II.
 *
 * Almost everything in this file rewritten by Michel Hoche-Mong, 5/24/19
 */

/* send various messages to the client */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>
#include <errno.h>

#include "version.h"
#include "server.h"
#include "externs.h"
#include "protocol.h"
#include "strutil.h"
#include "msgs.h"
#include "mdb.h"
#include "send.h"

#include "murgil/murgil.h" /* for sendpacket() */

extern char * getremotename(int socketfd);

char packetbuffer[MAX_PKT_LEN]; /* packet buffer */


/* send an error message to the client */
void senderror(int to, const char *message)
{
    char msgbuf[MAX_PKT_DATA];
    filtertext(message, msgbuf, MAX_PKT_DATA);
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s",
            ICB_M_ERROR, msgbuf);
    doSend(-1, to);
}

/* send personal message to a client */
void sendperson(int from, int to, const char *message)
{

    char nickbuf[MAX_NICKLEN+1];
    char msgbuf[MAX_PKT_DATA];

    filtertext(u_tab[from].nickname, nickbuf, MAX_NICKLEN+1);
    filtertext(message, msgbuf, MAX_PKT_DATA-MAX_NICKLEN-1);

    char one[255], two[MAX_NICKLEN+1];
    snprintf(one, 255, "%s@%s", u_tab[from].loginid, u_tab[from].nodeid);
    ucaseit(one);
    snprintf(two, MAX_NICKLEN, "%s", u_tab[from].nickname);
    ucaseit(two);

    if ((!nlmatch(one, *u_tab[to].pri_s_hushed)) &&
        (!nlmatch(two, *u_tab[to].pri_n_hushed))) 
    {
        /* see note below in sendopen() as it applies here too */
        snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s", 
                 ICB_M_PERSONAL, nickbuf, msgbuf);
        lpriv_id[to] = from;
        doSend(from, to);
    }
    else
        sendstatus(from, "Bounce", "Message did not go through");
}

/* send normal open group message to the client */
void sendopen(int from, int to, const char *message)
{
    char nickbuf[MAX_NICKLEN+1];
    char msgbuf[MAX_PKT_DATA];

    filtertext(u_tab[from].nickname, nickbuf, MAX_NICKLEN+1);
    filtertext(message, msgbuf, MAX_PKT_DATA-MAX_NICKLEN-1);

    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s",
            ICB_M_OPEN, nickbuf, msgbuf);
    doSend(from, to);
}

/* send an exit message to the client -- makes the client disconnect */
void sendexit(int to)
{
    /* construct exit message */
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c",
            ICB_M_EXIT);
    doSend(-1, to);
}

/* send a ping */
void sendping(int to, const char *who)
{
    /* construct ping message */
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s",
            ICB_M_PING, who);
    doSend(-1, to);
}

/* send a status message to the client */
void sendstatus(int to, const char *status, const char *message)
{
#define MAX_STATUS_LEN 20
    char statusbuf[MAX_STATUS_LEN+1];
    char msgbuf[MAX_PKT_DATA];

    filtertext(status, statusbuf, MAX_STATUS_LEN+1);
    filtertext(message, msgbuf, MAX_PKT_DATA-MAX_STATUS_LEN-1);

    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s", 
             ICB_M_STATUS, statusbuf, msgbuf);
    doSend(-1, to);
}

/* send "end of command" to the client */
void send_cmdend(int to, const char *message)
{
    char msgbuf[MAX_PKT_DATA];

    filtertext(message, msgbuf, MAX_PKT_DATA-4);
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001\001%s", 
             ICB_M_CMDOUT, "ec", msgbuf);
    doSend(-1, to);
}

/* send simple command output message to the client */
void sends_cmdout(int to, const char *message)
{
    char msgbuf[MAX_PKT_DATA];

    filtertext(message, msgbuf, MAX_PKT_DATA-3);
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s", 
             ICB_M_CMDOUT, "co", msgbuf);
    doSend(-1, to);
}

/* send a personal message to a user, using a custom nick. This is
 * mostly used for relaying stored messages from users who aren't
 * logged on at the moment.
 */
void send_person_stored(int to, const char* fromNick, const char *message)
{
    char nickbuf[MAX_NICKLEN+1];
    char msgbuf[MAX_PKT_DATA];

    filtertext(fromNick, nickbuf, MAX_NICKLEN+1);
    filtertext(message, msgbuf, MAX_PKT_DATA-MAX_NICKLEN-1);

    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s",
            ICB_M_PERSONAL, fromNick, msgbuf);
    doSend(-1, to);
}


/* send beep message to a client */
void sendbeep(int from, int to)
{
    if ( u_tab[to].nobeep != 0 )
    {
        senderror(from, "User has nobeep enabled.");

        if ( u_tab[to].nobeep == 2 )
        {
            snprintf (mbuf, MSG_BUF_SIZE,
                      "%s attempted (and failed) to beep you",
                      u_tab[from].nickname);
            sendstatus (to, "No-Beep", mbuf);
        }

        return;
    }

    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s",
        ICB_M_BEEP, u_tab[from].nickname);
    doSend(from, to);
}


void autoBeep(int to)
{
    sendperson(NICKSERV, to, "Beep yerself!");
    u_tab[NICKSERV].t_recv = time(NULL);
}

/* n  =  fd of their socket */
void s_new_user(int n)
{
    char *cp;

    /* construct proto(col) message */
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%d\001%s\001%s",
             ICB_M_PROTO, PROTO_LEVEL, thishost, VERSION);
    doSend(-1, n);

    cp = getremotename(n);

    if (cp == NULL) {
        vmdb(MSG_INFO, mbuf, "[CONNECT] %d", n);
        return;
    }

    if (strlen(cp) == 0) {
        vmdb(MSG_INFO, mbuf, "[CONNECT] %d", n);
        return;
    }

#if defined(SHORT_HOSTNAME) && defined(FQDN)
    if(strcasecmp(SHORT_HOSTNAME, cp) == 0) {
        cp = FQDN;
    }
#endif    /* SHORT_HOSTNAME && FQDN */

    snprintf(u_tab[n].nodeid, MAX_NODELEN+1, "%s", cp);

    vmdb(MSG_INFO, mbuf, "[CONNECT] %d: %s", n, cp);
}

void send_loginok(int to)
{
    /* construct loginok message */
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c",
            ICB_M_LOGINOK);
    doSend(-1, to);
}

/* send an important message to the client */
void sendimport(int to, const char *status, const char *message)
{
#define MAX_STATUS_LEN 20
    char statusbuf[MAX_STATUS_LEN+1];
    char msgbuf[MAX_PKT_DATA];

    filtertext(status, statusbuf, MAX_STATUS_LEN+1);
    filtertext(message, msgbuf, MAX_PKT_DATA-MAX_STATUS_LEN-1);

    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%c%s\001%s", 
             ICB_M_IMPORTANT, statusbuf, msgbuf);
    doSend(-1, to);
}


/*
   Copyright (c) 1991 by Keith Graham
   Modifications Copyright (c) 1991 by John Atwood deVries II

   "to" is the destination socket..
 */

void user_wline(int to, 
                char *mod, 
                char *nick, 
                int idle, 
                int resp, 
                int login, 
                char *user, 
                char *site, 
                char *name)
{ 
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1,
             "%cwl\001%s\001%s\001%ld\001%ld\001%ld\001%s\001%s\001%s%c",
             ICB_M_CMDOUT, mod, nick, (long)idle, (long)resp, (long)login, user, site, name, 0);
    doSend(-1, to);
}

void user_whead(int to)
{ 
    snprintf(&packetbuffer[1], MAX_PKT_LEN-1, "%cwh%c",
        ICB_M_CMDOUT, 0);
    doSend(-1, to);
}

/* This expects packetbuffer to be filled with the command byte
 * at packetbuffer[1] and any data following that. It also
 * expects that the data is NULL terminated.
 *
 * It will put the packet length into packetbuffer[0].
 *
 * Note that the length does not include the length byte itself
 * but does include the trailing NULL.
 *
 * Thus, the maximum we *ever* send is 256 bytes; the length byte
 * plus 255 bytes of packet contents.
 */
int doSend(int from,int to)
{
    int ret;
    int max_users = MAX_REAL_USERS; /* for debugging purposes */
    char line[MAX_PKT_DATA];
    size_t len;

    if (to < 0) {
        vmdb(MSG_ERR, "Attempted to send to negative fd: %d", to);
        return -1;
    }

    if (to >= max_users) /* talking to the server */
    {
        packetbuffer[0] = ' ';
        switch (packetbuffer[1]) {
            case ICB_M_BEEP: /* someone beeped us */
                autoBeep(from);
                break;
            case ICB_M_PERSONAL: /* someone sent us a message */
                split(packetbuffer);
                snprintf(line, MAX_PKT_DATA-2, "%s\001%s", getword(fields[1]), 
                         get_tail(fields[1]));
                cmdmsg(from, line);
                break;
            default: /* do nothing -- ignore */
                break;
        }
        return 0;
    } 

    /*
     * note: we compare against MAX_PKT_LEN
     * to make sure that no packets slipped through
     * that are too big.
     */
    len = strlen(&packetbuffer[1]) + 1; /* include the null terminator */
    if (len > (MAX_PKT_LEN - 1)) { /* reserve room for the length byte itself*/
        mdb(MSG_ERR, "doSend: pbuf too large:");
        vmdb(MSG_ERR, "from=%d to=%d pbuf=%s len=%d",
                from, to, &packetbuffer[1], len);
        snprintf(mbuf, MSG_BUF_SIZE, "Cannot transmit packet: too large");
        senderror(from, mbuf);
        return -1;
    }

    if (S_kill[to] > 0) 
        return -1;

    packetbuffer[0] = (unsigned char)len;
    if ((ret = sendpacket(to, packetbuffer, len + 1 )) < 0) {
        vmdb(MSG_ERR, "doSend: %d: %s (%d)", to, strerror(errno), ret);
        if (ret == -2) {
            /* bad news! */
            S_kill[to]++;
            /* need to clear the notifies of this user,
               lest he try to someone else who's dead
               who has this guy in his notify. Endless
               loop! */
            /* Don't know if it's still needed, but it
               doesn't hurt */
            nlclear(u_tab[to].n_notifies);
            nlclear(u_tab[to].s_notifies);
        }
    }

    return 0;
}

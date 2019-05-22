/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

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


/* send an error message to the client */
void senderror(int to, const char *error_string)
{
    char tmpbuf[BUFSIZ];

    snprintf(tmpbuf, BUFSIZ, "%s", error_string);
    filtertext(tmpbuf);
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s", ICB_M_ERROR, tmpbuf);
    doSend(-1, to);
}

/* send personal message to a client */
void sendperson(int from, int to, const char *message_string)
{
    char one[255], two[MAX_NICKLEN+1];
    char tmpbuf[BUFSIZ];

    snprintf(tmpbuf, BUFSIZ, "%s", message_string);
    filtertext(tmpbuf);

    snprintf(one, 255, "%s@%s", u_tab[from].loginid, u_tab[from].nodeid);
    ucaseit(one);
    snprintf(two, MAX_NICKLEN, "%s", u_tab[from].nickname);
    ucaseit(two);

    if ((!nlmatch(one, *u_tab[to].pri_s_hushed)) &&
        (!nlmatch(two, *u_tab[to].pri_n_hushed))) 
    {
        /* see note below in sendopen() as it applies here too */
        snprintf(pbuf, USER_BUF_SIZE-2, "%c%s\001%s", 
                 ICB_M_PERSONAL, u_tab[from].nickname, tmpbuf);
        lpriv_id[to] = from;
        doSend(from, to);
    }
    else
        sendstatus(from, "Bounce", "Message did not go through");
}

/* send normal open group message to the client */
void sendopen(int from, int to, const char *txt)
{
    char tmpbuf[BUFSIZ];

    snprintf(tmpbuf, BUFSIZ, "%s", txt);
    filtertext(tmpbuf);

    /* construct open message */

    /* NOTE: this can result in text being truncated!!
     * because the protocol sucks donkey dick and "txt" could
     * be up to (USER_BUF_SIZE-2) yet here it tries to squeeze
     * in the user's nickname into the same available space
     *
     * better fix would be to split into two messages
     * best fix would be to design better protocol
     *
     * luckily, many clients are nice and don't use up all of
     * the packet buffer space because presumably sean wrote
     * them to handle this, but new client developers have no
     * clue.
     */
    snprintf(pbuf, USER_BUF_SIZE - 2, "%c%s\001%s", ICB_M_OPEN, 
             u_tab[from].nickname, tmpbuf);
    doSend(from, to);
}

/* send an exit message to the client -- makes the client disconnect */
void sendexit(int to)
{
    /* construct exit message */
    snprintf(pbuf, USER_BUF_SIZE-2, "%c", ICB_M_EXIT);
    doSend(-1, to);
}

/* send a ping */
void sendping(int to, const char *who)
{
    /* construct ping message */
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s", ICB_M_PING, who);
    doSend(-1, to);
}

/* send a status message to the client */
void sendstatus(int to, const char *class_string, const char *message_string)
{
    char tmpbuf1[BUFSIZ], tmpbuf2[BUFSIZ];

    snprintf(tmpbuf1, BUFSIZ, "%s", class_string);
    snprintf(tmpbuf2, BUFSIZ, "%s", message_string);

    filtertext(tmpbuf1);
    filtertext(tmpbuf2);
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s\001%s", 
             ICB_M_STATUS, tmpbuf1, tmpbuf2);
    doSend(-1, to);
}

/* send "end of command" to the client */
void send_cmdend(int to, const char *output_string)
{
    char tmpbuf[BUFSIZ];

    snprintf(tmpbuf, BUFSIZ, "%s", output_string);
    filtertext(tmpbuf);
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s\001\001%s", 
             ICB_M_CMDOUT, "ec", tmpbuf);
    doSend(-1, to);
}

/* send simple command output message to the client */
void sends_cmdout(int to, const char *output_string)
{
    char tmpbuf[BUFSIZ];

    snprintf(tmpbuf, BUFSIZ, "%s", output_string);
    filtertext(tmpbuf);
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s\001%s", 
             ICB_M_CMDOUT, "co", tmpbuf);
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

    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s", ICB_M_BEEP, u_tab[from].nickname);
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
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%d\001%s\001%s",
             ICB_M_PROTO,PROTO_LEVEL, thishost, VERSION);
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
    snprintf(pbuf, USER_BUF_SIZE-2, "%c", ICB_M_LOGINOK);
    doSend(-1, to);
}

/* send an important message to the client */
void sendimport(int to, const char *class_string, const char *output_string)
{
    char tmpbuf1[BUFSIZ], tmpbuf2[BUFSIZ];

    snprintf(tmpbuf1, BUFSIZ, "%s", class_string);
    snprintf(tmpbuf2, BUFSIZ, "%s", output_string);

    filtertext(tmpbuf1);
    filtertext(tmpbuf2);
    snprintf(pbuf, USER_BUF_SIZE-2, "%c%s\001%s", 
             ICB_M_IMPORTANT, tmpbuf1, tmpbuf2);
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
    snprintf(pbuf, USER_BUF_SIZE-2,
             "%cwl\001%s\001%s\001%ld\001%ld\001%ld\001%s\001%s\001%s%c",
             ICB_M_CMDOUT, mod, nick, (long)idle, (long)resp, (long)login, user, site, name, 0);
    doSend(-1, to);
}

/* XXX what the hell does this do? what are c and d? -hoche 5/10/00 */
/*
   void user_wgroupline(int to, char *group, char* topic, c, d)
   { 
   snprintf(pbuf, USER_BUF_SIZE-2, "%cwg\001%s\001%s\001%s\001%s\000",
   ICB_M_CMDOUT, group, topic, c, d);
   doSend(-1, to);
   }
 */

void user_whead(int to)
{ 
    snprintf(pbuf, USER_BUF_SIZE-2, "%cwh%c", ICB_M_CMDOUT, 0);
    doSend(-1, to);
}

int doSend(int from,int to)
{
    int ret;
    int max_users = MAX_REAL_USERS; /* for debugging purposes */
    char line[USER_BUF_SIZE-1];

    if (to < 0) {
        vmdb(MSG_ERR, "Attempted to send to negative fd: %d", to);
        return -1;
    }

    if (to >= max_users) 
    {
        pp[0] = ' ';
        switch (pp[1]) {
            case ICB_M_BEEP: /* someone beeped us */
                autoBeep(from);
                break;
            case ICB_M_PERSONAL: /* someone sent us a message */
                split(pp);
                snprintf(line, USER_BUF_SIZE-2, "%s\001%s", getword(fields[1]), 
                         get_tail(fields[1]));
                cmdmsg(from, line);
                break;
            default: /* do nothing -- ignore */
                break;
        }
    } 
    else
    {
        size_t len;

        /*
         * note: we compare against USER_BUF_SIZE
         * to make sure that no packets slipped through
         * that are too big.
         */
        len = strlen(pbuf) + 1; /* include the null terminator */
        if ((len + 1) > USER_BUF_SIZE) {
            mdb(MSG_ERR, "doSend: pbuf too large:");
            vmdb(MSG_ERR, "from=%d to=%d pbuf=%s len=%d", from, to, pbuf, len);
            snprintf(mbuf, MSG_BUF_SIZE, "Cannot transmit packet: too large");
            senderror(from, mbuf);
            return -1;
        }

        if (S_kill[to] > 0) 
            return -1;

        pp[0] = (unsigned char)len;
        if ((ret = sendpacket(to, pp, len + 1 )) < 0) {
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
    }
    return 0;
}

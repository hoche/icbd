/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* code for the automagical users */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

#include "server.h"
#include "externs.h"
#include "strutil.h"
#include "lookup.h"
#include "protocol.h"
#include "access.h"
#include "send.h"

/* basic dispatch routine */
extern char packetbuffer[];

int s_auto(int n, int f)
{
    char * cp;
    int which;
    int ret = 0;
    char p1[256];

    if (f > 0)
        split(packetbuffer);

    cp = getword(fields[f]);

    which = lookup(cp,auto_table);

    switch(which) {
        case AUTO_READ:
            nickreadmsg(n, NULL);
            break;
        case AUTO_WHO:
            if (f)
                cp = getword(get_tail(fields[f]));
            else
                cp = getword(fields[f+1]);
            ret = nicklookup(n, cp, NULL);
            break;
        case AUTO_REGISTER:
            if (f)
                cp = getword(get_tail(fields[f]));
            else
                cp = getword(fields[f+1]);
            ret = nickwrite(n, cp, 0, NULL);
            break;
        case AUTO_REAL:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: rname Real Name");
            else 
                nickchinfo(n, "realname", cp, 25, "Real Name", NULL);
            break;
        case AUTO_WRITE:
            if (f)
                nickwritemsg(n, 
                             getword(get_tail(fields[f])),
                             get_tail(get_tail(fields[f])),
                             NULL);
            else
                nickwritemsg(n, 
                             getword(fields[f+1]),
                             get_tail(fields[f+1]),
                             NULL);
            break;
        case AUTO_TEXT:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: text Message Text");
            else 
                nickchinfo(n, "text", cp, 200, "Message text", NULL);
            break;
        case AUTO_ADDR:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: addr Address Line 1 | Address Line 2 | Address Line 3");
            else 
                nickchinfo(n, "addr", cp, 79, "Address", NULL);
            break;
        case AUTO_PHONE:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: phone 1-800-555-1212");
            else 
                nickchinfo(n, "phone", cp, 14, "Phone Number", NULL);
            break;
        case AUTO_DELETE:
            if (f)
                cp = getword(get_tail(fields[f]));
            else
                cp = getword(fields[f+1]);
            if ( strlen(cp) == 0 )
                sends_cmdout(n, "Usage: delete password");
            else
                nickdelete(n, cp, NULL);
            break;
        case AUTO_INFO:
            ret = nicklookup(n,"server", NULL);
            break;
        case AUTO_HELP:
            sends_cmdout(n,
                         "Command <argument(s)>           What it does");
            sends_cmdout(n,
                         "---------------------           ------------");
            sends_cmdout(n,
                         "whois <nick>                    returns info if available");
            sends_cmdout(n,
                         "p <passwd>                      registers current user info");
            sends_cmdout(n,
                         "delete <password>               deletes your info from the server db");
            sends_cmdout(n,
                         "cp <oldpassword> <newpasswd>    changes password");
            sends_cmdout(n,
                         "rname                           sets your real name");
            sends_cmdout(n,
                         "phone                           sets your phone number");
            sends_cmdout(n,
                         "addr                            sets your street address");
            sends_cmdout(n,
                         "email                           sets your e-mail address");
            sends_cmdout(n,
                         "text                            sets your text message");
            sends_cmdout(n,
                         "www                             sets your WWW home page");
            sends_cmdout(n,
                         "read                            reads your server messages");
            sends_cmdout(n,
                         "write <nick>                    send someone a server message");
            sends_cmdout(n,
                         "secure                          always requires a password to register");
            sends_cmdout(n,
                         "nosecure                        auto-validates based on username/hostname");
            sends_cmdout(n,
                         "info                            gives more info on Server");
            sends_cmdout(n,
                         "help                            lists this table");
            sends_cmdout(n,
                         "?                               shows usage");
            sends_cmdout(n, "See also /s_help");
            break;
        case AUTO_CP:
            if (f) {
                strncpy(p1, getword(get_tail(fields[f])), 255);
                nickchpass(n, p1, 
                           getword(get_tail(get_tail(fields[f]))),
                           NULL);
            }
            else
            {
                strncpy(p1, getword(fields[f+1]), 255);
                nickchpass(n, p1, 
                           getword(get_tail(fields[f+1])),
                           NULL);
            }
            break;
        case AUTO_SECURE:
            setsecure(n, 1, NULL);
            break;
        case AUTO_NOSECURE:
            setsecure(n, 0, NULL);
            break;
        case AUTO_WWW:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: www URL");
            else
                nickchinfo(n, "www", cp, 80, "WWW", NULL);
            break;
        case AUTO_EMAIL:
            if (f)
                cp = get_tail(fields[f]);
            else
                cp = fields[f+1];
            if (strlen(cp) == 0)
                sends_cmdout(n, "Usage: email e-mail address");
            else 
                nickchinfo(n, "email", cp, 60, "E-Mail", NULL);
            break;
        case AUTO_QUESTION:
        default:
            sendperson(NICKSERV, n,
                       "usage: /m server {whois | p | cp | delete | rname | email | phone | addr | text | read | write | www | info | secure | nosecure | help | ?}");
            break;
    }
    u_tab[NICKSERV].t_recv = time(NULL);

    return ret;
}


/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* "icb" global variables */

#include "config.h"

#include <sys/param.h>
#include <netdb.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>	/* for struct timeval */
#endif

#include "server.h"
#include "groups.h"
#include "users.h"

int debug_level;

/* the world */
USER_ITEM u_tab[MAX_USERS]; /* that many users possible */
GROUP_ITEM g_tab[MAX_GROUPS]; /* only one group for now */

/* non-global definitions */
char messagebuffer[MSG_BUF_SIZE];		/* generic large buffer */

/* global defs */
long TimeToDie = -1.0;
int ShutdownNotify = 0;
int restart = 0;
char *mbuf = messagebuffer;	/* message buffer */
char thishost[MAXHOSTNAMELEN+1];/* our hostname */
time_t curtime;			/* current time */
char **restart_argv;		/* how we were invoked (for /restart) */
int restart_argc;
short S_kill[MAX_USERS];
int lpriv_id[MAX_USERS];	/* last private message came from this id */
int pong_req[MAX_USERS];
struct timeval ping_time[MAX_USERS];

/* lookup tables */

/* commands */
const char *command_table[] =
{
    "drop",
    "shutdown",
    "wall",
    "beep",
    "cancel",
    "g",
    "invite",
    "pass",
    "boot",
    "status",
    "topic",
    "motd",
    "m",
    "echoback",
    "name",
    "v",
    "w",
    "whereis",
    "restart",
    "news",
    "hush",
    "shush",
    "s_help",
    "exclude",
    "shuttime",
    "notify",
    "talk",
    "ping",
    "nobeep",
    "away",
    "noaway",
    "log",
    "stats",
    "brick",
    (char *) 0
};

const char *status_table[] =
{
    "r",	/* control: restricted */
    "m",	/* control: moderated */
    "p",	/* control: public */
    "i",	/* visibility: invisible */
    "s",	/* visibility: secret */
    "v",	/* visibility: visible */
    "q",	/* volume: quiet */
    "n",	/* volume: normal */
    "l",	/* volume: loud */
    "name", /* name of the group */
    "?",	/* list this table */
    "c",	/* control: controlled */
    "#",	/* maximum number of users in the group */
    "b",	/* idleboot setting for the group */
    "idlebootmsg",	/* idleboot string setting for the group */
    "im",	/* idlemod setting for the group */
    (char *) 0
};

const char *auto_table[] =
{
    "read",		/* read messages left for you */
    "whois",	/* whois (lookup) */
    "p",		/* register (loginid, nodeid, nickname, password) */
    "rname",	/* set or change real name */
    "write",	/* leave a message for someone */
    "text",		/* text message */
    "addr",		/* snailmail address */
    "phone",	/* phone number */
    "delete",	/* delete some item */
    "info",		/* information on this service */
    "help",		/* help for use of the service */
    "?",		/* list this table */
    "cp",		/* change password */
    "email",	/* e-mail address */
    "secure", 	/* Secure more secure */
    "nosecure", 	/* Secure less secure */
    "www", 		/* www home pageinfo */
    (char *) 0
};

/* flags set in .icbdrc */
int m_whoheader = 1;	/* who header output */
int m_groupheader = 1;	/* group header output */
int m_nomesg = 0;	/* turn off messages while in icbd */
int m_watchtime = 0;	/* using boring time format */

#if 0
GLOBS const gv = {
    ICBDLOG,	/* logfile */
    "normal",	/* timedisplay */
};
#endif

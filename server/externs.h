/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING */

#ifndef _SERVER_EXTERNS_H_
#define _SERVER_EXTERNS_H_

#include "config.h"
#include "server.h"

#include "groups.h"
#include <errno.h>
#include <sys/param.h>
#include <netdb.h>


/* external definitions for "go" global variables */

/* defined in split.c */
extern char *fields[MAX_FIELDS];	/* split fields */

/* global go variables settable with gorc */
/* extern GLOBS gv; */
	
/* defined in globals.c */
extern char *pp;		/* packet pointer */
extern char *pbuf;		/* packet buffer pointer */
extern char thishost[MAXHOSTNAMELEN+1];	/* our hostname */
extern char *mbuf;
extern time_t curtime;		/* current time */
extern USER_ITEM u_tab[MAX_USERS];	/* user table */
extern GROUP_ITEM g_tab[MAX_GROUPS];	/* group table */

/* lookup tables */
/* commands */
extern char *command_table[];
/* status options */
extern char *status_table[];
/* automagic resident user commands */
extern char *auto_table[];

/* saved args for /restart */
extern char **restart_argv;
extern int restart_argc;

/* if there is a connection to kill, else 0 */
extern short S_kill[MAX_USERS];

extern int lpriv_id[MAX_USERS];
extern int pong_req[MAX_USERS];
extern struct timeval ping_time[MAX_USERS];

/* flags set in .gorc */
extern int m_watchtime;		/* using boring time format */

extern long TimeToDie;
extern int ShutdownNotify;
extern int restart;


#endif /* #ifdef _SERVER_EXTERNS_H_ */

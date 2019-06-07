/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#pragma once

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include <stdio.h>

#include "protocol.h"
#include "namelist.h"

/*
   BEWARE!! of the relationship between MAX_REAL_USERS and the various
   resident automagical users -- esp. for fencepost errors.
 */
#define	MAX_REAL_USERS (MAX_USERS - 1)
#define	NICKSERV (MAX_USERS - 1)

/*
 * define's for various permission bit settings
 */
#define	PERM_NULL	0L
#define	PERM_DENY	1L
#define	PERM_SLOWMSGS	2L

/*
 * define's for login status
 */
#define	LOGIN_FALSE	0
#define	LOGIN_PENDING	1
#define	LOGIN_COMPLETE	2

/* special global variables */

typedef struct {
    char *logfile;		/* name of session log file */
    char *timedisplay;	/* how time is displayed */
} GLOBS;

typedef struct {
    char loginid [MAX_IDLEN+1];  /* their loginid */
    char nodeid [MAX_NODELEN+1];  /* the id of their client's machine */
    char nickname [MAX_NICKLEN+1];  /* their nickname */
    char password [MAX_PASSWDLEN+1];  /* their password */
    char realname [MAX_REALLEN+1];  /* their real name */
    char group[MAX_GROUPLEN+1];  /* name of which group they are in */
    char awaymsg[MAX_AWAY_LEN+1]; /* their away msg, or \0 if not set */
    int lastaway;  /* the last person to whom they sent an away msg */
    time_t lastawaytime; /* and the time they sent it */
    int login;  /* have they sent a login message yet */
    int echoback; /* do they have echoback on or off? */
    int nobeep;	/* do they have nobeep on or off? */
    long perms;	/* permission information */
    int t_notify;   /* Have they been timeout notified? */
    time_t t_on;	/* when signed on, */
    time_t t_sent;	/* last time we sent them something */
    time_t t_recv;	/* last time they sent us something -- */
    time_t t_group;   /* last time they changed groups */
#ifdef BRICK
    int bricks;    /* number of bricks the user has */
#endif
    /* all time stuff */
    NAMLIST * pri_n_hushed;
    NAMLIST * pub_n_hushed;
    NAMLIST * pri_s_hushed;
    NAMLIST * pub_s_hushed;
    NAMLIST * n_notifies;
    NAMLIST * s_notifies;
} USER_ITEM;

typedef struct {
    char name [MAX_GROUPLEN+1];  /* the name of the group */
    /* if inactive, then NULL */
    char topic [MAX_TOPICLEN+1];  /* the topic of that group */
    /* if inactive, then NULL */
    int visibility; /* NORMAL, SECRET, SUPERSECRET */
    int control; /* PUBLIC, MODERATED, RESTRICTED, CONTROLLED */
    int volume; /* quiet, normal, loud */
    int mod; /* index of who is moderator (if any) */
    /* if none, then -1 */
    char missingmod [MAX_NICKLEN+1]; /* the timed out moderator */
    long modtimeout;	/* moderator timeout */
    NAMLIST * n_invites;
    NAMLIST * nr_invites;
    NAMLIST * s_invites;
    NAMLIST * sr_invites;
    NAMLIST * n_bars;
    NAMLIST * n_nr_bars;
    NAMLIST * s_bars;
    NAMLIST * s_nr_bars;
    NAMLIST * n_talk;
    NAMLIST * nr_talk;
    int	size;		/* how many people are allowed */
    int	idleboot;	/* how idle people can be before booted */
#define	IDLEBOOT_MSG_LEN	(79-15-MAX_NICKLEN+2)
    char	idleboot_msg[IDLEBOOT_MSG_LEN];
    /* max length of the message so it'll fit on
     * one line. 79=line length,
     * 15=strlen([=Idle-Boot=] )
     * MAX_NICKLEN = maximum size of nickname
     * 2 = "%s" which is replaced by nickname
     */
    int	idlemod;	/* how idle mods can be before they /pass */
    /*
       next group  (if it was a linked list instead of a table)
     */
} GROUP_ITEM;

/* these are stored in the table */
struct item {
    char nick[MAX_NICKLEN];
    long offset;
};

/* info on the user's tty */

typedef struct {
    char erase;		/* tty erase char */
    char kill;		/* tty kill char */
    char redraw;		/* tty reprint line char */
    char werase;		/* tty word erase char */
    int  rows;		/* tty word erase char */
    int  cols;		/* tty word erase char */
} TTYINFO;


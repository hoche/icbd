/* Copyright (c) 1990 by Carrick Sean Casey. */
/* Modified 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* This file contains routines that are unix dependent. */
/* Eventually, most unixisms should be moved here. */

#include "config.h"

#include <stdio.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "server.h"
#include "externs.h"


/* stash the current time in curtime */
void gettime(void)
{
    time(&curtime);
}


/* set line buffering for an open file pointer */
/* output will be flushed every newline */
void linebuffer(FILE *fp)
{
    setlinebuf(fp);
}

void clearargs(int argc, char *argv[])
{
    int x;
    char *cp;

    for (x = 1; x <= argc-1; x++) {
        cp = argv[x];
        while(*cp)
            *cp++ = ' ';
    }
}

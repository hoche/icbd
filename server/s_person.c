/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to send a personal message */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <sys/time.h>
#endif
#include <stdlib.h>

#include "server.h"
#include "externs.h"
#include "strutil.h"
#include "users.h"
#include "s_commands.h"
#include "unix.h"
#include "send.h"
#include "mdb.h"

int s_personal(int n, int argc)
{
	int dest;
	char tbuf[USER_BUF_SIZE-2];

	if (argc != 2)
	{
		mdb(MSG_INFO, "personal: wrong number of parz");
		return -1;
	}

	/* constraints: 
		destination nickname exists */

	if( (dest = find_user(getword(fields[1]))) < 0) 
	{
		/* error - no such nick */
		sprintf(mbuf,"%s not signed on.", getword(fields[1]));
		senderror(n, mbuf);
	} 
	else 
	{
		char	*tail,
		*args = (char *)NULL;

		/* check to see if their away is set and pester them if so. */
		if ( strlen(u_tab[n].awaymsg) > 0 && dest != NICKSERV )
			sendstatus(n, "Away", "Your away is still set...");

		tail = get_tail (fields[1]);
		args = strdup (tail);

		/* send a message to that nick */
		sendperson(n, dest, tail);

		if (u_tab[n].echoback == 2) 
		{
			snprintf(tbuf, USER_BUF_SIZE - 2, "<*to: %s*> %s", 
				u_tab[dest].nickname,
				(args == (char *)NULL) ? "" : args);

			sends_cmdout(n, tbuf);
		}

		/* send an away message if need-be */
		away_handle(n, dest);

		/* free temporary memory */
		if ( args != (char *)NULL )
		{
			free (args);
			args = (char *) NULL;
		}
	}
	return 0;
}

/*
 * away_handle() - check and process away message (called by s_personal and
 * s_beep) if its set
 */
void away_handle (int src, int dest)
{
    /* see if that person's away is set 
     * if it is, send their away message.
     * if they have echoback verbose on, echo to them
     * as well.
     *
     * do some accounting on the dest's away:
     * check to see if they've already sent an away to
     * us recently, and only send the message if they haven't. 
     * also make note that it was us they sent the message to
     * and note the time for future checks. 
     */
    if ( strlen(u_tab[dest].awaymsg) > 0 )
    {
	if ((u_tab[dest].lastaway != src) || 
	    (time(NULL) - u_tab[dest].lastawaytime > AWAY_NOSEND_TIME))
	{
	    int num_s = 1;
	    int fmt_ok;

	    u_tab[dest].lastaway = src;
	    u_tab[dest].lastawaytime = time(NULL);

	    /* Validate awaymsg before using as format string.
	     * It should contain exactly one %s (for the nickname).
	     * If corrupt/malicious, fall back to literal output.
	     */
	    fmt_ok = (filterfmt(u_tab[dest].awaymsg, &num_s) == NULL);

	    if (fmt_ok)
		snprintf(mbuf, USER_BUF_SIZE - 2, u_tab[dest].awaymsg,
		    u_tab[dest].nickname);
	    else
		snprintf(mbuf, USER_BUF_SIZE - 2, "%s", u_tab[dest].awaymsg);

	    sendstatus(src, "Away", mbuf);
	    if (u_tab[dest].echoback == 2) 
	    {
		struct tm *t;
		char tbuf[USER_BUF_SIZE-2];

		gettime ();
		t = localtime (&curtime);

		snprintf(tbuf, USER_BUF_SIZE-2, "<*to: %s*> [=@%d:%02d%s=] %s", 
		    u_tab[src].nickname,
		    (t->tm_hour > 12) ? (t->tm_hour - 12) :
			    (t->tm_hour == 0) ? 12 : t->tm_hour,
		    t->tm_min,
		    t->tm_hour > 11 ? "pm" : "am",
		    u_tab[dest].awaymsg);

		/* tbuf now has exactly one %s (the one from awaymsg) if
		 * fmt_ok, otherwise zero. Validate again before expanding.
		 */
		num_s = 1;
		if (filterfmt(tbuf, &num_s) == NULL)
		    snprintf(mbuf, USER_BUF_SIZE - 2, tbuf,
			u_tab[dest].nickname);
		else
		    snprintf(mbuf, USER_BUF_SIZE - 2, "%s", tbuf);

		sends_cmdout(dest, mbuf);
	    }
	}
    }
}

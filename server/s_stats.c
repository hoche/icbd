/*
 * Copyright 2001, jon r. luini <jon@luini.com>
 *
 * Released under the GPL.
 */

#include "config.h"
#include "version.h"	/* for mbuf */
#include "externs.h"	/* for mbuf */
#include "access.h"	/* for check_auth() */
#include "send.h"	/* for senderror() */
#include "users.h"	/* for count_users_in_groups() */
#include "s_stats.h"

struct _server_stats server_stats;

/*
 * s_server_stats() - display various statistics about the server
 */
int s_server_stats (int who, int argc)
{
    int i,
	num_users = 0,
	num_groups = 0,
	num_away = 0;

    if ( argc == 2 )
    {
	if ( strlen (fields[1]) > 0 && !strcmp (fields[1], "reset") )
	{
	    if ( !check_auth (who) )
	    {
		senderror (who, "You are not authorized to do this.");
		return (-1);
	    }

	    memset (&server_stats, '\0', sizeof (server_stats));
	    time (&server_stats.start_time);
	    sendstatus (who, "Stats", "Stats have been reset.");
	    return 0;
	}
    }

    /* server settings */
    sends_cmdout (who, "Server Settings:");

    snprintf (mbuf, MSG_BUF_SIZE, "  %s, Protocol Level %d",
	VERSION, PROTO_LEVEL);
    sends_cmdout (who, mbuf);

    snprintf (mbuf, MSG_BUF_SIZE, "  Max Users: %d Max Groups: %d",
	MAX_REAL_USERS, MAX_GROUPS);
    sends_cmdout (who, mbuf);

    /* server stats */
    sends_cmdout (who, "Server Stats:");

    snprintf (mbuf, MSG_BUF_SIZE,
	"  Started: %s", ctime (&server_stats.start_time));
    mbuf[strlen(mbuf)-1] = '\0';	/* drop the newline from ctime */
    sends_cmdout (who, mbuf);

    snprintf (mbuf, MSG_BUF_SIZE,
	"  %ld signon%s, %ld boot%s, %ld drop%s",
	server_stats.signons, server_stats.signons != 1 ? "s" : "",
	server_stats.boots, server_stats.boots != 1 ? "s" : "",
	server_stats.drops, server_stats.drops != 1 ? "s" : "");
    sends_cmdout (who, mbuf);

    snprintf (mbuf, MSG_BUF_SIZE,
	"  %ld idleboot%s, %ld idlemod pass%s",
	server_stats.idleboots, server_stats.idleboots != 1 ? "s" : "",
	server_stats.idlemods, server_stats.idlemods != 1 ? "es" : "");
    sends_cmdout (who, mbuf);

    /* count logged in and away users */
    for (i = 0; i < MAX_REAL_USERS; i++)
	if (u_tab[i].login > LOGIN_FALSE)
	{
	    num_users++;
	    if ( u_tab[i].awaymsg[0] != '\0' )
		++num_away;
       }

    /* count groups */
    for (i = 0; i < MAX_GROUPS; i++)
	if (strlen(g_tab[i].name) > 0)
	   if ((strcmp(g_tab[i].name, "ICB") != 0) ||
	      (count_users_in_group("ICB") > 1))
	   num_groups++;

    snprintf (mbuf, MSG_BUF_SIZE,
	"  %d user%s in %d group%s (%d away user%s)",
	num_users, num_users != 1 ? "s" : "",
	num_groups, num_groups != 1 ? "s" : "",
	num_away, num_away != 1 ? "s" : "");
    sends_cmdout (who, mbuf);

    return 0;
}

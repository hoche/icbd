/*
 * Copyright 2001, jon r. luini <jon@luini.com>
 * 
 * Released under the GPL.
 */

#ifndef _S_STATS_H_
#define _S_STATS_H_

struct _server_stats
{
    time_t	start_time;	/* when this server started */
    long	signons;	/* number of login_complete packets */
    long	boots;		/* how many times users have been booted */
    long	idleboots;	/* "" to IDLE_GROUP */
    long	idlemods;	/* # times group mod passed from idle mods */
    long	drops;		/* number of /drop cmds done */
};

extern struct _server_stats server_stats;

int s_server_stats (int who, int argc);

#endif /* #ifdef _S_STATS_H_ */


/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* user table manipulation */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>	/* for struct timeval */
#endif

#include "server.h"
#include "groups.h"
#include "externs.h"
#include "access.h"

/* clear a particular user entry */
void clear_user_item(int n)
{
	int i;

	if (u_tab[n].login > LOGIN_FALSE)
	    nickwritetime(n, 1, NULL);
	memset(u_tab[n].loginid, 0, MAX_IDLEN+1);
	memset(u_tab[n].nodeid, 0, MAX_NODELEN+1);
	memset(u_tab[n].nickname, 0, MAX_NICKLEN+1);
	memset(u_tab[n].password, 0, MAX_PASSWDLEN+1);
	memset(u_tab[n].realname, 0, MAX_REALLEN+1);
	memset(u_tab[n].group, 0, MAX_GROUPLEN+1);
	memset(u_tab[n].awaymsg, 0, MAX_AWAY_LEN+1);
	u_tab[n].lastaway = 0;
	u_tab[n].lastawaytime = (time_t)0;
	u_tab[n].login = LOGIN_FALSE;
	u_tab[n].echoback = 0;
	u_tab[n].nobeep = 0;
	u_tab[n].perms = PERM_NULL;
	u_tab[n].t_on = (long) 0;
	u_tab[n].t_sent= (long) 0;
	u_tab[n].t_recv = (long) 0;
	u_tab[n].t_group = (long) 0;
	nlclear(u_tab[n].pri_n_hushed);
	nlclear(u_tab[n].pub_n_hushed);
	nlclear(u_tab[n].pri_s_hushed);
	nlclear(u_tab[n].pub_s_hushed);
	nlclear(u_tab[n].n_notifies);
	nlclear(u_tab[n].s_notifies);
	S_kill[n] = 0;
	lpriv_id[n] = -1;
	pong_req[n] = -1;
	timerclear (&ping_time[n]);

	/* clear out anyone that has this slot:
	 *   a) listed as their last priv msg
	 *   b) as a pending pong request
	 */
	for ( i = 0; i < MAX_USERS; i++ )
	{
	    if ( lpriv_id[i] == n )
		lpriv_id[i] = -1;

	    if ( pong_req[i] == n )
	    {
		pong_req[i] = -1;
		timerclear (&ping_time[i]);
	    }
	}
}

/* fill a particular user entry */
/* called when a loginmsg is received on that fd */
void fill_user_entry(int n, 
		const char *loginid, 
		const char *nodeid, 
		const char *nickname, 
		const char *password, 
		const char *group,
		const char *awaymsg,
		int mylogin, 
		int echoback, 
		int nobeep, 
		long perms)
{
	strncpy(u_tab[n].loginid, loginid, MAX_IDLEN);
	strncpy(u_tab[n].nodeid, nodeid, MAX_NODELEN);
	strncpy(u_tab[n].password, password, MAX_PASSWDLEN);
	strncpy(u_tab[n].nickname, nickname, MAX_NICKLEN);
	strncpy(u_tab[n].awaymsg, awaymsg, MAX_AWAY_LEN);

	strncpy(u_tab[n].group, group, MAX_GROUPLEN);
	u_tab[n].login = mylogin;
	u_tab[n].echoback = echoback;
	u_tab[n].t_notify = 0;
	u_tab[n].nobeep = nobeep;
	u_tab[n].perms = perms;
}


/* clear the entire user table */
void clear_users(void)
{
	int i;

	for (i=0; i<MAX_USERS; i++ ) {
		u_tab[i].pri_n_hushed = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].pri_n_hushed, MAX_HUSHED);
		u_tab[i].pub_n_hushed = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].pub_n_hushed, MAX_HUSHED);
		u_tab[i].pri_s_hushed = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].pri_s_hushed, MAX_HUSHED);
		u_tab[i].pub_s_hushed = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].pub_s_hushed, MAX_HUSHED);
		u_tab[i].n_notifies = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].n_notifies, MAX_HUSHED);
		u_tab[i].s_notifies = (NAMLIST *) malloc(sizeof(NAMLIST));
		nlinit(u_tab[i].s_notifies, MAX_HUSHED);
		clear_user_item(i);
	}
}

/* check the user table to see how many of them belong to a particular group */
int count_users_in_group(const char *group)
{
	int result = 0;
	int i;

        /* don't attempt to find if null */
        if (strlen(group) == 0)
                return result;
	
	for (i=0; i<MAX_USERS; i++) {
		if (strcasecmp(u_tab[i].group,group) == 0) {
			result++;
		}
	}
	return result;
}

/* find a slot in the user table with a particular name */
/* return that index if found, -1 otherwise */
/* case insensitive */
int find_user(char *name)
{
        int result = -1;
        int i;

        /* don't attempt to find if null */
        if (strlen(name) == 0)
                return result;

	/* search through table */
        for (i=0; i<MAX_USERS; i++) {
                if ( strcasecmp(u_tab[i].nickname, name) == 0) {
                        result = i;
                        break;
                }
        }
        return result;
}

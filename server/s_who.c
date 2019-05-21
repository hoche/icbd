/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to list users */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

#include "server.h"
#include "externs.h"
#include "namelist.h"
#include "strutil.h"
#include "mdb.h"
#include "users.h"
#include "send.h"

#define DOGROUPONLY	1
#define DOSHORT		2
#define MAX_LINE	75

int add_item(int n, const char * item, char *buf)
{
	if ((strlen(item) + 1) >= MAX_LINE) {
		mdb(MSG_ERR, "Attempted to add_item something too big");
		return -1;
		}
	if ((strlen(item) + strlen(buf) + 1) >= MAX_LINE) {
		sends_cmdout(n, buf);
		memset(buf, 0, MAX_LINE);
		/* do NOT put comma at the beginning of the line */
		if (strcmp(item, ", ") != 0) {
			strcpy(buf, item);
		}
	} else {
		strcat(buf, item);
	}
	return 0;
}


void pgm_short(int n, int which)
{
	int user;
	int num_users;
	char line[MAX_LINE];

	num_users = count_users_in_group(g_tab[which].name);
	memset(line, 0, MAX_LINE);

	add_item(n, "    Members: ", line);

	for(user=0; user<MAX_USERS; user++) {
		/* are they in this group? */
		if(which == find_group(u_tab[user].group)) {
			add_item(n, u_tab[user].nickname, line);
			num_users--;
			if (num_users != 0) {
				add_item(n, ", ", line);
			}
		}
	}
	/* flush last line if any */
	if (strlen(line) != 0) {
		sends_cmdout(n, line);
	}
}

void pgm_long(int n, int which)
{
        int user;
	int Idle;
	long TheTime;
	int gpmod;
	int num_users;
	char cp[5];
	char status[8];
	int nr = 0, aw = 0;
	int user_list[MAX_REAL_USERS];
	int i, j;
	int users = 0;


	TheTime = time(NULL);

	num_users = count_users_in_group(g_tab[which].name);
	if (num_users >= 2)      /* bweh! */
		user_whead(n);
        for(user=0; user<MAX_USERS; user++) {
                /* are they in this group? */
                if(which == find_group(u_tab[user].group)) {
			if (users == 0) 
			   user_list[0] = user;
			else {
		        i = 0;
#ifdef SORT_BY_NICKNAME
		        while ((i < users) && (strcasecmp(u_tab[user].nickname,
		                 u_tab[user_list[i]].nickname) > 0))
#else
		        while ((i < users) && u_tab[user].t_group >
		                 u_tab[user_list[i]].t_group)
#endif
			   i++;
		        for (j = (users + 1); j > i; j--)
			   user_list[j] = user_list[j - 1];
		        user_list[i] = user;
			}
                        users++;
		        }
		}
		for (i = 0; i < users; i++)
			{
			user = user_list[i];
			Idle = TheTime - u_tab[user].t_recv;

			gpmod = (g_tab[which].mod == user);
			if (gpmod) {
				strcpy(cp, "m");
			} else {
				strcpy(cp, " ");
			}

			nr = (strlen(u_tab[user].realname) == 0);
			aw = (strlen(u_tab[user].awaymsg) > 0);

			if (nr || aw)
			{
				status[0] = '(';
				status[1] = '\0';
				if (nr)
					strcat(status, "nr");

				if (nr && aw)
					strcat(status, ",");

				if (aw)
					strcat(status, "aw");
				strcat(status, ")");
			}
			else
				strcpy(status, "");

			user_wline(n, cp, u_tab[user].nickname, Idle, 0,
                                            u_tab[user].t_on,
                                            u_tab[user].loginid,
                                            u_tab[user].nodeid,
                                            status);
                }
}
void print_group_members(int n, int which, int flags)
{
	if (flags & DOGROUPONLY) {
		/* don't print users */
	} else {
		if(flags & DOSHORT) {
			pgm_short(n, which);
		} else {
			pgm_long(n, which);
		}
	}
}

void print_group_title(int n)
{
	sends_cmdout(n, "  Name   ( Moderator  ) [flags] Topic");
}

void print_group_header(int n, int grp)
{
	char GroupName[MAX_GROUPLEN + 3];
	char TheMod[MAX_NICKLEN + 7];
	int mod;
	char TheTopic[MAX_TOPICLEN + 1];
	char Options[20];
	int isMyGroup;
	int is_invited;
	static const char *ctl[] = {"*", "p", "m", "r", "c"};
	static const char *vol[] = {"*", "q", "n", "l"};
	static const char *vis[] = {"*", "v", "s", "i"};
	long TheTime;
	char one[255];

	sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
	ucaseit(one);
	memset(TheMod, 0, MAX_NICKLEN + 1);
	mod = g_tab[grp].mod;
	if (mod < 0)
		if (g_tab[grp].modtimeout == 0.0)
			strcpy(TheMod, "(None)");
		else {
			TheTime = time(NULL);
			sprintf(TheMod, "%s (%d)", g_tab[grp].missingmod, 
				(int) (g_tab[grp].modtimeout - TheTime));
		}
	else
		strcpy(TheMod, u_tab[mod].nickname);

	memset(TheTopic, 0, MAX_TOPICLEN + 1);
	if (strlen(g_tab[grp].topic) == 0) {
		strcpy(TheTopic, "(None)");
	} else {
		strcpy(TheTopic, g_tab[grp].topic);
	}

	memset(Options, 0, 10);
	sprintf(Options,"(%s%s%s)",
			ctl[g_tab[grp].control],
			vis[g_tab[grp].visibility],
			vol[g_tab[grp].volume]);

	isMyGroup = (find_group(u_tab[n].group) == grp);
	memset(GroupName, 0, MAX_GROUPLEN + 1);
       is_invited = (nlpresent(u_tab[n].nickname, 
			   *g_tab[grp].n_invites) || 
			((nlpresent(u_tab[n].nickname, 
			   *g_tab[grp].nr_invites)) && 
			   (strlen(u_tab[n].realname) > 0)) ||
			(nlmatch(one, *g_tab[grp].s_invites)) ||
			((nlmatch(one, *g_tab[grp].s_invites)) &&
			   (strlen(u_tab[n].realname) > 0)) ||
                        (! strncmp("ADMIN", u_tab[n].nickname, MAX_NICKLEN)));
        if ((g_tab[grp].visibility == SECRET) ||
            (g_tab[grp].visibility == SUPERSECRET))
                if (!isMyGroup && !is_invited)
                        strcpy(GroupName,"-SECRET-");
                else {
                        strcpy(GroupName, "*");
                        strcat(GroupName, g_tab[grp].name);
                        strcat(GroupName, "*");
                        }
        else {
                strcpy(GroupName,g_tab[grp].name);
        }

	memset(mbuf, 0, 80);
	sends_cmdout(n, " "); /* a blank line */
	sprintf(mbuf,"Group: %-8s %s Mod: %-13s Topic: %s",
			   GroupName,
			   Options,
			   TheMod,
			   TheTopic);
	sends_cmdout(n, mbuf);

}

void doOne(int n, int flags, char *tgrp, int how)
{
	int which;

	which = find_group(tgrp);

	if (which >= 0) {
		/* the group exists */
		/* how says whether to print the header or not */
/*		if (how)
			print_group_title(n); */
		print_group_header(n, which);
		print_group_members(n, which, flags);
	} else {
		/* the group doesn't exist */
		memset(mbuf, 0, 80);
		sprintf(mbuf, "The group %s doesn't exist.", tgrp);
		senderror(n, mbuf);
	}
}

void doAll(int n, int flags)
{
	int group;
	int my_group;
	int is_invited;
	int num_users = 0;
	int num_groups = 0;
	int group_list[MAX_GROUPS];
	int i, j, groups = 0;
	char one[255];

	my_group = find_group(u_tab[n].group);

	sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
	ucaseit(one);
	for (group=0; group < MAX_GROUPS; group++) {
                is_invited = (nlpresent(u_tab[n].nickname, 
			   *g_tab[group].n_invites) || 
			((nlpresent(u_tab[n].nickname, 
			   *g_tab[group].nr_invites)) && 
			   (strlen(u_tab[n].realname) > 0)) ||
			(nlmatch(one, *g_tab[group].s_invites)) ||
			((nlmatch(one, *g_tab[group].s_invites)) &&
			   (strlen(u_tab[n].realname) > 0)) ||
                        (! strncmp("ADMIN", u_tab[n].nickname, MAX_NICKLEN)));
                if( (strlen(g_tab[group].name)!=0) &&
                    ((g_tab[group].visibility != SUPERSECRET) || is_invited ||
                        (group == my_group))) {
		    if (groups == 0)
		       group_list[0] = group;
		    else {
		       i = 0;
		       while ((i < groups) && (strcasecmp(g_tab[group].name,
		                 g_tab[group_list[i]].name) > 0))
			   i++;
		       for (j = (groups + 1); j > i; j--)
			   group_list[j] = group_list[j - 1];
		       group_list[i] = group;
		       }
		    groups++;
		}
	}
	for (i = 0; i < groups; i++)
            doOne(n, flags, g_tab[group_list[i]].name, 0);

        for (i = 0; i < MAX_REAL_USERS; i++)
            if (u_tab[i].login > LOGIN_FALSE)
               num_users++;
	for (i = 0; i < MAX_GROUPS; i++)
            if (strlen(g_tab[i].name) > 0)
	       if ((strcmp(g_tab[i].name, "ICB") != 0) ||
		  (count_users_in_group("ICB") > 1))
               num_groups++;
	if (num_users == 0)
	   sprintf (mbuf, "No users found.");
	else if (num_users > 1)
	   if (num_groups > 1)
	      sprintf (mbuf, "Total: %d users in %d groups", 
			num_users, num_groups);
	   else
	      sprintf (mbuf, "Total: %d users in %d group", 
			num_users, num_groups);
	else
	   if (num_groups > 1)
	      sprintf (mbuf, "Total: %d user in %d groups", 
			num_users, num_groups);
	   else
	      sprintf (mbuf, "Total: %d user in %d group", 
			num_users, num_groups);

	sends_cmdout(n, mbuf);
}


int s_who(int n, int argc)
{
	int len;
	int how_many;
	int flags;
	int target_user;
	char tgrp[MAX_GROUPLEN + 1];
	char * cp;

	if (argc == 2) {

		/* fields[0] was "w" */
		/* fields[1] is a string indicating the kind of who */

		/* handle the flags (if any) */
		cp = getword(fields[1]);
		if (strcasecmp(cp, "-g") == 0) {
			flags = DOGROUPONLY;
		} else if (strcasecmp(cp, "-s") == 0){
			flags = DOSHORT;
		} else if (strcasecmp(cp, "?") ==0){
			sends_cmdout(n, "Usage: /w {-g | -s} {groupname}");
			return 0;
		} else {
			flags = 0;
		}

		/*
		   use fields[1] if no flag
		   otherwise use get_tail(fields[1])
		*/
		if (flags == 0) {
			len = strlen(getword(fields[1]));
		} else {
			len = strlen(getword(get_tail(fields[1])));
		}

		how_many = (MAX_GROUPLEN > len) ? len:MAX_GROUPLEN;
		memset(tgrp, 0, MAX_GROUPLEN + 1);

		if (flags == 0) {
			strncpy(tgrp, getword(fields[1]),how_many);
		} else {
			strncpy(tgrp, getword(get_tail(fields[1])),
				how_many);
		}

		if (len == 0) {
			/* of all groups */
			doAll(n, flags);
		} else {
			/* on some particular group */
			if (fields[1][0] == '@') {
				if ((target_user = find_user(&fields[1][1])) < 0) {
					senderror(n, "User not found.");
					return -1;
				} else {
					if ((g_tab[find_group(u_tab[target_user].group)].visibility == SUPERSECRET) && (strncmp("ADMIN", u_tab[n].nickname, MAX_NICKLEN))) {
						senderror(n, "User not found.");
						return -1;
					} else {
						strcpy(tgrp, u_tab[target_user].group);
					}
				}
			}

			if (strcmp(tgrp, ".")==0) {
				/* n's group */
				strcpy(tgrp, u_tab[n].group);
			}
			doOne(n, flags, tgrp, 1);
		}
	} else {
		mdb(MSG_INFO, "who: wrong number of parz");
	}

	return 0;
}


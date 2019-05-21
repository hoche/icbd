/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* various defines for group level stuff */

#include "config.h"

#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "groups.h"
#include "users.h"
#include "externs.h"
#include "mdb.h"
#include "namelist.h"


/* clear a particular group entry */
void clear_group_item(int n)
{
	memset(g_tab[n].name, 0, MAX_GROUPLEN+1);
	memset(g_tab[n].topic, 0, MAX_TOPICLEN+1);
	memset(g_tab[n].missingmod, 0, MAX_NICKLEN+1);
	g_tab[n].visibility = VISIBLE;
	g_tab[n].control = PUBLIC;
	g_tab[n].volume = LOUD;
	g_tab[n].mod = -1; /* no one */
	g_tab[n].modtimeout = 0.0;
	nlclear(g_tab[n].n_invites);
	nlclear(g_tab[n].nr_invites);
	nlclear(g_tab[n].s_invites);
	nlclear(g_tab[n].sr_invites);
	nlclear(g_tab[n].n_bars);
	nlclear(g_tab[n].n_nr_bars);
	nlclear(g_tab[n].s_bars);
	nlclear(g_tab[n].s_nr_bars);
	nlclear(g_tab[n].n_talk);
	nlclear(g_tab[n].nr_talk);
	g_tab[n].size = 0;
	g_tab[n].idleboot = DEF_IDLE_BOOT;
	memset(g_tab[n].idleboot_msg, 0, sizeof(g_tab[n].idleboot_msg));
	g_tab[n].idlemod = DEF_IDLE_MOD;
}

/* initialize the entire group table */
void init_groups(void)
{
	int i;

	for (i=0; i<MAX_GROUPS; i++) {
		if ((g_tab[i].n_invites =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].n_invites, MAX_INVITES);
		if ((g_tab[i].nr_invites =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].nr_invites, MAX_INVITES);
		if ((g_tab[i].s_invites =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].s_invites, MAX_INVITES);
		if ((g_tab[i].sr_invites =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].sr_invites, MAX_INVITES);
		if ((g_tab[i].n_bars =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].n_bars, MAX_INVITES);
		if ((g_tab[i].n_nr_bars =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].n_nr_bars, MAX_INVITES);
		if ((g_tab[i].s_bars =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].s_bars, MAX_INVITES);
		if ((g_tab[i].s_nr_bars =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].s_nr_bars, MAX_INVITES);
		if ((g_tab[i].n_talk =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].n_talk, MAX_INVITES);
		if ((g_tab[i].nr_talk =
		   (NAMLIST *) malloc(sizeof(NAMLIST))) == NULL) {
			mdb(MSG_ERR, "Cannot init group table");
			exit(1);
		}
		nlinit(g_tab[i].nr_talk, MAX_INVITES);

		clear_group_item(i);
	}
}

/* find a slot in the group table with a particular name
 * return that index if found, -1 otherwise
 *
 * case insensitivoe
 *
 * name      groupname
 */

int find_group(char *name)
{
	int result = -1;
	int i;

	/* don't attempt to find if null */
	if (strlen(name) == 0)
		return result;

	/* search through table */
	for (i=0; i<MAX_GROUPS; i++) {
		if ( strcasecmp(g_tab[i].name, name) == 0) {
			result = i;
			break;
		}
	}
	return result;
}

/* find an empty slot in the group table
 * return that index if found, -1 otherwise 
 */
int find_empty_group(void)
{
	int result = -1;
	int i;

	for (i=0; i<MAX_GROUPS; i++) {
		if(strlen(g_tab[i].name) == 0) {
			result = i;
			break;
		}
	}
	return result;
}

/* check the group table to see if user was mod of any
 * return that index if so, -1 otherwise
 *
 * case insensitive
 *
 * u_index	index in user table of this user
 */

int check_mods(int u_index)
{
	int result = -1;
	int i;

	for (i=0; i<MAX_GROUPS; i++) {
		if (g_tab[i].mod == u_index) {
			result = i;
			break;
		}
	}
	return result;
}


/* fill a particular group entry
 * leave the invite list as it was 
 */
void fill_group_entry(int n, 
		const char *name, 
		const char *topic, 
		int visibility, 
		int control, 
		int mod, 
		int volume)
{
        strcpy( g_tab[n].name, name);
        strcpy( g_tab[n].topic, topic);

        g_tab[n].visibility = visibility;
        g_tab[n].control = control;
        g_tab[n].volume = volume;
        g_tab[n].mod = mod;
}


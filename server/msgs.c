/* Copyright (c) 1991 by John Atwood deVries II. */

/* handle various messages from the client */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/time.h>	/* for gettimeofday */

#include "server.h"
#include "externs.h"
#include "mdb.h"
#include "access.h"
#include "lookup.h"
#include "users.h"
#include "groups.h"
#include "send.h"
#include "namelist.h"
#include "strutil.h"
#include "s_commands.h"
#include "wildmat.h"
#include "s_stats.h"	/* for server_stats */

#ifndef	timersub
#define timersub(tvp, uvp, vvp)						\
        do {								\
                (vvp)->tv_sec = (tvp)->tv_sec - (uvp)->tv_sec;		\
                (vvp)->tv_usec = (tvp)->tv_usec - (uvp)->tv_usec;	\
                if ((vvp)->tv_usec < 0) {				\
                        (vvp)->tv_sec--;				\
                        (vvp)->tv_usec += 1000000;			\
                }							\
        } while (0)
#endif		/* !timersub */


long perm2val(char *str)
{
    long	perm = PERM_NULL;
    char	*cp;

    cp = strtok (str, "|");
    do
    {
	if ( !strcasecmp (cp, "perm_null") ) { perm |= PERM_NULL; }
	else if ( !strcasecmp (cp, "perm_deny") ) { perm |= PERM_DENY; }
	else if ( !strcasecmp (cp, "perm_slowmsgs") ) { perm |= PERM_SLOWMSGS; }
    } while ( (cp = strtok ((char *) NULL, "|")) != (char *)NULL );

    return (perm);
}

/* get_perms 
 *
 *   n          socket on which they sent the message
 *   login      login to check
 */
long get_perms (int n, char *login)
{
    struct sockaddr_in	rs;
    socklen_t			rs_size = sizeof(rs);
    static FILE		*perms_fp = (FILE *) NULL;
    char		pfilebuf[BUFSIZ];
    long		perm = PERM_NULL;
    char		*type;
    char 		*arg1;
    char		*arg2;
    char		*perms;


    if (getpeername(n, (struct sockaddr *)&rs, &rs_size) < 0)
    {
		sprintf(mbuf, "getpeername() failed: %s", strerror(errno));
		senderror(n, mbuf);
		mdb(MSG_ERR, mbuf);
		return (-1L);
    }

    /*
     * this really shouldn't be necessary, but for some reason
     * there is some strange case where this is being left open, perhaps
     * from a timer interrupting this function's ability to complete
     */
    if ( perms_fp != (FILE *) NULL )
    {
		fclose (perms_fp);
		perms_fp = (FILE *)NULL;
    }

    if ( (perms_fp = fopen (PERM_FILE, "r")) == (FILE *)NULL )
    {
		sprintf(mbuf, "Permissions File Open: %s", strerror(errno));
		mdb(MSG_ERR, mbuf);
		return (PERM_NULL);
    }

    while ( fgets (pfilebuf, BUFSIZ, perms_fp) != (char *) NULL )
    {
		if ( strchr("\n\t#", pfilebuf[0]) != (char *) NULL )
		{
			continue;
		}

		type = strtok (pfilebuf, " \t");
		if ( (arg1 = strtok ((char *)NULL, " \t")) == (char *)NULL )
			{ continue; }
		if ( (arg2 = strtok ((char *)NULL, " \t")) == (char *)NULL )
			{ continue; }
		if ( (perms = strtok ((char *)NULL, " \t\n")) == (char *)NULL )
			{ continue; }

		if ( !strcasecmp (type, "user") )
		{
			if ( !strcasecmp (arg1, login) &&
				!strcasecmp (arg2, u_tab[n].nodeid) )
			{
				perm = perm2val (perms);
				break;
			}
		}
		else if ( !strcasecmp (type, "mask") )
		{
			unsigned long addr, mask;

			addr = inet_addr(arg1);
			mask = inet_addr(arg2);

			if ( (rs.sin_addr.s_addr & mask) == addr )
			{
				perm = perm2val (perms);
				break;
			}
		}
    }

    fclose (perms_fp);
    perms_fp = (FILE *)NULL;

    return (perm);
}

/* open message
 *
 *  note that we need to know who sent the message.  it doesn't
 *  come with the sender's nick, so we have to reformat the message
 *  before sending it to the group
 *
 *   n          socket on which they sent the message
 *   pkt        packet buffer
 */

int u_recv_count[MAX_USERS];

void openmsg(int n, char *pkt)
{
	time_t TheTime;
	int gi;

        if (u_tab[n].login >= LOGIN_COMPLETE) {
		/* check to see if their away is set and pester them if so. */
		if (strlen(u_tab[n].awaymsg) > 0)
			sendstatus(n, "Away", "Your away is still set...");

        	TheTime = time(NULL);

		if ( !strcmp (u_tab[n].group, "1") )
		{
		    u_recv_count[n]++;
		    if ( u_tab[n].t_recv == TheTime )
		    {
			if ( u_recv_count[n] > 2 )
			{
			    extern int is_booting;

			    /* tell 'em it happened. */
			    sendstatus(n, "Boot", "No spamming in group 1.");
			    sprintf(mbuf,"%s was auto-booted for spamming.",
				    u_tab[n].nickname);
			    s_status_group(1,1,n, "Boot", mbuf);
			    /* fake an s_change to group ICB */
			    strcpy (fields[1], "ICB");
			    is_booting = 1;
			    s_change(n, 2);
			    is_booting = 0;
			}
		    }
		    else
		    {
			u_recv_count[n] = 0;
		    }
		}

	        /* record the time */
                u_tab[n].t_recv= TheTime;

		if (split(pkt) != 1) {
			mdb(MSG_WARN, "got bad open message packet");
		} else {
		   vmdb(MSG_INFO, "[OPEN] %d", n);

		   gi = find_group(u_tab[n].group);
		   if (gi < 0) {
			senderror(n, "You are not in a valid group.");
			return;
		   }
		   if (g_tab[gi].volume == QUIET)
			senderror(n,
			"Open messages not permitted in quiet groups.");
		    /*
		     * if the group is CONTROLLED,  and they
		     * aren't the moderator, check to make sure they are in
		     * in the talk list
		     */
		    else if ( (g_tab[gi].control == CONTROLLED
			&& g_tab[gi].mod != n)
		        && ( (nlpresent (u_tab[n].nickname,
			    *g_tab[gi].n_talk) == 0)
			&& (strlen (u_tab[n].realname) == 0
			    || nlpresent (u_tab[n].nickname,
				*g_tab[gi].nr_talk) == 0) ) )
		    {
			senderror (n,
	    "You do not have permission to send open messages in this group.");
			sprintf (mbuf,
			    "Use \"/m %s may i talk?\" to request permission.",
			    u_tab[g_tab[gi].mod].nickname);
			sendstatus (n, "INFO", mbuf);
		    }
		   else if (count_users_in_group(g_tab[gi].name) < 2)
			senderror(n,
			"No one else in group!");
		   else {
			s_send_group(n);
		   }
		}
	} else {
	    vmdb(MSG_INFO, "%d: cannot send open messages until logged in (%s)",
	      n, fields[0]);
	}
}


/*
 *  they sent us a login message. we need to put their info into 
 *  the user information table and then send them back a loginok message.
 *  for any field they didn't specify, come up with a plausible default.
 *  we also need to handle "w" vs. "login"
 *  we also need to dump them if they try using a:
 *	nickname already in use -or-
 *	group they aren't allowed to enter.
 *   their loginid may NOT be blank or null
 * 
 *   n          socket on which they sent the message
 *   pkt        packet buffer
 *
 *   returns 0 if everything went ok, -1 if it was an unknown login type
 */

int loginmsg(int n, char *pkt)
{
    int num_fields;
    char which_group[MAX_NICKLEN+4];
    char temp[MAX_NICKLEN+4];
    char one[255], two[255], three[255];
    int len;
    int ret;
    int how_many;
    int i, j;
    time_t TheTime;
    int target_user;
    char * cp;
    int access_file;
    char c;
    long perms = PERM_NULL;
    const char *password = "";

    if (u_tab[n].login > LOGIN_FALSE)
    {
		senderror(n, "Already logged in.");
		return 1;
    }

    u_tab[n].login = LOGIN_FALSE;
    /* just in case */
    TheTime = time(NULL);

    num_fields = split(pkt);

    if (num_fields < 4) return -1;
    if (num_fields >= 5 && fields[4] != NULL)
	password = fields[4];

    len = strlen(fields[2]);

    memset(temp, 0, MAX_NICKLEN+4);
    memset(which_group, 0, MAX_NICKLEN+4);

    if (len == 0) /* no group specified, give them one */
    {
		strcpy(which_group,"1");
    }
    else
    {
		how_many = (len > (MAX_NICKLEN+3))? (MAX_NICKLEN+3): len;
		strncpy(which_group, fields[2], how_many);
    }

    if ( strcasecmp(fields[3],"w") == 0)
    {
		sprintf(mbuf, "[WHO] %d: %s@%s", n, fields[0], u_tab[n].nodeid);
		mdb(MSG_INFO, mbuf);

		/* who only */
		fields[0][0] = 'w';
		fields[0][1] = '\000';
		fields[1][0] = '\000';

		s_who(n, 2);
		return -1; /* fail */
    }
    /*
     * this is basically the same as "w" above, but properly supports
     * passing a group name. some old clients erroneously default the
     * group to "1" which makes "icb -w" only list group 1. yes, it's
     * wrong, but we want to keep people happy
     */
    else if ( strcasecmp(fields[3],"wg") == 0)
    {
		sprintf(mbuf, "[WHO] %d: %s@%s", n, fields[0], u_tab[n].nodeid);
		mdb(MSG_INFO, mbuf);

		/* who only */
		fields[0][0] = 'w';
		fields[0][1] = '\000';

		/* if we got a groupname, use it */
		if ( num_fields >= 3 && fields[2][0] != '\0' )
			sprintf (fields[1], "%s%c", fields[2], 0);
		else
			fields[1][0] = '\000';

		s_who(n, 2);
		return -1; /* fail */
    }
    else if ( !strcasecmp (fields[3], "login") )
    {
		/* regular login */

		/* make sure the nickname hasn't already been taken */
		if (find_user(fields[1]) >= 0)
		{
			/* oops, someone already has this nick */
			senderror(n,"Nickname already in use.");
			return(-1);
		}

		/* make sure it isn't a null nickname */
		if ((strlen(fields[1]) == 0) || (strlen(fields[1]) > MAX_NICKLEN))
		{
			sprintf(mbuf, "Nickname must be between 1 and %d characters.",
				MAX_NICKLEN);
			senderror(n, mbuf);
			return -1;
		}

		/* make sure the length of the loginid isn't wrong */
		if ((strlen(fields[0]) == 0) || (strlen(fields[0]) > MAX_IDLEN))
		{
			sprintf(mbuf, "Login id must be between 1 and %d characters.",
			MAX_IDLEN);
			senderror(n, mbuf);
			return -1;
		}

		for (i = 0; i < strlen(fields[0]); i++) {
			if (isalnum(fields[0][i]) == 0)
			{
			senderror(n,
				"Login id must contain only alphanumeric characters.");
			return -1;
			}
		}

		/* This is set up in s_new_user now */
		cp = u_tab[n].nodeid;

		perms = get_perms (n, fields[0]);
		if ( perms < 0L )
		{
			return perms;
		}
		else
		{
			if ( perms & PERM_DENY )
			{
				sprintf (mbuf, "Login denied.");
				senderror(n, mbuf);
				mdb(MSG_INFO, mbuf);
				return (-1);
			}

			if ( perms != PERM_NULL )
			{
				mbuf[0] = '\0';
				if ( perms & PERM_SLOWMSGS ) { 
					strcat (mbuf, "slowmsgs");
				}
				sendstatus(n, "Permissions", mbuf);
			}
		}

		memset(one, 0, 255);
		memset(three, 0, 255);
		sprintf(one, "%s@%s", fields[0], cp);
		ucaseit(one);
		access_file = open(ACCESS_FILE, O_RDONLY);
		if (access_file >= 0)
		{
			while ((i = read(access_file, &c, 1)) > 0)
			{
				if (isspace(c))
				{
					ucaseit(three);
					if (wildmat(one, three))
					{
					sprintf (mbuf, "Login refused for %s", one);
					senderror(n, mbuf);
					mdb(MSG_INFO, mbuf);
					memset(two, 0, 255);
					while ((i = read(access_file, &c, 1)) > 0)
					{
						if (c == '\012') break;
						strncat(two, &c, 1);
					}
					sprintf (mbuf, "Reason: %s", two);
					senderror(n, mbuf);
					if (close(access_file) != 0)
					{
						sprintf(mbuf, "Access File Close: %s", strerror(errno));
						mdb(MSG_ERR, mbuf);
					}
					return -1;
					}
					memset(three, 0, 255);
				}
				else
				{
					strncat(three, &c, 1);
				}
			}

			if (close(access_file) != 0)
			{
			sprintf(mbuf, "Access File Close: %s", strerror(errno));
			mdb(MSG_ERR, mbuf);
			}
		}
		else
		{
			sprintf(mbuf, "Access File Open: %s", strerror(errno));
			mdb(MSG_ERR, mbuf);
		}
			
		/* get rid of nasty characters in the nickname */
		filternickname(fields[1]);

		/* fill in what we can */
		fill_user_entry(n, fields[0], cp, fields[1],
			password, "", "", LOGIN_FALSE, 0, 0, perms);

		sprintf(mbuf, "[LOGIN] %d: %s@%s", n, fields[0], cp);
		mdb(MSG_INFO, mbuf);

		/* make sure they are allowed to use this nickname */
		if (strcasecmp(fields[1],"admin") == 0)
		{
			if (!check_auth(n))
			{
			/* oops, password is wrong */
			senderror(n,"Nickname already in use.");
			return(-1);
			}
		}

		send_loginok(n);

		s_motd(n,2);

		if (which_group[0] == '@')
		{
			if ((target_user = find_user(&which_group[1])) < 0)
			{
			senderror (n,
				"User not found or is in a secret or invisible group.");
			return(-1);
			}
			else
			{
			int	g = find_group(u_tab[target_user].group);

			if ( ((g_tab[g].visibility == SUPERSECRET)
				|| (g_tab[g].visibility == SECRET))
				&& (strncmp("ADMIN", u_tab[n].nickname, MAX_NICKLEN)))
			{
				senderror (n,
				"User not found or is in a secret or invisible group.");
				return (-1);
			}
			else
				strcpy (which_group, u_tab[target_user].group);
			}
		}

		/* could find group slot, all is ok */
		/* fill in last members of table entry */
		/* strcpy(u_tab[n].group, which_group); */
		u_tab[n].login = LOGIN_PENDING;
		u_tab[n].t_on = TheTime;
		u_tab[n].t_recv = TheTime;
		u_tab[n].t_sent = TheTime;

		sprintf(mbuf, "Welcome to ICB %s", u_tab[n].nickname);
		sends_cmdout(n, mbuf);

		/* check to see if we know this person */
		ret = nicklookup(-n, u_tab[n].nickname, NULL);

		/* we know this person but they're not validated */
		if (ret == -2)
		{
			/*
			 * if password has a value in it, it's been passed from the
			 * client with the login message. if we aren't authenticated
			 * yet, try to.
			 */
			if ( u_tab[n].password[0] != '\0' )
			{
				if ( nickwrite (n, u_tab[n].password, 1, NULL) == 0 )
				{
					ret = 0;
				}
			}

			if ( ret != 0 )
			{
			sendstatus(n, "Register",
				"Send password to authenticate your nickname.");
			j = 0;

			for (i = 0; i < MAX_GROUPS; i++)
				if (strcasecmp(u_tab[n].nickname, g_tab[i].missingmod) == 0)
				{
				sprintf(mbuf, "You are moderator of group %s",
					g_tab[i].name);
				sendstatus(n, "Mod", mbuf);
				j++;
				}

			if (j == 1)
				sendstatus(n, "Mod",
		"You must register using /p <password> to regain mod of the above group.");

			if (j > 1)
				sendstatus(n, "Mod",
		"You must register using /p <password> to regain mod of the above groups.");
			}
		}

		if (ret == 0)
		{
			/* we know this person */
			if ( strcmp (u_tab[n].realname, "registered") )
			{
			strcpy(u_tab[n].realname, "registered");
			sendstatus(n, "Register", "Nick registered");
			nickwritetime(n, 0, NULL);

			for (i = 1; i < MAX_GROUPS; i++)
				if ((g_tab[i].modtimeout > 0.0) &&
				(strcmp(g_tab[i].missingmod, u_tab[n].nickname)==0))
				{
					g_tab[i].modtimeout = 0;
					g_tab[i].mod = n;
					memset(g_tab[i].missingmod, 0, MAX_NICKLEN);
					sprintf(mbuf, "%s is the active moderator again.",
					u_tab[n].nickname);
					for (j = 1; j < MAX_REAL_USERS; j++)
					if ((strcasecmp(u_tab[j].group, g_tab[i].name) == 0)
						&& (j != n))
						sendstatus(j, "Mod", mbuf);
					sprintf(mbuf, "You are the moderator of group %s",
					g_tab[i].name);
					sendstatus(n, "Mod", mbuf);
				}

			if ((i = nickckmsg(n, NULL)) > 0) {
				if (i == 1)
				sendstatus(n, "Message", "You have 1 message");
				else
				{
				sprintf(mbuf, "You have %d messages", i);
				sendstatus(n, "Message", mbuf);
				}
			}
			}
		}
		else if ( ret != -2 )
		{
			sendstatus(n, "No-Pass", "Your nickname does not have a password.");
			sendstatus(n, "No-Pass", "For help type /m server ?");
		}

		memset(one, 0, 255);
		sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
		ucaseit(one);
		for (i = 0; i < MAX_GROUPS; i++)
		{
			if (
			   (nlpresent(u_tab[n].nickname, *g_tab[i].n_invites) > 0) ||
			   (nlmatch(one, *g_tab[i].s_invites)) ||
			   (nlpresent(u_tab[n].nickname, *g_tab[i].nr_invites) &&
			  (strlen(u_tab[n].realname) > 0)) ||
			   (nlmatch(one, *g_tab[i].sr_invites) && 
			  (strlen(u_tab[n].realname) > 0)))
			{
			sprintf (mbuf, "Invited to: %s", g_tab[i].name);
			sends_cmdout(n, mbuf);
			}
		}
			
		/*
		 * this is also done in nickwrite(), so only run it again
		 * if that did not run or did not succeed
		 */
		if ( strcmp (u_tab[n].realname, "registered") )
		{
			for (i = 0; i < MAX_GROUPS; i++)
			talk_report (n, i);
		}

		/* fake a s_change */
		strcpy(fields[1], which_group);
		if (s_change(n,3) < 0)
		{
			/* login fails because can't get into that group */
			return(-1);
		}

		/* we've finally done the group change (s_change) */
		u_tab[n].login = LOGIN_COMPLETE;

		server_stats.signons++;

		memset(one, 0, 255);
		memset(two, 0, 255);
		memset(three, 0, 255);
		sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
		ucaseit(one);
		strcpy(three, u_tab[n].nickname);
		ucaseit(three);
		for (j = 0; j < MAX_USERS; j++)
			if ((n != j) && (u_tab[j].login >= LOGIN_COMPLETE))
			{
			if ((nlmatch(three, *u_tab[j].n_notifies) ||
			   nlmatch(one, *u_tab[j].s_notifies)) && 
			   (g_tab[find_group(u_tab[n].group)].visibility !=
				  SUPERSECRET))
			{
				sprintf (two, "%s (%s) has just signed on",
				u_tab[n].nickname, one);
				sendstatus(j, "Notify-On", two);
			}
			}

		return 0;
    }
    else
    {
		sprintf (mbuf, "Unknown login type \"%s\"", fields[3]);
		senderror (n, mbuf);
		return -1;
    }
}


/* command message
 * 
 *   n          socket on which they sent the message
 *   pkt        packet buffer
 */

void cmdmsg(int n, char *pkt)
{
	int argc;
	time_t TheTime;
	const char *arg1;

	if (u_tab[n].login >= LOGIN_COMPLETE) 
	{
                /* record the time */
                TheTime = time(NULL);
                u_tab[n].t_recv= TheTime;

		argc = split(pkt);
		arg1 = (argc > 1 && fields[1] != NULL) ? fields[1] : "";

		if (strcmp(fields[0], "m") == 0)
		  sprintf(mbuf, "[COMMAND] %d: %s %s", n, fields[0],
			getword((char *)arg1));
		else
		  sprintf(mbuf, "[COMMAND] %d: %s %s", n, fields[0], arg1);
		mdb(MSG_DEBUG, mbuf);
	
		switch(lookup(fields[0], command_table)) {

			case CMD_DROP:
				s_drop(n, argc);
				break;

			case CMD_RESTART:
				s_restart(n, argc);
				break;
			
			case CMD_SHUTDOWN:
				s_shutdown(n, argc);
				break;

			case CMD_WALL:
				s_wall(n, argc);
				break;

		 	case CMD_BEEP:
				s_beep(n, argc);
				break;

			case CMD_CANCEL:
				s_cancel(n, argc);
				break;

			case CMD_G:	
				s_change(n, argc);
				break;

			case CMD_INVITE:
				s_invite(n, argc);
				break;

			case CMD_PASS:
				s_pass(n, argc);
				break;

			case CMD_BOOT:
				s_boot(n, argc);
				break;

			case CMD_STATUS:
				s_status(n, argc);
				break;

			case CMD_TOPIC:
				s_topic(n, argc);
				break;

			case CMD_MOTD:
				s_motd(n, argc);
				break;

			case CMD_M:
				s_personal(n, argc);
				break;

			case CMD_ECHOBACK:
				s_echoback(n, argc);
				break;

			case CMD_NAME:
				s_name(n, argc);
				break;

			case CMD_V:
				s_version(n, argc);
				break;

			case CMD_W:
				s_who(n, argc);
				break;

			case CMD_INFO:
				s_info(n, argc);
				break;

			case CMD_NEWS:
				s_news(n, argc);
				break;

			case CMD_HUSH:
			case CMD_SHUSH:
				s_hush(n, argc);
				break;

			case CMD_HELP:
				s_help(n, argc);
				break;

			case CMD_EXCLUDE:
				s_exclude(n, argc);
				break;

			case CMD_SHUTTIME:
				s_shuttime(n, argc);
				break;

			case CMD_NOTIFY:
				s_notify(n, argc);
				break;

			case CMD_PING:
				s_ping(n, argc);
				break;

			case CMD_TALK:
				s_talk(n, argc);
				break;

			case CMD_NOBEEP:
				s_nobeep(n, argc);
				break;

			case CMD_AWAY:
				s_away(n, argc);
				break;

			case CMD_NOAWAY:
				s_noaway(n, argc);
				break;

			case CMD_LOG:
				s_log(n, argc);
				break;

			case CMD_STATS:
				s_server_stats(n, argc);
				break;

		default:
			if (lookup(fields[0], auto_table) >= 0)
				s_auto(n, 0);
			else {
				sendstatus(n, "Server", "Unknown command");
				sprintf(mbuf,
			 		"%d: Invalid command \"%s\"",
					n, fields[0]);
				mdb(MSG_INFO, mbuf);
				}
		}
	} 
	else 
	{
		sprintf (mbuf,
		    "%d: cannot issue command until logged in (%s)",
		    n, fields[0]);
		mdb(MSG_INFO, mbuf);
	}
}



/* pong message
 *
 *   n          socket on which they sent the message
 *   pkt        packet buffer
 */

void pong(int n, char *pkt)
{
	char tbuf[255];

        if (u_tab[n].login >= LOGIN_COMPLETE) {
		struct timeval now;
		gettimeofday (&now, NULL);
		if ( pong_req[n] != -1 )
		{
		    struct timeval diff;

		    timersub (&now, &ping_time[n], &diff);
		    sprintf (tbuf, "%s client responded in %ld.%02ld seconds.",
			u_tab[n].nickname, (long)diff.tv_sec, (long)diff.tv_usec / 10000);
		    sendstatus (pong_req[n], "PONG", tbuf);
		    pong_req[n] = -1;
		    timerclear (&ping_time[n]);
		}
		else
		{
		    sendstatus (n, "PONG", "You were pinged, but they aren't there to receive your pong.");
		}
	} else {
		 mdb(MSG_INFO, "cannot send pong message until signed on");
	}
}


/*
 * ok2read()
 * - this function is called every time a socket is ready to read some data.
 *   therefore, here is where we apply the SLOWMSGS permissions so that
 *   certain connections' data is read less frequently than normal
 */
int ok2read(int n)
{
    if ( u_tab[n].perms & PERM_SLOWMSGS )
    {
	time_t TheTime;

	TheTime = time(NULL);

	/*
	 * if the current time - the last time they sent something
	 * is less than 1 second, ignore 'em
	 */
	if ( (TheTime - u_tab[n].t_recv) < 1 )
	{
	    return (0);
	}


	return (1);
    }

    return (1);
}


/* copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to change user info */

#include "config.h"

#include <unistd.h>
#include <errno.h>
#include <string.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>    /* for gettimeofday & struct tm */
#endif

#include "server.h"
#include "externs.h"
#include "protocol.h"
#include "mdb.h"
#include "send.h"
#include "strutil.h"
#include "namelist.h"
#include "access.h"
#include "users.h"
#include "s_commands.h"
#include "unix.h"

int s_help(int n, int argc)
{
    int help_fd;
    char c;
    int i;
    char tbuf[256];

    help_fd = open(ICBDHELP,O_RDONLY);

    /* if the file is there, list it, otherwise report error */
    if (help_fd >= 0) {
        memset(tbuf, 0, 255);
        while ((i = read(help_fd, &c, 1)) > 0) {
            if (c == '\012') {
                sends_cmdout(n, tbuf);
                memset(tbuf, 0, 255);
            }
            else {
                size_t len = strlen(tbuf);
                if (len < sizeof(tbuf) - 1) {
                    tbuf[len] = c;
                    tbuf[len + 1] = '\0';
                }
            }
        }
        if (close(help_fd) != 0) {
            sprintf(mbuf, "Help File Close: %s",
                    strerror(errno));
            mdb(MSG_ERR, mbuf);
        }
    } else {
        sprintf(mbuf, "Help File Open: %s",
                strerror(errno));
        mdb(MSG_ERR, mbuf);
        senderror(n, "No help file found.");
    }
    return 0;
}

int s_hush(int n, int argc)
{
    int i;
    int personal = -1;
    int public = -1;
    int mode = -1;
    int quiet = -1;
    char *flags;
    char *who = NULL;

    if (argc >= 2)
        who = fields[1];
    else
        who = (char *) NULL;

    if ( who != (char *)NULL )
    {
        while (*who == '-')
        {
            flags = getword(who);
            for (i = 1; i < strlen(flags); i++)
            {
                switch(flags[i])
                {
                    case 'q':
                        if (quiet == -1)
                            quiet = 1;
                        else
                        {
                            senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'n':
                        if (mode == -1)
                            mode = 0;
                        else
                        {
                            senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 's':
                        if (mode == -1)
                            mode = 1;
                        else
                        {
                            senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'p':
                        if (personal == -1)
                            personal = 1;
                        else
                        {
                            senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'o':
                        if (public == -1)
                            public = 1;
                        else
                        {
                            senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    default:
                        senderror(n, "Usage: hush {-q} {-p} {-o} {-n nickname | -s address}");
                        return -1;
                        break;
                }
            }
            who = get_tail(who);
        }

        if (quiet == -1) quiet = 0;

        if (mode == -1) mode = 0;

        if ((personal == -1) && (public == -1))
        {
            personal = 1;
            public = 1;
        }

        if (strlen(who) == 0)
            who = (char *) NULL;
        else
            ucaseit (who);
    }

    /* no arg means just list hushed people and return */
    if (who == (char *)NULL)
    {
        if ((nlcount(*u_tab[n].pri_n_hushed) == 0) &&
            (nlcount(*u_tab[n].pub_n_hushed) == 0) &&
            (nlcount(*u_tab[n].pri_s_hushed) == 0) &&
            (nlcount(*u_tab[n].pub_s_hushed) == 0))
            sendstatus(n, "Hush-List", "Empty List");
        else {
            if (nlcount(*u_tab[n].pri_n_hushed) > 0)
                for (i = 0; i < nlcount(*u_tab[n].pri_n_hushed); i++) 
                    sendstatus(n, "Personal-Nick-Hushed", nlget(u_tab[n].pri_n_hushed));
            if (nlcount(*u_tab[n].pub_n_hushed) > 0)
                for (i = 0; i < nlcount(*u_tab[n].pub_n_hushed); i++) 
                    sendstatus(n, "Open-Nick-Hushed", nlget(u_tab[n].pub_n_hushed));
            if (nlcount(*u_tab[n].pri_s_hushed) > 0)
                for (i = 0; i < nlcount(*u_tab[n].pri_s_hushed); i++) 
                    sendstatus(n, "Personal-Site-Hushed", nlget(u_tab[n].pri_s_hushed));
            if (nlcount(*u_tab[n].pub_s_hushed) > 0)
                for (i = 0; i < nlcount(*u_tab[n].pub_s_hushed); i++) 
                    sendstatus(n, "Open-Site-Hushed", nlget(u_tab[n].pub_s_hushed));
        }

        return 0;
    }

    if (mode == 1) { /* site addr */
        if (personal == 1) {
            if (nlpresent(who, *u_tab[n].pri_s_hushed) == 0) {
                if (nlcount(*u_tab[n].pri_s_hushed) == MAX_HUSHED) {
                    senderror(n, "Max number of site personal hushes reached.");
                    return -1;
                }
                nlput(u_tab[n].pri_s_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s added to site personal hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
            else {
                nldelete(u_tab[n].pri_s_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s removed from site personal hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
        }
        if (public == 1) {
            if (nlpresent(who, *u_tab[n].pub_s_hushed) == 0) {
                if (nlcount(*u_tab[n].pub_s_hushed) == MAX_HUSHED) {
                    senderror(n, "Max number of site open hushes reached.");
                    return -1;
                }
                nlput(u_tab[n].pub_s_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s added to site open hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
            else {
                nldelete(u_tab[n].pub_s_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s removed from site open hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
        }
    }
    else { /* nickname */
        if (personal == 1) {
            if (nlpresent(who, *u_tab[n].pri_n_hushed) == 0) {
                if (nlcount(*u_tab[n].pri_n_hushed) == MAX_HUSHED) {
                    senderror(n, "Max number of nickname personal hushes reached.");
                    return -1;
                }
                nlput(u_tab[n].pri_n_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s added to nickname personal hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
            else {
                nldelete(u_tab[n].pri_n_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s removed from nickname personal hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
        }
        if (public == 1) {
            if (nlpresent(who, *u_tab[n].pub_n_hushed) == 0) {
                if (nlcount(*u_tab[n].pub_n_hushed) == MAX_HUSHED) {
                    senderror(n, "Max number of nickname open hushes reached.");
                    return -1;
                }
                nlput(u_tab[n].pub_n_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s added to nickname open hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
            else {
                nldelete(u_tab[n].pub_n_hushed, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s removed from nickname open hush list.", who);
                    sendstatus(n, "Hush", mbuf);
                }
            }
        }
    }
    return 0;
}

int s_name(int n, int argc)
{
    int len, i, j;
    int how_many;
    char new_name[MAX_NICKLEN+1];
    int ret;

    if (argc == 2) {
        /* constraints:
           will only use the first max_nicklen letters of
           a nickname.
           nickname must be at least 1 character long.
           to use the nickname admin, must have password
           set to ADMIN_PWD
         */

        /* only use so much of it, but at least some of it */
        len = strlen(fields[1]);
        if (len <= 0) {
            /* oops, too short */
            senderror(n,"The nickname may not be null.");
            return(-1);
        }

        how_many = (MAX_NICKLEN > len) ? len:MAX_NICKLEN;
        memset(new_name, 0, MAX_NICKLEN+1);
        strncpy(new_name, fields[1], how_many);
        filternickname(new_name);

        /* make sure the nickname hasn't already been taken */

        if ((find_user(new_name) >= 0) &&
            (strcasecmp(new_name, u_tab[n].nickname) != 0)) {
            /* oops, someone already has this nick */
            senderror(n,"Nickname already in use.");
            return(-1);
        }

        /* make sure they are allowed to use this nickname */
        if (strcasecmp(new_name,"admin") == 0) {
            if (strcmp(u_tab[n].password, ADMIN_PWD) != 0) {
                /* oops, password is wrong */
                senderror(n,"Nickname already in use.");
                return(-1);
            }
        }

        sprintf(mbuf,"%s changed nickname to %s",
                u_tab[n].nickname, new_name);
        s_status_group(1,0,n,"Name",mbuf);
        nickwritetime(n, 1, NULL);
        strcpy(u_tab[n].nickname, new_name);



        /* side-effects:
           inform folx in same group of change
           if they are registered, set their real name.
         */
        sendstatus(n,"Name",mbuf);

        /* check to see if we know this person */
        memset(u_tab[n].realname, 0, MAX_REALLEN+1);
        ret = nicklookup(-n, u_tab[n].nickname, NULL);
        if (ret == 0) {
            /* we know this person */
            strcpy(u_tab[n].realname, "registered");
            sendstatus(n, "Register", "Nick registered");
            nickwritetime(n, 0, NULL);
            for (i = 1; i < MAX_GROUPS; i++)
                if ((g_tab[i].modtimeout > 0.0) &&
                    (strcmp(g_tab[i].missingmod, u_tab[n].nickname)==0)){
                    g_tab[i].modtimeout = 0;
                    g_tab[i].mod = n;
                    memset(g_tab[i].missingmod, 0, MAX_NICKLEN);
                    sprintf(mbuf, "%s is the active moderator again.",
                            u_tab[n].nickname);
                    for (j = 1; j < MAX_REAL_USERS; j++)
                        if ((strcasecmp(u_tab[j].group, g_tab[i].name)
                             == 0) && (j != n))
                            sendstatus(j, "Mod", mbuf);
                    sprintf(mbuf, "You are the moderator of group %s",
                            g_tab[i].name);
                    sendstatus(n, "Mod", mbuf);
                }
            if ((i = nickckmsg(n, NULL)) > 0) {
                if (i == 1)
                    sendstatus(n, 
                               "Message", "You have 1 message");
                else {
                    sprintf(mbuf,"You have %d messages", i);
                    sendstatus(n, "Message", mbuf);
                }
            }

        } 
        else if (ret == -2) 
            /* we know this person but they're not validated */
        {
            sendstatus(n, "Register", "Send password to authenticate your nickname.");
            j = 0;
            for (i = 0; i < MAX_GROUPS; i++)
                if (strcasecmp(u_tab[n].nickname, g_tab[i].missingmod) == 0) {
                    sprintf(mbuf, "You are moderator of group %s", g_tab[i].name);
                    sendstatus(n, "Mod", mbuf);
                    j++;
                }
            if (j == 1) sendstatus(n, "Mod", "You must register using /p <password> to regain mod of the above group.");
            if (j > 1) sendstatus(n, "Mod", "You must register using /p <password> to regain mod of the above groups.");
        }
        else {
            /* we don't know this person */
            memset(u_tab[n].realname, 0, MAX_REALLEN + 1);
            sendstatus(n, "No-Pass", "To register your nickname type /m server p mypassword");
        }

        for (i = 0; i < MAX_GROUPS; i++)
            talk_report (n, i);

    } else {
        mdb(MSG_INFO, "name: wrong number of parz");
    }
    return 0;
}

int s_notify(int n, int argc)
{
    char *who, *flags;
    int quiet = -1;
    int mode = -1;
    int i;

    if ( argc >= 2 )
        who = fields[1];
    else
        who = (char *)NULL;

    if ( who != (char *) NULL )
    {
        while (who[0] == '-')
        {
            flags = getword(who);
            for (i = 1; i < strlen(flags); i++)
            {
                switch(flags[i])
                {
                    case 'q':
                        if (quiet == -1)
                            quiet = 1;
                        else
                        {
                            senderror(n, "Usage: notify {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'n':
                        if (mode == -1)
                            mode = 0;
                        else
                        {
                            senderror(n, "Usage: notify {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 's':
                        if (mode == -1)
                            mode = 1;
                        else
                        {
                            senderror(n, "Usage: notify {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    default:
                        senderror(n, "Usage: notify {-q} {-n nickname | -s address}");
                        return -1;
                        break;
                }
            }
            who = get_tail(who);
        }

        if (strlen(who) == 0)
            who = (char *)NULL;
        else
            ucaseit(who);
    }

    /* no args means just list current notifies and return */
    if (who == (char *)NULL)
    {
        if ((nlcount(*u_tab[n].n_notifies) == 0) &&
            (nlcount(*u_tab[n].s_notifies) == 0))
        {
            sendstatus(n, "Notify-List", "Empty List");
        }
        else
        {
            if (nlcount(*u_tab[n].n_notifies) > 0)
            {
                for (i = 0; i < nlcount(*u_tab[n].n_notifies); i++)
                    sendstatus(n, "Notify-Nickname", nlget(u_tab[n].n_notifies));
            }

            if (nlcount(*u_tab[n].s_notifies) > 0)
            {
                for (i = 0; i < nlcount(*u_tab[n].s_notifies); i++)
                    sendstatus(n, "Notify-Site", nlget(u_tab[n].s_notifies));
            }
        }
        return 0;
    }

    if (mode == -1) mode = 0;
    if (quiet == -1) quiet = 0;

    if (mode == 1)
    {
        if (nlpresent(who, *u_tab[n].s_notifies) == 0)
        {
            if (nlcount(*u_tab[n].s_notifies) == MAX_NOTIFIES)
            {
                senderror(n, "Max number of site notifies reached.");
                return -1;
            }
            nlput(u_tab[n].s_notifies, who);
            if (quiet == 0)
            {
                sprintf(mbuf, "%s added to site notify list.", who);
                sendstatus(n, "Notify", mbuf);
            }
        }
        else
        {
            nldelete(u_tab[n].s_notifies, who);
            if (quiet == 0)
            {
                sprintf(mbuf, "%s removed from site notify list.", who);
                sendstatus(n, "Notify", mbuf);
            }
        }
    }
    else 
    {
        if (nlpresent(who, *u_tab[n].n_notifies) == 0)
        {
            if (nlcount(*u_tab[n].n_notifies) == MAX_NOTIFIES)
            {
                senderror(n,"Max number of nickname notifies reached.");
                return -1;
            }
            nlput(u_tab[n].n_notifies, who);
            if (quiet == 0)
            {
                sprintf(mbuf, "%s added to nickname notify list.", who);
                sendstatus(n, "Notify", mbuf);
                if ((i = find_user(who)) > 0)
                {
                    sprintf(mbuf, "%s is logged in now.", u_tab[i].nickname);
                    sendstatus(n, "Notify", mbuf);
                }
            }
        }
        else
        {
            nldelete(u_tab[n].n_notifies, who);
            if (quiet == 0)
            {
                sprintf(mbuf, "%s removed from nickname notify list.", who);
                sendstatus(n, "Notify", mbuf);
            }
        }
    }
    return 0;
}

int s_echoback(int n, int argc)
{
    if (argc == 2) {
        if(strcasecmp(fields[1],"off") == 0) {
            u_tab[n].echoback = 0; /* off */
            sendstatus(n, "Echo","Echoback off");
        } else if (strcasecmp(fields[1],"on") == 0) {
            u_tab[n].echoback = 1; /* on */
            sendstatus(n,"Echo","Echoback on");
        } else if (strcasecmp(fields[1],"verbose") == 0) {
            u_tab[n].echoback = 2; /* verbose */
            sendstatus(n,"Echo","Echoback on verbose");
        } else {
            senderror(n,"Echoback: needs on/off/verbose");
        }
    } else {
        mdb(MSG_INFO, "echoback: wrong number of parz");
    }
    return 0;
}

int s_nobeep(int n, int argc)
{
    if (argc == 2) {
        if(strcasecmp(fields[1],"off") == 0) {
            u_tab[n].nobeep = 0; /* off */
            sendstatus(n, "No-Beep","No-Beep off");
        } else if (strcasecmp(fields[1],"on") == 0) {
            u_tab[n].nobeep = 1; /* on */
            sendstatus(n,"No-Beep","No-Beep on");
        } else if (strcasecmp(fields[1],"verbose") == 0) {
            u_tab[n].nobeep = 2; /* verbose */
            sendstatus(n,"No-Beep","No-Beep on (verbose)");
        } else {
            senderror(n,"No-Beep: needs off/on/verbose");
        }
    } else {
        mdb(MSG_INFO, "No-Beep: wrong number of parz");
    }
    return 0;
}

int s_ping (int n, int argc)
{
    char *p;
    int sendto = n;    /* default ping yourself */

    if ( argc != 2 )
    {
        mdb(MSG_INFO, "ping: wrong number of parz");
        return (-1);
    }

    p = getword (fields[1]);

    if ( p && *p )
    {
        int i;

        if ( (i = find_user(p)) > 0 )
        {
            sendto = i;
        }
        else
        {
            sprintf (mbuf, "Can't locate user (%s)", p);
            senderror (n, mbuf);
            return (-1);
        }
    }

    gettimeofday (&ping_time[sendto], NULL);
    pong_req[sendto] = n;

    sendping (sendto, u_tab[n].nickname);
    return 0;
}

int s_noaway(int n, int argc)
{
    if (argc == 0 || argc > 2)
    {
        mdb(MSG_INFO, "NoAway: wrong number of parz");
        return -1;
    }

    if (strlen(u_tab[n].awaymsg) == 0)
    {
        sendstatus(n, "Away", "Away not set!");
        return 0;
    }

    *(u_tab[n].awaymsg) = '\0';
    u_tab[n].lastaway = 0;
    u_tab[n].lastawaytime = (time_t)0;
    sendstatus(n, "Away", "Away message unset.");
    return 0;
}

int s_away(int n, int argc)
{
    struct tm *t;
    char  *retstr;
    int num_s = 1;    /* how many %s can be in awaymsg */

    if (argc == 0 || argc > 2)
    {
        mdb(MSG_INFO, "Away: wrong number of parz");
        return -1;
    }

    if ( strlen (fields[1]) == 0 )
    {
        if ( strlen (u_tab[n].awaymsg) > 0 )
        {
            sprintf (mbuf, "Away message is set to \"%s\"",
                     u_tab[n].awaymsg);
            sendstatus (n, "Away", mbuf);
        }
        else
            sendstatus(n, "Away", "Away message is not set.");
        return 0;
    }

    /* make sure there aren't more than 1 %s subs */
    num_s = 1;
    if ( (retstr = filterfmt (fields[1], &num_s)) != (char *)NULL )
    {         
        senderror (n, retstr);
        return 0;
    }         

    gettime();
    t = localtime(&curtime);
    snprintf(u_tab[n].awaymsg, MAX_AWAY_LEN, "(%%s@%d:%02d%s) %s", 
             (t->tm_hour>12) ? (t->tm_hour-12) : ((t->tm_hour==0) ? 12 : t->tm_hour),
             t->tm_min,
             t->tm_hour > 11 ? "pm" : "am",
             fields[1]);
    snprintf(mbuf, MSG_BUF_SIZE, "Away message set to \"%s\"",
             u_tab[n].awaymsg);
    u_tab[n].lastaway = 0;
    u_tab[n].lastawaytime = (time_t)0;

    sendstatus(n, "Away", mbuf);
    return 0;
}

#if BRICK
/*
 * From Scott Reynold's hacked ICB.
 */
int
s_brick(int n, int argc)
{
	char    target[MAX_NICKLEN + 1];
	char	line[MAX_INPUTSTR];
	int     t;
	int		bricks;

	if (argc == 2) {
		strncpy(target, fields[1], MAX_NICKLEN+1);
		target[sizeof(target) - 1] = '\0';
		filternickname(target);
		t = find_user(target);
	} else {
		target[0] = '\0';
		t = (-1);
	}

	bricks = u_tab[n].bricks;
	if (t < 0 && strcmp(target, "") == 0) {
		if (bricks > 0) {
			snprintf(line, MAX_INPUTSTR, "You have %d brick%s remaining.",
					bricks, bricks == 1 ? "" : "s");
		} else if (bricks < 0) {
			snprintf(line, MAX_INPUTSTR, "You owe %d bricks.", (-bricks));
		} else {
			snprintf(line, MAX_INPUTSTR, "You have no bricks remaining.");
		}
		sendstatus(n, "Message", line);
		return 0;
	}

	// decrement a brick from the sender
	bricks = --u_tab[n].bricks;
	if (bricks < (-5)) {
		senderror(n, "You are out of bricks.  Good bye.");
		snprintf(line, MAX_INPUTSTR, "out of bricks; dropped %s (%d)",
				u_tab[n].nickname, n);
		s_status_group(1, 1, n, "DROP", line);
		S_kill[n]++;
	} else if (bricks < (-4)) {
		snprintf(line, MAX_INPUTSTR, "%s has fallen, and can't get up.",
				u_tab[n].nickname);
		s_status_group(1, 1, n, "FYI", line);
		senderror(n, "You are out of bricks.  Please desist.");
	} else if (bricks < 0) {
		senderror(n, "You are out of bricks.");
	} else if (t < 0 || strcasecmp(target, "server") == 0) {
		s_status_group(1, 1, n, "FYI",
				"A brick flies off into the ether.");
	} else {
		// add the brick to the target
		if (u_tab[t].bricks < MAX_BRICKS && t != n) {
			u_tab[t].bricks++;
		}
		snprintf(line, MAX_INPUTSTR, "%s has been bricked.", target);
		s_status_group(1, 1, n, "FYI", line);
		if (strcasecmp(u_tab[n].group, u_tab[t].group) != 0) {
			s_status_group(1, 1, t, "FYI", line);
		}
	}
	return 0;
}
#endif /* BRICK */


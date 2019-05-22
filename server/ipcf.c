/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <string.h>

#include "server.h"
#include "externs.h"
#include "dispatch.h"
#include "mdb.h"
#include "icbutil.h"
#include "send.h"
#include "groups.h"
#include "strutil.h"
#include "namelist.h"
#include "users.h"
#include "s_commands.h"
#include "s_stats.h"    /* for server_stats */

#include "murgil/murgil.h" /* for disconnectuser */

void c_packet(char *pkt)
{
    /* the server doesn't handle client packets */
    mdb(MSG_INFO, "server erroneously got client packet");
}


void c_didpoll(void)
{
    /* do interesting nothing for now */
    mdb(MSG_INFO, "server erroneously got client didpoll call");
}


void c_lostcon(void)
{
    mdb(MSG_INFO, "server erroneously got client lost connection packet");
}

void s_didpoll(int n)
{
    int i, j, newmod;
    long TheTime;
    extern int is_booting;

    if (n == 1) {
        /* do nothing on the frequent polls */
    } else {
        TheTime = time(NULL);
        if ((TheTime >= TimeToDie) && (TimeToDie > 0.0))
            icbexit(0);

        if (((TheTime >= (TimeToDie - 300)) && (TimeToDie > 0.0)) &&
            (ShutdownNotify == 0)) {
            for (i = 0; i < MAX_REAL_USERS; i++) {
                if (u_tab[i].login > LOGIN_FALSE)
                    sendimport(i, "Shutdown",
                               "Server shutting down in 5 minutes!");
            }
            ShutdownNotify++;
        }

        if (((TheTime >= (TimeToDie - 60)) && (TimeToDie > 0.0)) &&
            (ShutdownNotify == 1)) {
            for (i = 0; i < MAX_REAL_USERS; i++) {
                if (u_tab[i].login > LOGIN_FALSE)
                    sendimport(i, "Shutdown",
                               "Server shutting down in 1 minute!");
            }
            ShutdownNotify++;
        }

        for(i=0;i<MAX_REAL_USERS;i++) {
            if ( u_tab[i].login > LOGIN_FALSE ) {
                if (S_kill[i] > 0) {
                    sprintf(mbuf, "[KILL] killing %d (%d)", i, S_kill[i]);
                    mdb(MSG_INFO, mbuf);
                    server_stats.drops++;
                    disconnectuser(i);
                }
            }
        }

        for(i=0;i<MAX_GROUPS;i++)
        {
            /* look at non-public groups w/ mods for mods who are idle */
            if ( g_tab[i].control != PUBLIC && g_tab[i].mod >= 0 )
            {
                int mod = g_tab[i].mod;
                int mod_idle = TheTime - u_tab[mod].t_recv;

                /* if the group's mod is too idle, check further */
                if ( mod_idle > g_tab[i].idlemod )
                {
                    int ui;

                    /*
                     * check to make sure that there are other users
                     * in this group who are less idle than the mod
                     * and not away.
                     * this code is a particularly fine example of 
                     * just how ineffeciently this server is architected
                     */

                    for ( ui = 0; ui < MAX_REAL_USERS; ui++ )
                    {
                        if ( (ui != mod)
                             && (u_tab[ui].login >= LOGIN_COMPLETE)
                             && (u_tab[ui].awaymsg[0] == '\0')
                             && (find_group (u_tab[ui].group) == i)
                             && ((TheTime - u_tab[ui].t_recv) < mod_idle) )
                        {
                            break;
                        }
                    }

                    /* reliquish that group's mod */
                    if ( ui < MAX_REAL_USERS )
                    {
                        const char *modmsg = IDLE_MOD_MSG;

                        /* let them know */
                        sprintf (mbuf, modmsg, "you", g_tab[i].name);
                        sendstatus(mod, "Idle-Mod", mbuf);

                        /* let their group know */
                        if (g_tab[i].volume != QUIET)
                        {
                            sprintf (mbuf, modmsg, u_tab[mod].nickname,
                                     g_tab[i].name);

                            /* if the mod is in that group, send the 
                             * message to to all in the group but the mod
                             */
                            if ( find_group (u_tab[mod].group) == i )
                                s_status_group (1, 0, mod, "Idle-Mod", mbuf);
                            else
                                /* otherwise send to all in the group */
                                s_status_group (2, 0, i, "Idle-Mod", mbuf);
                        }

                        /* guaranteed to trigger the below assignment of new mod */
                        g_tab[i].modtimeout = 1;
                        server_stats.idlemods++;
                    }
                }
            }

            /*
             * if the modtimeout has passed, select the least-idle 
             * user in the group to pass mod to
             */
            if ((g_tab[i].modtimeout > 0.0) &&
                (g_tab[i].modtimeout < TheTime))
            {
                newmod = -1;
                for (j=0; j<MAX_USERS; j++)
                {
                    if ((u_tab[j].login >= LOGIN_COMPLETE)
                        && (u_tab[j].awaymsg[0] == '\0')
                        && (strcasecmp(u_tab[j].group,g_tab[i].name) == 0))
                    {
                        if (newmod == -1)
                            newmod = j;
                        else if ((u_tab[j].t_recv - u_tab[newmod].t_recv) > 0)
                            newmod = j;
                    }
                }

                /*
                 * if we didn't find a new mod (possible if other users 
                 * have /away set), skip til next check, otherwise set the
                 * new mod.
                 */
                if ( newmod != -1 )
                {
                    g_tab[i].mod = newmod;
                    g_tab[i].modtimeout = 0.0;
                    memset(g_tab[i].missingmod, 0, MAX_NICKLEN);
                    sprintf (mbuf, "%s is now mod.", u_tab[newmod].nickname);
                    s_status_group (2,0,i, "Timeout", mbuf);
                }
            }
        }

#ifdef MAX_IDLE
        for(i=0;i<MAX_REAL_USERS;i++) {
            /* look at folx who are logged in */
            if(u_tab[i].login >= LOGIN_COMPLETE) {
                if((TheTime - u_tab[i].t_recv) > MAX_IDLE) {
                    /* kill that puppy */
                    sprintf(mbuf, "[TIMEOUT] %d (%ld - %ld > %d)", 
                            i, TheTime, u_tab[i].t_recv, MAX_IDLE);
                    mdb(MSG_INFO, mbuf);
                    sendstatus(i, "Drop", 
                               "Your connection has been idled out.");
                    disconnectuser(i);
                }
                else if ((TheTime - u_tab[i].t_recv) > (MAX_IDLE - IDLE_WARN))
                {
                    if (u_tab[i].t_notify == 0)
                    {
                        char tbuf[MAX_INPUTSTR];
                        snprintf (tbuf, MAX_INPUTSTR, IDLE_WARN_MSG,
                                  (int) (IDLE_WARN / 60));
                        sendstatus (i, "Drop", tbuf);
                        u_tab[i].t_notify++;
                    }
                }
                else u_tab[i].t_notify = 0;
            }
        }
#endif    /* MAX_IDLE */

        for(i=0;i<MAX_REAL_USERS;i++) {
            int gi;

            /* they must be logged in */
            if (u_tab[i].login < LOGIN_COMPLETE )
                continue;

            if ( (gi = find_group (u_tab[i].group)) < 0 ) {
                sprintf(mbuf,
                        "Can't locate group (%s) of possible idler %s",
                        u_tab[i].group, u_tab[i].nickname);
                mdb(MSG_INFO, mbuf);
                continue;
            }

            /* look at folx who in group w/out idleboot disabled
             * and not already in group idle */
            if ( strcasecmp (u_tab[i].group, IDLE_GROUP) != 0
                 && g_tab[gi].idleboot > 0 ) {
                const char    *bootmsg = IDLE_BOOT_MSG;

                if ( g_tab[gi].idleboot_msg[0] != '\0' )
                    bootmsg = g_tab[gi].idleboot_msg;

                /* boot that puppy */
                if((TheTime - u_tab[i].t_recv) > g_tab[gi].idleboot) {

                    /* logfile message */
                    sprintf(mbuf, "[IDLE_BOOT] %d (%ld > %d)", 
                            i, (TheTime - u_tab[i].t_recv), g_tab[gi].idleboot);
                    mdb(MSG_INFO, mbuf);

                    /* let them know */
                    sprintf (mbuf, bootmsg, "you");
                    sendstatus(i, "Idle-Boot", mbuf);

                    /* let their group know */
                    if(g_tab[gi].volume != QUIET) {
                        sprintf (mbuf, bootmsg, u_tab[i].nickname);
                        s_status_group (1, 0, i, "Idle-Boot", mbuf);
                    }

                    /* this is done after they are moved so the
                     * don't see the messages since s_status_group
                     * doesn't have a nice interface for groups
                     * we aren't actually in
                     */

                    /* check for groups they were mod of and let go */
                    for ( j = 0; j < MAX_GROUPS; j++ ) {
                        if ( g_tab[j].mod == i ) {
                            int newmod, k;

                            /*
                             * this is a bit verbose i decided 
                             * so it's commented out for now
                             *
                             if(g_tab[j].volume != QUIET)
                             s_status_group(1, 0, i, "Idle-Boot", 
                             "Your group moderator idled away. (No timeout)");
                             */

                            newmod = -1;
                            for (k=0; k<MAX_USERS; k++) {
                                if ((u_tab[k].login >= LOGIN_COMPLETE) && 
                                    (strcasecmp(u_tab[k].group,g_tab[j].name) == 0)) {
                                    if (newmod == -1)
                                        newmod = k;
                                    else if ((u_tab[k].t_recv - u_tab[newmod].t_recv) > 0)
                                        newmod = k;
                                }
                            }
                            g_tab[j].mod = newmod;
                            sprintf (mbuf, "%s is now mod.", u_tab[newmod].nickname);
                            if (!strcmp (g_tab[j].name, u_tab[i].group))
                                s_status_group(1, 0, i, "Pass", mbuf);
                            else
                                s_status_group(2, 0, j, "Pass", mbuf);
                        }
                    }

                    /* fake an s_change to group IDLE_GROUP */
                    strcpy (fields[1], IDLE_GROUP);
                    is_booting = 1;
                    s_change(i, 2);
                    is_booting = 0;
                    server_stats.idleboots++;
                }
            }
        }
    }
}

void c_userchar(void)
{
    mdb(MSG_ERR, "server erroneously got client userchar call");
}

int s_lost_user(int n)         /* n = fd of that user */
{
    int num_left;
    int was_mod;
    int ogi;
    char t_grp[MAX_GROUPLEN+1];
    char t_name[MAX_NICKLEN+1];
    char t_fid[512];
    int how_many;
    int len, i, j;
    char one[255], two[255], three[255];
    int  newmod, was_registered, is_killed;
    long TheTime, UserTime;

    if (n >= MAX_REAL_USERS) {
        sprintf(mbuf,"[LOST] %d: >= %d (MAX_REAL_USERS)", n,
                MAX_REAL_USERS);
        mdb(MSG_INFO, mbuf);
        return 0;
    }

    /* Make sure the user was logged in at all */
    if (u_tab[n].login == LOGIN_FALSE) {
        sprintf(mbuf,"[LOST] %d: not logged in", n);
        mdb(MSG_INFO, mbuf);
        /* they were NOT logged in */
        /* just clear their entry and return */
        clear_user_item(n);
        return 0;
    }
    else {
        sprintf(mbuf,"[LOST] %d: %s@%s", n, 
                u_tab[n].loginid, u_tab[n].nodeid);
        mdb(MSG_INFO, mbuf);
    }

    /* save stuff we want to know about the user */
    if (strlen(u_tab[n].realname) > 0)
        was_registered = 1;
    else
        was_registered = 0;
    is_killed = S_kill[n];
    UserTime = u_tab[n].t_recv;
    len = strlen(u_tab[n].group);
    how_many = (MAX_GROUPLEN > len) ? len:MAX_GROUPLEN;
    memset(t_grp, 0, MAX_GROUPLEN+1);
    strncpy(t_grp,u_tab[n].group, how_many);

    len = strlen(u_tab[n].nickname);
    how_many = (MAX_NICKLEN > len) ? len:MAX_NICKLEN;

    memset(t_name, 0, MAX_NICKLEN+1);
    strncpy(t_name,u_tab[n].nickname, how_many);

    sprintf(t_fid,"%s (%s@%s)", u_tab[n].nickname, u_tab[n].loginid,
            u_tab[n].nodeid);

    memset(one, 0, 255);
    memset(two, 0, 255);
    memset(three, 0, 255);
    sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
    ucaseit(one);
    strcpy(three, u_tab[n].nickname);
    ucaseit(three);
    for (j = 0; j < MAX_USERS; j++) {
        if ((n != j) && (u_tab[j].login > LOGIN_FALSE)) {
            if ((nlmatch(three, *u_tab[j].n_notifies) ||
                 nlmatch(one, *u_tab[j].s_notifies)) &&
                (g_tab[find_group(u_tab[n].group)].visibility != 
                 SUPERSECRET)) {
                sprintf (two, "%s (%s) has just signed off", 
                         u_tab[n].nickname, one);
                sendstatus(j, "Notify-Off", two);
            }
        }
    }

    /* clear the user from the user table to keep searches from
       finding them.
     */
    clear_user_item(n);

    /* At least they were logged in */

    /* find out what group they are in */
    /* note: they may be moderator of a group they are NOT in */

    ogi = find_group(t_grp);
    if (ogi < 0) return -1;

    /* check if there are any users left in present group */
    num_left = count_users_in_group(t_grp);

    if ( num_left > 0) {
        /* tell remaining group members user left */
        TheTime = time(NULL);
        if (is_killed > 0)
            sprintf(mbuf,"%s has been disconnected.", t_fid);
#ifdef MAX_IDLE
        else if((TheTime - UserTime) > MAX_IDLE)
            sprintf(mbuf,"%s has idled out.", t_fid);
#endif    /* MAX_IDLE */
        else
            sprintf(mbuf,"%s has signed off.", t_fid);
        if(g_tab[ogi].volume != QUIET)
            s_status_group(2,0,ogi, "Sign-off", mbuf);
    } else {
        /* otherwise zap the group he was in */
        clear_group_item(ogi);
    }

    /* check if user was mod of any group */
    while ( (was_mod = check_mods(n)) != -1 )
    {
        /* if was mod, check if there are any users left in moderated group */
        num_left =
            count_users_in_group(g_tab[was_mod].name);
        if ( num_left > 0) {
            /* if, additionally, the user was mod,
               tell them that too. */
            if (was_registered == 0) {
                if(g_tab[was_mod].volume != QUIET)
                    s_status_group(2,0,was_mod, "Sign-off", 
                                   "Your group moderator signed off. (No timeout)");
                newmod = -1;
                for (i=0; i<MAX_USERS; i++) 
                    if ((u_tab[i].login >= LOGIN_COMPLETE) && 
                        (strcasecmp(u_tab[i].group,g_tab[was_mod].name) == 0)) {
                        if (newmod == -1)
                            newmod = i;
                        else if ((u_tab[i].t_recv - u_tab[newmod].t_recv) > 0)
                            newmod = i;
                    }
                g_tab[was_mod].mod = newmod;
                sprintf (mbuf, "%s is now mod.", u_tab[newmod].nickname);
                s_status_group(2,0,was_mod, "Pass", mbuf);
            }
            else {
                sprintf(mbuf, "Your group moderator signed off. (%d second timeout)", (int) MOD_TIMEOUT);
                s_status_group(2,0,was_mod, "Mod", mbuf);
                g_tab[was_mod].mod = -1;
                strcpy(g_tab[was_mod].missingmod, t_name);
                TheTime = time(NULL);
                g_tab[was_mod].modtimeout = TheTime + MOD_TIMEOUT;
            }
        }
    }
    return 0;
}

void s_packet(int x, char *pkt)
{
    /* go figure out what kind of packet */
    dispatch(x, ++pkt);    
}


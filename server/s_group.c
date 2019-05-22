/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to handle group requests */

#include "config.h"

#ifdef HAVE_TIME_H
#include <time.h>
#endif
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "server.h"
#include "externs.h"
#include "lookup.h"
#include "groups.h"
#include "strutil.h"
#include "access.h"
#include "namelist.h"
#include "send.h"
#include "users.h"
#include "mdb.h"
#include "s_commands.h"
#include "s_stats.h"    /* for server_stats */

int is_booting = 0;

int s_cancel(int n, int argc)
{
    int gi;
    int is_restricted, is_moderator;
    int can_do;
    int i;
    int mode = -1;
    int quiet = -1;
    char *who, *flags;

    if (argc == 2)
    {
        /* constraints:
           group is restricted & inviter is mod & 
           fields[1] is in invite list
           change in invite list
           otherwise
           complain appropriately
           side-effects:
           none
other:
fields[1] is nickname of person for cancel
         */
        /* gi is canceller's groupindex */
        gi = find_group(u_tab[n].group);

        is_restricted = (g_tab[gi].control == RESTRICTED);
        is_moderator =(g_tab[gi].mod == n);
        can_do = (is_restricted && is_moderator) ;
        who = fields[1];
        while (who[0] == '-')
        {
            flags = getword(who);
            for (i = 1; i < strlen(flags); i++)
                switch (flags[i]) {
                    case 'q':
                        if (quiet == -1)
                            quiet = 1;
                        else {
                            senderror(n, "Usage: cancel {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 's':
                        if (mode == -1)
                            mode = 1;
                        else {
                            senderror(n, "Usage: cancel {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'n':
                        if (mode == -1)
                            mode = 0;
                        else {
                            senderror(n, "Usage: cancel {-q} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    default:
                        senderror(n, "Usage: cancel {-q} {-n nickname | -s address}");
                        return -1;
                }
            who = get_tail(who);
        }

        if (mode == -1) mode = 0;
        if (mode == 1) ucaseit(who);
        if (quiet == -1) quiet = 0;

        if (can_do)
        {
            if ((mode == 0) &&
                ((nldelete(g_tab[gi].n_invites,who) == 0) ||
                 (nldelete(g_tab[gi].nr_invites,who) == 0)))
            {
                if (quiet == 0)
                {
                    sprintf(mbuf, "%s cancelled", who);
                    sendstatus(n,"FYI",mbuf);
                }
                if (find_user(who) < 0)
                {
                    sprintf(mbuf,"%s is not signed on",who);
                    sendstatus(n, "Warning", mbuf);
                }
            }
            else if ((mode == 1) &&
                     ((nldelete(g_tab[gi].s_invites,who) == 0) ||
                      (nldelete(g_tab[gi].sr_invites,who) == 0)))
            {
                if (quiet == 0)
                {
                    sprintf(mbuf, "%s cancelled", who);
                    sendstatus(n,"FYI",mbuf);
                }
            }
            else
            {
                sprintf(mbuf, "%s was not invited", who);
                sendstatus(n,"Warning",mbuf);
            }
        }
        else
        {
            if ( !is_restricted )
                senderror(n,"The group isn't restricted.");

            if ( !is_moderator )
                senderror(n,"You aren't the moderator.");
        }
    } else {
        mdb(MSG_INFO, "cancel: wrong number of parz");
    }
    return 0;
}

/* change group (if possible) */
/* group 1 is always:
   normal visibility
   public control
   otherwise the group is created:
   normal visibility 
   n is mod
 */

int s_change(int n, int argc)
{
    int group_exists;
    int is_restricted;
    int is_moderator;
    int visibility;
    int is_invited;
    int target_user;
    int ngi, ogi; /* new and old group indices */
    int len, how_many, i;
    char n_g_n[MAX_GROUPLEN+3]; /* new group name */
    long TheTime;
    char useraddr[255];
    int login_status = 0;

    /* this is a silly hack, but so is this entire server, really */
    if ( argc == 3 )
    {
        login_status = 1;
        argc--;
    }

    if (argc == 2)
    {
        /* constraints:
           if group does not exist,
           there must be an empty slot in g_tab.
           if the group does exist, and is public or moderated
           none.
           if the group does exist, and is restricted
           n must have an invitation
           -or-
           n must be the mod.
           new group can't be the same as the old group.
           new group name has to be at least 1 char.
           new group name is limited in length.
           new group has < maximum users allowed

notes:
the old group doesn't exist if this is a login.
presumes u_tab has an entry set up for n
(except the group name and login members)

side-effects:
if success, new group gets status "join" message
if old group existed and still has members,
old group gets status "leave" message
otherwise old group gets destroyed if nec.
return is 0
for either the old or new groups -- if QUIET,
then no message.

if failure, message given and return is -1
         */

        memset(useraddr, 0, sizeof (useraddr));
        sprintf(useraddr, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
        ucaseit(useraddr);
        len = strlen(fields[1]);

        if (fields[1][0] == '@')
        {
            if ((target_user = find_user(&fields[1][1])) < 0)
            {
                senderror(n, "User not found.");
                return(-1);
            }
            else if (g_tab[find_group(u_tab[target_user].group)].visibility != VISIBLE)
            {
                senderror(n, "User not found.");
                return(-1);
            }
            else
            {
                strcpy (fields[1], u_tab[target_user].group);
                return(s_change(n, argc));
            }
        }

        if (len <=0)
        {
            /* oops, not long enough */
            senderror(n, "Null groupnames are not allowed.");
            return -1;
        }

        memset(n_g_n, 0, sizeof (n_g_n));

        if (fields[1][0] == '.')
        {
            if (fields[1][1] == '.')
            {
                how_many = (MAX_GROUPLEN + 2> len) ? len:MAX_GROUPLEN + 2;
                strncpy(n_g_n, &fields[1][2], how_many - 2);
                visibility = SUPERSECRET;
            }
            else
            {
                how_many = (MAX_GROUPLEN + 1> len) ? len:MAX_GROUPLEN + 1;
                strncpy(n_g_n, &fields[1][1], how_many - 1);
                visibility = SECRET;
            }
        }
        else
        {
            how_many = (MAX_GROUPLEN > len) ? len:MAX_GROUPLEN;
            strncpy(n_g_n, fields[1], how_many);
            visibility = VISIBLE;
        }

        if (strlen(n_g_n) < 1)
        {
            senderror(n, "Group name too short.");
            return(-1);
        }

        /*
         * feature note: if someone specifies a group named "..."
         * it results in a super-secret group named "." which does
         * funny things with other commands. ie, a user doing a 
         * /w @user-in-dotgroup will end up with a /w listing of
         * their current group since "." will be interpreted as the
         * current group. we sorta like this oddity so have left it in.
         */

        filtergroupname(n_g_n);

        group_exists = ((ngi = find_group(n_g_n)) >= 0);

        if (group_exists)
            is_invited = ((nlpresent(u_tab[n].nickname, 
                                     *g_tab[ngi].n_invites) > 0) ||
                          (nlmatch(useraddr, *g_tab[ngi].s_invites)) ||
                          (nlpresent(u_tab[n].nickname, *g_tab[ngi].nr_invites) &&
                           (strlen(u_tab[n].realname) > 0)) ||
                          (nlmatch(useraddr, *g_tab[ngi].sr_invites) &&
                           (strlen(u_tab[n].realname) > 0)));
        else
            is_invited = 0;

        if (!group_exists)
        {
            /* if it doesn't exist, try to create it */
            /* see if there is an empty slot */
            ngi = find_empty_group();
            if (ngi < 0)
            {
                /* complain and fail */
                senderror(n,"Too many groups!");
                return (-1);
            }

            /* could create it, so fill in the info */
            g_tab[ngi].visibility = visibility;
            strcpy(g_tab[ngi].name, n_g_n);

            /* special settings employed for special groups */
            if ( strcasecmp (BOOT_GROUP, n_g_n) == 0 )
            {
                g_tab[ngi].control = PUBLIC;
                g_tab[ngi].visibility = SECRET;
                g_tab[ngi].volume = LOUD;
                g_tab[ngi].idleboot = 3600;
                memset(g_tab[ngi].topic, 0, MAX_TOPICLEN+1);
                strncpy(g_tab[ngi].topic, "Boot-ees", MAX_TOPICLEN);
            }
            else if ( strcasecmp (IDLE_GROUP, n_g_n) == 0 )
            {
                g_tab[ngi].control = PUBLIC;
                g_tab[ngi].visibility = VISIBLE;
                g_tab[ngi].volume = QUIET;
                g_tab[ngi].idleboot = 0;    /* disabled */
                memset(g_tab[ngi].topic, 0, MAX_TOPICLEN+1);
                strncpy(g_tab[ngi].topic, "Idlers",
                        MAX_TOPICLEN);
            }
            else if (strcasecmp("1", n_g_n) != 0)
            {
                g_tab[ngi].control = MODERATED;
                g_tab[ngi].mod = n;
            } else {
                g_tab[ngi].control = PUBLIC;
            }

            /* initialize the invitation list */
            nlinit(g_tab[ngi].n_invites, MAX_INVITES);
            nlinit(g_tab[ngi].nr_invites, MAX_INVITES);
            nlinit(g_tab[ngi].s_invites, MAX_INVITES);
            nlinit(g_tab[ngi].sr_invites, MAX_INVITES);
            nlinit(g_tab[ngi].n_bars, MAX_INVITES);
            nlinit(g_tab[ngi].n_nr_bars, MAX_INVITES);
            nlinit(g_tab[ngi].s_bars, MAX_INVITES);
            nlinit(g_tab[ngi].s_nr_bars, MAX_INVITES);
        }

        /* if we get here, the destination group exists */

        /* is it restricted? */

        is_restricted = (g_tab[ngi].control == RESTRICTED);
        if (is_restricted)
        {
            /* are we the moderator or admin? */
            is_moderator = (check_auth(n) || (g_tab[ngi].mod == n));
            if (!is_moderator)
            {
                /* are we invited? */
                is_invited = ((nlpresent(u_tab[n].nickname, 
                                         *g_tab[ngi].n_invites) > 0) ||
                              (nlmatch(useraddr, *g_tab[ngi].s_invites)) ||
                              (nlpresent(u_tab[n].nickname, *g_tab[ngi].nr_invites) &&
                               (strlen(u_tab[n].realname) > 0)) ||
                              (nlmatch(useraddr, *g_tab[ngi].sr_invites) &&
                               (strlen(u_tab[n].realname) > 0)));

                if (!is_invited && is_booting == 0)
                {
                    sprintf(mbuf, "%s is restricted.", n_g_n);
                    senderror(n, mbuf);

                    /*
                     * send probe msg if group is loud 
                     * AND user isn't hushed by mod
                     */
                    if ((g_tab[ngi].mod > 0) && 
                        (g_tab[ngi].volume == LOUD))
                    {
                        char one[255], two[255];
                        int    gm = g_tab[ngi].mod;

                        sprintf(one, "%s@%s", u_tab[n].loginid,
                                u_tab[n].nodeid);
                        ucaseit(one);
                        strcpy(two, u_tab[n].nickname);
                        ucaseit(two);

                        if ( !nlmatch(one, *u_tab[gm].pri_s_hushed) &&
                             !nlmatch(two, *u_tab[gm].pri_n_hushed) )
                        {
                            sprintf (mbuf,
                                     "%s tried to enter the group.",
                                     u_tab[n].nickname);
                            sendstatus(g_tab[ngi].mod, "Probe",
                                       mbuf);
                            /*
                             * this counts as if it was a private
                             * message for /whereis 
                             */
                            lpriv_id[gm] = n;
                        }

                    }
                    return(-1);
                }
            }
        }

        /* remember if old group existed, and if so,
           what was its index
         */

        if (strlen(u_tab[n].group) == 0)
        {
            ogi = -1;
        }
        else
        {
            ogi = find_group(u_tab[n].group);
            if (ogi < 0)
            {
                sprintf(mbuf, "Can't find group %s\n", u_tab[n].group);
                mdb(MSG_ERR, mbuf);
            }
        }

        /*
         * if it's not a restricted or public group
         * (ie, controlled or moderated), check to
         * see if there's a size restriction
         */
        if ( g_tab[ngi].control != RESTRICTED && 
             g_tab[ngi].control != PUBLIC )
        {
            if ( g_tab[ngi].size > 0 )
            {
                int num = 0;

                /* count the users in this group */
                for ( i = 0; i < MAX_REAL_USERS; i++ )
                {
                    if ( !strcasecmp (u_tab[i].group, g_tab[ngi].name) )
                        ++num;
                }

                /* if it's full, and we're not mod */
                if ( num >= g_tab[ngi].size && g_tab[ngi].mod != n )
                {
                    if ( (g_tab[ngi].mod > 0) && 
                         (g_tab[ngi].volume == LOUD) )
                    {
                        char one[255], two[255];
                        int    gm = g_tab[ngi].mod;

                        sprintf(one, "%s@%s", u_tab[n].loginid,
                                u_tab[n].nodeid);
                        ucaseit(one);
                        strcpy(two, u_tab[n].nickname);
                        ucaseit(two);

                        if ( !nlmatch(one, *u_tab[gm].pri_s_hushed) &&
                             !nlmatch(two, *u_tab[gm].pri_n_hushed) )
                        {
                            sprintf (mbuf,
                                     "%s tried to enter the group, but it's full.",
                                     u_tab[n].nickname);
                            sendstatus(g_tab[ngi].mod, "Probe", mbuf);
                            /*
                             * this counts as if it was a private
                             * message for /whereis 
                             */
                            lpriv_id[gm] = n;
                        }
                    }

                    sprintf(mbuf, "Group %s is full.", n_g_n);
                    senderror(n, mbuf);

                    return (-1);
                }
            }
        }

        /* is the new group different than the old group? */
        if (ngi != ogi) {
            /* the group exists and we are allowed in. */
            strcpy(u_tab[n].group, g_tab[ngi].name);

            /* tell the new group about the arrival */
            sprintf(mbuf,"%s (%s@%s) entered group",
                    u_tab[n].nickname,
                    u_tab[n].loginid,
                    u_tab[n].nodeid);
            if(g_tab[ngi].volume != QUIET) {
                if ( (u_tab[n].login < LOGIN_COMPLETE)
                     || (login_status == 1) ) {
                    s_status_group(1,0,n,"Sign-on",mbuf);
                } else {
                    s_status_group(1,0,n,"Arrive",mbuf);
                }
            }
            sprintf(mbuf,"You are now in group %s",
                    g_tab[ngi].name);
            if (g_tab[ngi].mod == n)
                strcat (mbuf, " as moderator");
            sendstatus(n,"Status",mbuf);

            TheTime = time(NULL);
            u_tab[n].t_group = TheTime;

            /* did the old group exist? */
            if (ogi >= 0) {
                /* is there anyone left? */
                if(count_users_in_group(g_tab[ogi].name)>0) {
                    /* tell them user left */
                    sprintf(mbuf,"%s (%s@%s) just left",
                            u_tab[n].nickname,
                            u_tab[n].loginid,
                            u_tab[n].nodeid);
                    if(g_tab[ogi].volume != QUIET){
                        s_status_group(2,0,ogi,
                                       "Depart",
                                       mbuf);
                        if (g_tab[ogi].mod == n) {
                            sprintf(mbuf, "You are still mod of group %s", g_tab[ogi].name);
                            sendimport(n, "Mod", mbuf);
                        }
                    }
                } else {
                    /* zap the old group */
                    clear_group_item(ogi);
                }
                return 0;
            }
        }
        else
        {
            senderror(n,"You are already in that group!");
            return -1;
        }
    } else {
        mdb(MSG_INFO, "change: wrong number of parz");
        return (-1);
    }
    return 0;
}

extern void add_item(int n, const char* item, char *buf);

int list_invites (int n, int gi)
{
    int i, count, tot_count;
    NAMLIST * ip;
    char line[75],    /* NOTE: this is MAX_LINE from s_who.c */
         *cp;

    tot_count = 0;

    /* invitations */
    memset(line, 0, sizeof (line));
    ip = g_tab[gi].n_invites;
    count = nlcount(*ip);
    tot_count += count;
    for (i=0; i<count; i++)
    {
        if (i == 0)
            add_item(n, "Nicks invited: ", line);
        else
            add_item(n, ", ", line);
        cp = nlget(ip);
        add_item(n, cp, line);
    }
    ip = g_tab[gi].nr_invites;
    count = nlcount(*ip);
    tot_count += count;
    for (i=0; i<count; i++)
    {
        if (strlen(line) == 0)
            add_item(n, "Nicks invited: ", line);
        else
            add_item(n, ", ", line);
        cp = nlget(ip);
        sprintf(mbuf, "%s(r)", cp);
        add_item(n, mbuf, line);
    }
    if (strlen(line) > 0)
        sends_cmdout(n, line);

    memset(line, 0, sizeof (line));
    ip = g_tab[gi].s_invites;
    count = nlcount(*ip);
    tot_count += count;
    for (i=0; i<count; i++)
    {
        if (i == 0)
            add_item(n, "Addrs invited: ", line);
        else
            add_item(n, ", ", line);
        cp = nlget(ip);
        add_item(n, cp, line);
    }
    ip = g_tab[gi].sr_invites;
    count = nlcount(*ip);
    tot_count += count;
    for (i=0; i<count; i++)
    {
        if (strlen(line) == 0)
            add_item(n, "Addrs invited: ", line);
        else
            add_item(n, ", ", line);
        cp = nlget(ip);
        sprintf(mbuf, "%s(r)", cp);
        add_item(n, mbuf, line);
    }
    if (strlen(line) > 0)
        sends_cmdout(n, line);

    return (tot_count);
}


int s_invite(int n, int argc)
{
    int gi;
    int is_restricted, is_moderator;
    int dest;
    int mode = -1;
    int r = -1;
    int i;
    int quiet = -1;
    char *who = (char *)NULL;
    char *flags;

    if (argc == 2)
    {
        /* constraints:
           group is restricted & inviter is mod => 
           messages and change in invite list
           group is not restricted =>
           message only
           side-effects:
           if invitee is signed on, send status message
other:
fields[1] is nickname of person to invite
         */

        gi = find_group(u_tab[n].group);

        is_restricted = (g_tab[gi].control == RESTRICTED);
        is_moderator =(g_tab[gi].mod == n);
        who = fields[1];
        while (who[0] == '-')
        {
            flags = getword(who);
            for (i = 1; i < strlen(flags); i++)
                switch (flags[i])
                {
                    case 'q':
                        if (quiet == -1)
                            quiet = 1;
                        else
                        {
                            senderror(n, "Usage: invite {-q} {-r} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 's':
                        if (mode == -1)
                            mode = 1;
                        else
                        {
                            senderror(n, "Usage: invite {-q} {-r} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'n':
                        if (mode == -1)
                            mode = 0;
                        else
                        {
                            senderror(n, "Usage: invite {-q} {-r} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    case 'r':
                        if (r == -1)
                            r = 1;
                        else
                        {
                            senderror(n, "Usage: invite {-q} {-r} {-n nickname | -s address}");
                            return -1;
                        }
                        break;
                    default:
                        senderror(n, "Usage: invite {-q} {-r} {-n nickname | -s address}");
                        return -1;
                }
            who = get_tail(who);
        }

        if ( who == (char *)NULL || *who == '\0' )
        {
            /*
               senderror(n,
               "Usage: invite {-q} {-r} {-n nickname | -s address}");
               s_status(n, argc);
             */
            if ( !is_restricted )
            {
                sendstatus (n, "Invite", "Invite lists only function in restricted groups.");
            }
            else
            {
                if ( list_invites (n, gi) < 1 )
                    sendstatus (n, "Invite", "No one on invite list");
            }
            return -1;
        }

        if (mode == -1) mode = 0;
        if (r == -1) r = 0;
        if (mode == 1) ucaseit(who);
        if (quiet == -1) quiet = 0;

        if (is_restricted && is_moderator) {
            if ((mode == 0) && (r == 0)) {
                nlput(g_tab[gi].n_invites, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s invited", who);
                    sendstatus(n,"FYI",mbuf);
                }
            }
            else if ((mode == 0) && (r == 1)) {
                nlput(g_tab[gi].nr_invites, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s invited (registered only)", who);
                    sendstatus(n,"FYI",mbuf);
                }
            }
            else if ((mode == 1) && (r == 0)) {
                nlput(g_tab[gi].s_invites, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s invited", who);
                    sendstatus(n,"FYI",mbuf);
                }
            }
            else if ((mode == 1) && (r == 1)) {
                nlput(g_tab[gi].sr_invites, who);
                if (quiet == 0) {
                    sprintf(mbuf, "%s invited (registered only)", who);
                    sendstatus(n,"FYI",mbuf);
                }
            }
        }
        if ((mode == 0) && (!is_restricted || is_moderator)) {
            if((dest = find_user(who)) >= 0) {
                if ((strlen(u_tab[dest].realname) > 0) || (r == 0)) {
                    sprintf(mbuf, "You are invited to group %s by %s",
                            u_tab[n].group, u_tab[n].nickname);
                    sendstatus(dest,"RSVP",mbuf);
                }
            } else {
                sprintf(mbuf, "%s is not signed on.", who);
                sendstatus(n, "Warning", mbuf);
            }
        }
        if ((mode == 1) && !is_restricted)
            senderror(n, "The group isn't restricted.");
        if (!is_moderator && is_restricted)
            senderror(n, "You aren't the moderator.");
    }
    else {
        mdb(MSG_INFO, "invite: wrong number of parz");
    }
    return 0;
}

int s_pass(int n, int argc)
{
    int is_moderator;
    int gi;
    int dest;
    char * cp;

    if (argc == 2) {
        /* constraints:
           user must be mod of group they are in
           if passing, dest. nickname must be on

           side-effects:
           if passed successfully,
           dest. nickname gets message
           if passed or relinquished successfully,
           user's moderated group gets message
           if the group is QUIET, no messages.
         */

        /* what is the user's group? */
        gi = find_group(u_tab[n].group);

        /* are they the mod or admin? */
        is_moderator = (check_auth(n) || (g_tab[gi].mod == n));

        if (!is_moderator) {
            senderror(n,"You aren't the moderator.");
            return 0;
        }

        /* they are the mod */

        /* do they want to pass or relinquish? */
        if( strlen(fields[1]) == 0) {
            /* relinquish mod */
            g_tab[gi].mod = -1;

            /* tell people */
            sprintf(mbuf, "%s just relinquished the mod.",
                    u_tab[n].nickname);
            if (g_tab[gi].volume != QUIET) {
                s_status_group(1,1,n,"Pass",mbuf);
            }

            if ( g_tab[gi].control != PUBLIC &&
                 g_tab[gi].control != MODERATED &&
                 g_tab[gi].control != RESTRICTED )
            {
                g_tab[gi].control = PUBLIC;

                if ( g_tab[gi].volume != QUIET )
                    s_status_group (1, 1, n, "Change",
                                    "Group is now public.");
            }

        } else {
            /* check dest. nickname */
            cp = getword(fields[1]);
            if (strlen(cp) == 0) {
                mdb(MSG_INFO, "null string in pass");
                return 0;
            }

            if (strcasecmp(cp, "server") == 0) {
                senderror(n, "Cannot pass to Server.");
                return -1;
            }

            /* get index of dest. nickname */
            dest = find_user(cp);

            /* do they exist? */
            if (dest >= 0) {
                /* fix the mod-ship */
                g_tab[gi].mod = dest;

                /* send message to dest.*/
                sprintf(mbuf,
                        "%s just passed you moderation of group %s",
                        u_tab[n].nickname,
                        g_tab[gi].name);    
                sendstatus(dest,"Pass", mbuf);

                /* send message to group */
                sprintf(mbuf,
                        "%s has passed moderation to %s",
                        u_tab[n].nickname, 
                        u_tab[dest].nickname);
                if (g_tab[gi].volume != QUIET) {
                    s_status_group(1,1,n,"Pass",mbuf);
                }
            } else {
                sprintf(mbuf,"%s is not on!", cp);
                senderror(n, mbuf);
            }
        }
    } else {
        mdb(MSG_INFO, "pass: wrong number of parz");
    }
    return 0;
}

int s_boot(int n, int argc)
{
    int gi;
    int is_moderator;
    int dest;
    int destgi;
    char *msg;
    char *user;

    if (argc == 2) 
    {
        /* constraints:
           inviter is mod & fields[1] is in this group
           put fields[1] in group 1
           send boot message to fields[1]
           otherwise
           complain appropriately
           if group is QUIET, no messages sent.
           side-effects:
           if boot occurs, inform others in group
other:
fields[1] is nickname of person to boot
         */

        /* gi is canceller's groupindex */
        gi = find_group(u_tab[n].group);

        /* are they the mod or admin? */
        /*is_moderator =(g_tab[gi].mod == n); */
        is_moderator = (check_auth(n) || (g_tab[gi].mod == n));

        if (is_moderator) 
        {
            user = getword (fields[1]);
            msg = get_tail (fields[1]);
            dest = find_user(user);
            if (dest < 0) 
            {
                sprintf(mbuf,
                        "%s is not signed on", user);
                sendstatus(n,"Warning",mbuf);
            } 
            else if (strcasecmp(user, "ADMIN") == 0)
            {
                senderror(n, "You nobk! You can't boot the admin!");
                sprintf(mbuf,
                        "%s tried to boot you!", u_tab[n].nickname);
                sendstatus(dest, "Boot Attempt!", mbuf);
            }
            else 
            {
                int num_s = 0;

                if ( msg != (char *)NULL && *msg != '\0' )
                {
                    char *r;

                    num_s = 1;
                    r = filterfmt (msg, &num_s);

                    /* if r != null, num_s may be 0
                     * which is ok */
                    if ( r != (char *)NULL && num_s != 0 )
                    {
                        senderror (n, r);
                        return 0;
                    }
                }
                else
                    msg = (char *)NULL;
                destgi = find_group(u_tab[dest].group);
                if (gi == destgi) {
                    /* send status to group */
                    if ((nldelete(g_tab[destgi].n_invites,
                                  u_tab[dest].nickname) == 0) ||
                        (nldelete(g_tab[destgi].nr_invites,
                                  u_tab[dest].nickname) == 0)) {
                        sprintf(mbuf, "%s cancelled",
                                u_tab[dest].nickname);
                        sendstatus(n, "FYI", mbuf);
                    }
                    if ( msg != (char *)NULL )
                    {
                        if ( num_s == 0 )
                            sprintf(mbuf, "%s %s",
                                    u_tab[dest].nickname,
                                    msg);
                        else
                            sprintf(mbuf, msg,
                                    u_tab[dest].nickname);
                    }
                    else
                        sprintf(mbuf,"%s was booted.",
                                u_tab[dest].nickname);

                    if(g_tab[destgi].volume != QUIET) {
                        s_status_group(1,1,n,"Boot",mbuf);
                    }
                    /* tell 'em it happened. */
                    sprintf(mbuf,"%s booted you.",
                            u_tab[n].nickname);
                    sendstatus(dest,"Boot",mbuf);
                    /* fake s_change to group BOOT_GROUP */
                    strcpy(fields[1],BOOT_GROUP);
                    is_booting = 1;
                    s_change(dest,2);
                    is_booting = 0;
                    server_stats.boots++;
                    /* note: although s_change returns a result, we presume that we can
                       always join BOOT_GROUP. */
                } else {
                    sprintf(mbuf,
                            "%s is not in your group",
                            fields[1]);
                    senderror(n,mbuf);
                }
            }
        } 
        else 
        {
            if ( !is_moderator ) {
                senderror(n,"You aren't the moderator.");
            }
        }
    } 
    else 
    {
        mdb(MSG_INFO, "boot: wrong number of parz");
    }

    return 0;
}


/* used by s_status() */
void dump_table(int n)
{
    sendstatus(n,"Information","Table Of Status Commands");
    sendstatus(n,"Information","r - restricted c - controlled m - moderated p - public");
    sendstatus(n,"Information","i - invisible s - secret v - visible");
    sendstatus(n,"Information","q - quiet n - normal l - loud");
    sendstatus(n,"Information","name - change group name");
    sendstatus(n,"Information","? -- this table");
    sendstatus(n,"Information","# - change the number of allowed users");
    sendstatus(n,"Information","b - change the idleboot setting");
    sendstatus(n,"Information","idlebootmsg - change the idleboot message");
    sendstatus(n,"Information","im - change the idlemod setting");
}

/* used by s_status() */
int set_name(int n, int group, char *name)
{
    char * cp;
    int i;
    int len;
    int how_many;
    char n_g_n[MAX_GROUPLEN + 1];

    /* make sure they gave us a name to change to */
    if (strlen(name) == 0) {
        senderror(n, "New name may not be null.");
        return -1;
    }

    /* use only as much as needed */
    len = strlen(name);
    how_many = (MAX_GROUPLEN > len) ? len:MAX_GROUPLEN;
    memset(n_g_n, 0, MAX_GROUPLEN + 1);
    strncpy(n_g_n, name, how_many);

    if (!strcmp(n_g_n, "1")) {
        senderror (n, "Cannot change name to group 1.");
        return -1;
    }

    /* make sure that group doesn't already exist */
    if ((find_group(n_g_n) >= 0) && (find_group(n_g_n) != group)) {
        senderror(n, "That group already exists.");
        return -1;
    }

    /* get a copy to the old name */
    cp = g_tab[group].name;

    /* check through the user table to see who is in that group */
    /* and change their group name entries */
    for (i=0; i< MAX_REAL_USERS; i++) {
        if (strcasecmp(u_tab[i].group, cp) == 0) {
            strcpy(u_tab[i].group, n_g_n);
        }
    }

    /* finally change the name of the group itself */
    strcpy(g_tab[group].name, n_g_n);
    return 0;
}




int s_status(int n, int argc)
{
    int count;
    int i;
    const char * cp;
    char cp2[160];
    const char * vis;
    const char * con;
    const char * volume;
    const char * mod;
    int is_moderator;
    int is_public;
    int has_mod;
    char * p1;
    char * p2;
    int gi;
    NAMLIST * ip;
    char line[75];    /* NOTE: this is MAX_LINE from s_who.c */

    if (argc == 2)
    {
        if( strlen(fields[1]) == 0)
        {
            gi = find_group(u_tab[n].group); /* is n's groupindex */

            /* visibility */
            switch(g_tab[gi].visibility)
            {

                case VISIBLE:
                    vis = "Visible";
                    break;
                case SECRET:
                    vis = "Secret";
                    break;
                case SUPERSECRET:
                    vis = "Invisible";
                    break;
                default:
                    mdb(MSG_INFO, "s_status visibility bad");
                    vis = "Unknown";
            }

            /* control */
            switch(g_tab[gi].control)
            {

                case PUBLIC:
                    con = "Public";
                    break;
                case MODERATED:
                    con = "Moderated";
                    break;
                case RESTRICTED:
                    con = "Restricted";
                    break;
                case CONTROLLED:
                    con = "Controlled";
                    break;
                default:
                    mdb(MSG_INFO, "s_status control bad");
                    con = "Unknown";
            }

            /* volume */
            switch(g_tab[gi].volume)
            {

                case QUIET:
                    volume = "Quiet";
                    break;
                case NORMAL:
                    volume = "Normal";
                    break;
                case LOUD:
                    volume = "Loud";
                    break;
                default:
                    mdb(MSG_INFO, "s_status volume bad");
                    volume = "Unknown";
            }

            /* moderator */
            if(g_tab[gi].mod >= 0)
                mod = u_tab[g_tab[gi].mod].nickname;
            else if (g_tab[gi].modtimeout == 0.0)
                mod = "None";
            else
                mod = g_tab[gi].missingmod;

            sprintf(mbuf, "Name: %s Mod: %s (%s / %s / %s)",
                    g_tab[gi].name, mod, vis, con, volume);
            sends_cmdout(n, mbuf);

            list_invites (n, gi);

            memset(line, 0, sizeof (line));
            ip = g_tab[gi].n_talk;
            count = nlcount(*ip);
            for (i=0; i<count; i++)
            {
                if (i == 0)
                    add_item(n, "Nicks who can talk: ", line);
                else
                    add_item(n, ", ", line);
                cp = nlget(ip);
                add_item(n, cp, line);
            }
            ip = g_tab[gi].nr_talk;
            count = nlcount(*ip);
            for (i=0; i<count; i++)
            {
                if (strlen(line) == 0)
                    add_item(n, "Nicks who can talk: ", line);
                else
                    add_item(n, ", ", line);
                cp = nlget(ip);
                sprintf(mbuf, "%s(r)", cp);
                add_item(n, mbuf, line);
            }
            if (strlen(line) > 0)
                sends_cmdout(n, line);

            if ( g_tab[gi].size == 0 )
            {
                strcpy (mbuf, "Size: no limit");
            }
            else
            {
                sprintf (mbuf, "Size: %d user%s limit", g_tab[gi].size,
                         g_tab[gi].size == 1 ? "" : "s");
            }
            sends_cmdout(n, mbuf);

            if ( g_tab[gi].idleboot == 0 )
            {
                strcpy (mbuf, "Idle-Boot: no limit");
            }
            else
            {
                int hours, mins;

                hours = (g_tab[gi].idleboot / (60 * 60));
                mins = (g_tab[gi].idleboot % (60 * 60)) / 60;

                sprintf (mbuf, "Idle-Boot: %d hour%s, %d minute%s",
                         hours, hours == 1 ? "" : "s",
                         mins, mins == 1 ? "" : "s");
            }
            sends_cmdout(n, mbuf);

            if ( g_tab[gi].idleboot_msg[0] != '\0' )
            {
                sprintf (mbuf, "Idle-Boot Message: %s", g_tab[gi].idleboot_msg);
                sends_cmdout(n, mbuf);
            }

            if ( g_tab[gi].control != PUBLIC )
            {
                if ( g_tab[gi].idlemod == 0 )
                {
                    strcpy (mbuf, "Idle-Mod: no limit");
                }
                else
                {
                    int hours, mins;

                    hours = (g_tab[gi].idlemod / (60 * 60));
                    mins = (g_tab[gi].idlemod % (60 * 60)) / 60;

                    sprintf (mbuf, "Idle-Mod: %d hour%s, %d minute%s",
                             hours, hours == 1 ? "" : "s",
                             mins, mins == 1 ? "" : "s");
                }
                sends_cmdout(n, mbuf);
            }
        }
        else
        {
            /* gi is n's group's index */
            gi = find_group(u_tab[n].group);

            is_public = (g_tab[gi].control == PUBLIC);
            is_moderator = (g_tab[gi].mod == n);
            has_mod = (g_tab[gi].mod != -1 || g_tab[gi].modtimeout > 0.0);

            /*
             * first, check a few cases which we disallow, including
             * trying to change special groups ("1", IDLE_GROUP, BOOT_GROUP)
             */

            if ( strcasecmp(g_tab[gi].name, "1") == 0 ) 
            {
                sprintf (mbuf, "Can't change group 1!");
                senderror(n, mbuf);
                return 0;
            }

            if ( strcasecmp(g_tab[gi].name, IDLE_GROUP) == 0 ) 
            {
                sprintf (mbuf, "Can't change group %s!", IDLE_GROUP);
                senderror(n, mbuf);
                return 0;
            }

            if ( strcasecmp(g_tab[gi].name, BOOT_GROUP) == 0 ) 
            {
                sprintf (mbuf, "Can't change group %s!", BOOT_GROUP);
                senderror(n, mbuf);
                return 0;
            }

            if ( !is_moderator && has_mod && !is_public )
            {
                senderror(n, "You aren't the moderator.");
                return 0;
            }

            /* note: if the group is public when the
               command is given, then the whole
               command is executed even if the first
               option makes the group restricted.
               if there is no moderator and it isn't
               group 1, then the remaining users can
               still change some of the options,
               but still can't boot.
also: this differs from Sean's server in
that the moderator can make the group
public while still retaining the mod
and thus the ability to boot.
but:  group 1 is still always public and
visible.
             */
            p1 = fields[1];
            while(strlen(p2=getword(p1)) > 0)
            {
                int    lu,
                    process = 1;

                cp = (char *)NULL;
                lu = lookup(p2,status_table);

                /*
                 * this is to filter out certain commands which can't
                 * be done by group members in a group w/out a moderator
                 */
                if ( lu == SET_RESTRICT ||
                     lu == SET_MODERATE ||
                     lu == SET_CONTROL ||
                     lu == SET_IDLEBOOT ||
                     lu == SET_IDLEMOD ||
                     lu == SET_QUIET )
                {
                    if ( !has_mod )
                    {
                        sprintf (mbuf,
                                 "Groups without a moderator cannot %s.",
                                 lu == SET_RESTRICT ? "become restricted" :
                                 lu == SET_MODERATE ? "become moderated" :
                                 lu == SET_CONTROL ? "become controlled" :
                                 lu == SET_IDLEBOOT ? "have idleboot changed" :
                                 lu == SET_IDLEMOD ? "have idlemod changed" :
                                 lu == SET_QUIET ? "be changed to quiet" :
                                 "???");

                        senderror (n, mbuf);

                        /* IDLE_BOOT & SET_IDLEMOD have args we need to eat */
                        if ( lu == SET_IDLEBOOT || lu == SET_IDLEMOD )
                        {
                            p1 = get_tail(p1);
                            p2 = getword(p1);
                        }

                        /* keep it from being processed */
                        process = 0;
                    }
                }

                if ( process == 1 )
                {
                    switch (lu)
                    {
                        case SET_RESTRICT:
                            g_tab[gi].control = RESTRICTED;
                            nlinit(g_tab[gi].n_bars, MAX_INVITES);
                            nlinit(g_tab[gi].n_nr_bars, MAX_INVITES);
                            nlinit(g_tab[gi].s_bars, MAX_INVITES);
                            nlinit(g_tab[gi].s_nr_bars, MAX_INVITES);
                            cp = "Group is now restricted.";

                            for (i=0; i<MAX_USERS; i++)
                                if (strcasecmp(u_tab[i].group,g_tab[gi].name) == 0)
                                {
                                    if (strlen(u_tab[i].realname) == 0)
                                        nlput(g_tab[gi].n_invites,
                                              u_tab[i].nickname);
                                    else
                                        nlput(g_tab[gi].nr_invites,
                                              u_tab[i].nickname);

                                    if (g_tab[gi].mod > 0)
                                    {
                                        sprintf(mbuf, "%s invited",u_tab[i].nickname);
                                        sendstatus(g_tab[gi].mod,"FYI",mbuf);
                                    }

                                    sprintf(mbuf,
                                            "You are invited to group %s by default.",
                                            g_tab[gi].name);
                                    sendstatus(i, "FYI", mbuf);
                                }
                            break;

                        case SET_MODERATE:
                            g_tab[gi].control = MODERATED;
                            nlinit(g_tab[gi].n_invites, MAX_INVITES);
                            nlinit(g_tab[gi].nr_invites, MAX_INVITES);
                            nlinit(g_tab[gi].s_invites, MAX_INVITES);
                            nlinit(g_tab[gi].sr_invites, MAX_INVITES);
                            cp = "Group is now moderated.";
                            break;


                        case SET_CONTROL:
                            g_tab[gi].control = CONTROLLED;
                            nlinit(g_tab[gi].n_invites, MAX_INVITES);
                            nlinit(g_tab[gi].nr_invites, MAX_INVITES);
                            nlinit(g_tab[gi].s_invites, MAX_INVITES);
                            nlinit(g_tab[gi].sr_invites, MAX_INVITES);
                            cp = "Group is now controlled.";
                            break;

                        case SET_SIZE:
                            {
                                p1 = get_tail(p1);
                                p2 = getword(p1);

                                if ( p2 == (char *) NULL || *p2 == '\0' )
                                {
                                    sprintf(mbuf, "Size must be between 0 and %d.",
                                            MAX_REAL_USERS);
                                    senderror (n, mbuf);
                                    cp = NULL;
                                }
                                else
                                {
                                    int new_size = atoi (p2);

                                    if ( new_size < 0 || new_size > MAX_REAL_USERS )
                                    {
                                        sprintf(mbuf,
                                                "Size must be between 0 and %d.",
                                                MAX_REAL_USERS);
                                        senderror(n,mbuf);
                                        cp = NULL;
                                    }
                                    else
                                    {
                                        g_tab[gi].size = new_size;
                                        if ( new_size == 0 )
                                        {
                                            sprintf (cp2,
                                                     "%s removed group size limit.",
                                                     u_tab[n].nickname);
                                            cp = cp2;
                                        }
                                        else
                                        {
                                            sprintf (cp2,
                                                     "%s made group limited to %d users.",
                                                     u_tab[n].nickname, g_tab[gi].size);
                                            cp = cp2;
                                        }
                                    }
                                }
                            }
                            break;

                        case SET_IDLEBOOT:
                            {
                                p1 = get_tail(p1);
                                p2 = getword(p1);

                                if ( p2 == (char *) NULL || *p2 == '\0' )
                                {
#ifdef MAX_IDLE
                                    sprintf(mbuf,
                                            "Idle-Boot must be between 0 and %d (0=disabled).",
                                            (MAX_IDLE/60));
#else
                                    strcpy (mbuf,
                                            "Idle-Boot must be specified in number of minutes (0=disabled).");
#endif /* MAX_IDLE */
                                    senderror (n, mbuf);
                                    cp = NULL;
                                }
                                else
                                {
                                    int new_ib = atoi (p2) * 60;

#ifdef MAX_IDLE
                                    if ( new_ib < 0 || new_ib > MAX_IDLE )
                                    {
                                        sprintf(mbuf, "Idle-Boot must be between 0 and %d.",
                                                (MAX_IDLE/60));
                                        senderror(n,mbuf);
                                        cp = NULL;
                                    }
#else
                                    if ( new_ib < 0 )
                                    {
                                        strcpy (mbuf, "Idle-Boot must be > 0 (or 0 to disable).");
                                        senderror(n, mbuf);
                                        cp = NULL;
                                    }
#endif    /* MAX_IDLE */
                                    else
                                    {
                                        g_tab[gi].idleboot = new_ib;

                                        if ( new_ib == 0 )
                                        {
                                            sprintf (cp2,
                                                     "%s disabled idle-boot.",
                                                     u_tab[n].nickname);
                                            cp = cp2;
                                        }
                                        else
                                        {
                                            if ( g_tab[gi].idleboot > (59 * 60) )
                                            {
                                                int hours, mins;

                                                hours = (g_tab[gi].idleboot / (60 * 60));
                                                mins = (g_tab[gi].idleboot % (60 * 60)) / 60;

                                                sprintf (cp2,
                                                         "%s set idle-boot to %d hour%s, %d minute%s.",
                                                         u_tab[n].nickname,
                                                         hours, hours == 1 ? "" : "s",
                                                         mins, mins == 1 ? "" : "s");
                                            }
                                            else
                                            {
                                                sprintf (cp2,
                                                         "%s set idle-boot to %d minute%s.",
                                                         u_tab[n].nickname,
                                                         g_tab[gi].idleboot / 60,
                                                         (g_tab[gi].idleboot / 60) == 1 ? "" : "s");
                                            }

                                            cp = cp2;
                                        }
                                    }
                                }
                            }
                            break;

                        case SET_IDLEBOOT_MSG:
                            p1 = get_tail(p1);

                            if ( p1 == (char *) NULL || *p1 == '\0'
                                 || strlen (p1) > IDLEBOOT_MSG_LEN )
                            {
                                sprintf(mbuf,
                                        "Idle-Boot message length must be between 1 and %d (yours was %d).",
                                        IDLEBOOT_MSG_LEN, strlen (p1));
                                senderror (n, mbuf);
                                cp = NULL;
                            }
                            else
                            {
                                char *new_ibmsg = p1,
                                     *retstr;
                                int num_s = 1;

                                if ( (retstr = filterfmt (new_ibmsg, &num_s)) != (char *)NULL )
                                {
                                    senderror (n, retstr);
                                    cp = NULL;
                                }
                                else
                                {
                                    strcpy (g_tab[gi].idleboot_msg, new_ibmsg);
                                    sprintf (cp2, "Idle-boot msg: %s",
                                             g_tab[gi].idleboot_msg);

                                    /* only send this info back to the mod */
                                    sendstatus (n, "Change", cp2);
                                    cp = NULL;
                                }
                            }

                            /* skip to the end */
                            while(strlen(p2=getword(p1)) > 0)
                                p1 = get_tail(p2);
                            break;

                        case SET_IDLEMOD:
                            {
                                p1 = get_tail(p1);
                                p2 = getword(p1);

                                if ( p2 == (char *) NULL || *p2 == '\0' )
                                {
                                    sprintf(mbuf, "Idle-Mod must be between 0 and %d.",
                                            (MAX_IDLE_MOD/60));
                                    senderror (n, mbuf);
                                    cp = NULL;
                                }
                                else
                                {
                                    int new_im = atoi (p2) * 60;

                                    if ( new_im == 0 )
                                    {
                                        sendstatus(n, "Info", "Using maximum value allowed");
                                        new_im = MAX_IDLE_MOD;
                                    }

                                    if ( new_im < 1 || new_im > MAX_IDLE_MOD )
                                    {
                                        sprintf(mbuf, "Idle-Mod must be between 0 and %d.",
                                                (MAX_IDLE_MOD/60));
                                        senderror(n,mbuf);
                                        cp = NULL;
                                    }
                                    else
                                    {
                                        g_tab[gi].idlemod = new_im;

                                        if ( g_tab[gi].idlemod > (59 * 60) )
                                        {
                                            int hours, mins;

                                            hours = (g_tab[gi].idlemod / (60 * 60));
                                            mins = (g_tab[gi].idlemod % (60 * 60)) / 60;

                                            sprintf (cp2,
                                                     "%s set idle-mod to %d hour%s, %d minute%s.",
                                                     u_tab[n].nickname,
                                                     hours, hours == 1 ? "" : "s",
                                                     mins, mins == 1 ? "" : "s");
                                        }
                                        else
                                        {
                                            sprintf (cp2,
                                                     "%s set idle-mod to %d minute%s.",
                                                     u_tab[n].nickname,
                                                     g_tab[gi].idlemod / 60,
                                                     (g_tab[gi].idlemod / 60) == 1 ? "" : "s");
                                        }

                                        cp = cp2;
                                    }
                                }
                            }
                            break;

                        case SET_PUBLIC:
                            g_tab[gi].control = PUBLIC;
                            nlinit(g_tab[gi].n_invites, MAX_INVITES);
                            nlinit(g_tab[gi].nr_invites, MAX_INVITES);
                            nlinit(g_tab[gi].s_invites, MAX_INVITES);
                            nlinit(g_tab[gi].sr_invites, MAX_INVITES);
                            nlinit(g_tab[gi].n_bars, MAX_INVITES);
                            nlinit(g_tab[gi].n_nr_bars, MAX_INVITES);
                            nlinit(g_tab[gi].s_bars, MAX_INVITES);
                            nlinit(g_tab[gi].s_nr_bars, MAX_INVITES);
                            /* cp = "Group is now public."; */
                            sprintf (cp2, "%s made group public.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_INVISIBLE:
                            g_tab[gi].visibility = SUPERSECRET;
                            /* cp = "Group is now invisible."; */
                            sprintf (cp2, "%s made group invisible.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_SECRET:
                            g_tab[gi].visibility = SECRET;
                            /* cp = "Group is now secret."; */
                            sprintf (cp2, "%s made group secret.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_VISIBLE:
                            g_tab[gi].visibility = VISIBLE;
                            /* cp = "Group is now visible."; */
                            sprintf (cp2, "%s made group visible.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_QUIET:
                            g_tab[gi].volume = QUIET;
                            /* cp = "Group is now quiet."; */
                            sprintf (cp2, "%s made group quiet.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_NORMAL:
                            g_tab[gi].volume = NORMAL;
                            /* cp = "Group is now normal."; */
                            sprintf (cp2, "%s made group normal.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_LOUD:
                            g_tab[gi].volume = LOUD;
                            /* cp = "Group is now loud."; */
                            sprintf (cp2, "%s made group loud.", 
                                     u_tab[n].nickname);
                            cp = cp2;
                            break;

                        case SET_NAME:
                            p1 = get_tail(p1);
                            p2=getword(p1);
                            filtergroupname(p2);
                            if (set_name(n, gi, p2) < 0) {
                                cp = "";
                            } else {
                                /*
                                   cp = "(changed name)";
                                 */
                                /* sprintf (cp2, "Group is now named %s", p2); */
                                sprintf (cp2, "%s renamed group to %s",
                                         u_tab[n].nickname, p2);
                                cp = cp2;
                            }
                            break;

                        case SET_QUESTION:
                            dump_table(n);
                            cp = NULL;
                            break;

                        default:
                            sprintf(mbuf, "Option %s unknown.", p2);
                            senderror(n,mbuf);
                            cp = NULL;
                    }

                    if (cp != NULL) 
                        if (strlen(cp) > 0) {
                            s_status_group(1,1,n,"Change",cp);
                        }
                }

                p1 = get_tail(p1);
            }
        }
    } else {
        mdb(MSG_INFO, "status: wrong number of parz");
    }
    return 0;
}

int s_topic(int n, int argc)
{
    int t_group;
    int is_moderator;
    int has_moderator;

    if (argc == 2) {
        t_group = find_group(u_tab[n].group);

        if (strlen(fields[1]) == 0) {
            if(strlen(g_tab[t_group].topic) == 0) {
                strcpy(mbuf,"The topic is not set.");
            } else {
                sprintf(mbuf,"The topic is: %s",
                        g_tab[t_group].topic);
            }
            sends_cmdout(n, mbuf);
            return 0;
        }

        is_moderator =(g_tab[t_group].mod == n);
        has_moderator = (g_tab[t_group].mod >= 0);

        if (has_moderator && (!is_moderator)) {
            senderror(n,"You aren't the moderator.");
            return 0;
        }

        /*
         * i eventually got tired of people complaining
         * about each other changing the topic in this group
         */
        if ( !strcmp (g_tab[t_group].name, "1") )
        {
            senderror (n,
                       "You can't change this group's topic.");
            return 0;
        }

        if ( !strcmp (g_tab[t_group].name, IDLE_GROUP) 
             || !strcmp (g_tab[t_group].name, BOOT_GROUP) )
        {
            senderror (n,
                       "You can't change this group's topic.");
            return 0;
        }

        memset(g_tab[t_group].topic, 0, MAX_TOPICLEN+1);
        strncpy(g_tab[t_group].topic, fields[1], MAX_TOPICLEN);
        if (g_tab[t_group].volume != QUIET) {
            sprintf(mbuf, "%s changed the topic to \"%s\"",
                    u_tab[n].nickname,
                    g_tab[t_group].topic);
            if(g_tab[t_group].volume != QUIET) {
                s_status_group(1,1,n,"Topic",mbuf);
            }
        }
    } else {
        mdb(MSG_INFO, "topic: wrong number of parz");
    }
    return 0;
}


/* send open group message */
/* loop through the table... */
/*   make sure they aren't us unless echoback is on */
/*   look for other user being in this user's group */
/* strings comparisons are done case-insensitive */
int s_send_group(int n)
{
    int i;
    char my_group[MAX_GROUPLEN+1];
    char one[255], two[255];

    strcpy(my_group, u_tab[n].group);
    sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
    ucaseit(one);
    strcpy(two, u_tab[n].nickname);
    ucaseit(two);

    /* send it to all of the real users */
    for (i=0; i<MAX_REAL_USERS; i++) {
        if ((i != n) || u_tab[n].echoback)
            if ((strcasecmp(my_group, u_tab[i].group) == 0) &&
                (!nlmatch(one, *u_tab[i].pub_s_hushed)) &&
                (!nlmatch(two, *u_tab[i].pub_n_hushed)) &&
                (u_tab[i].login == LOGIN_COMPLETE))
                sendopen(n, i, fields[0]);
    }
    return 0;
}

int s_exclude(int n, int argc)
{
    int i, j;
    char my_group[MAX_GROUPLEN+1];
    char one[255], two[255];

    if (argc == 2)
    {
        if (strlen(get_tail(fields[1])) > 0)
        {
            int gi = find_group(u_tab[n].group);
            if (gi != -1 && g_tab[gi].volume == QUIET) {
                senderror (n, "Open messages not permitted in quiet groups.");
                return 0;
            }

            strcpy(my_group, u_tab[n].group);
            sprintf(one, "%s@%s", u_tab[n].loginid, u_tab[n].nodeid);
            ucaseit(one);
            strcpy(two, u_tab[n].nickname);
            ucaseit(two);

            /* send it to all of the real users */
            for (i=0; i<MAX_REAL_USERS; i++)
            {
                if ((i != n) || u_tab[n].echoback)
                    if ((strcasecmp(my_group, u_tab[i].group) == 0) &&
                        (!nlmatch(one, *u_tab[i].pub_s_hushed)) &&
                        (!nlmatch(two, *u_tab[i].pub_n_hushed)) &&
                        (u_tab[i].login >= LOGIN_COMPLETE) && 
                        (strcasecmp(u_tab[i].nickname, getword(fields[1]))))
                    {
                        if ((j = find_user(getword(fields[1]))) > 0)
                            sprintf(mbuf, "%s's next message excludes %s:",
                                    u_tab[n].nickname, u_tab[j].nickname);
                        else
                            sprintf(mbuf, "%s's next message excludes %s:",
                                    u_tab[n].nickname, getword(fields[1]));
                        sendstatus(i, "Exclude", mbuf);
                        sendopen(n, i, get_tail(fields[1]));
                    }
            }
        } else { senderror(n, "Empty message."); }
    }
    return 0;
}

/* send group status message
 * loop through the table...
 *     look for other user being in this user's group
 * strings comparisons are done case-insensitive
 *
 * k        1 means n is u_tab index, otherwise n is g_tab index
 * tellme    0 means don't tell me (i.e. n), 1 means do tell me
 * n
 * class_string
 * message_string
 */
int s_status_group(int k, 
                   int tellme, int n, const char *class_string, const char *message_string)
{
    int i;
    char my_group[MAX_GROUPLEN+1];

    if (k==1) {
        strcpy(my_group, u_tab[n].group);
        if (strlen(my_group) == 0) {
            mdb(MSG_WARN, "bad u_tab entry");
        }
    } else {
        strcpy(my_group, g_tab[n].name);
        if (strlen(my_group) == 0) {
            mdb(MSG_WARN, "bad g_tab entry");
        }
    }

    /* do it only for real users */
    for (i=0; i<MAX_REAL_USERS; i++) {
        if (strcasecmp(my_group, u_tab[i].group) == 0) {
            /* n means something different here */
            /* for k=1 (u_tab entry) vs k=2 ( g_tab entry) */
            /* so n for k=2 doesn't make sense. */
            /* 'cuz it'd mean don't do this when user i was
               equal to group n...*/
            if (tellme || (k != 1) || ((k==1) && (i != n))) {
                /* actually send it */
                sendstatus(i,class_string, message_string);
            }
        }
    }
    return 0;
}


int talk_delete (char *who, int gi)
{
    int        deleted = 0,
            dest,
            in_registered = 0,
            in_normal = 0;

    if ( nlpresent (who, *g_tab[gi].n_talk) != 0 )
    {
        in_normal = 1;
        deleted++;
        nldelete (g_tab[gi].n_talk, who);
    }

    if ( nlpresent (who, *g_tab[gi].nr_talk) != 0 )
    {
        in_registered = 1;
        deleted++;
        nldelete (g_tab[gi].nr_talk, who);
    }

    if ( deleted > 0 )
    {
        if ( (dest = find_user(who)) >= 0 )
        {
            if ( in_normal ||
                 (in_registered && (strlen (u_tab[dest].realname) > 0)) )
            {
                sprintf(mbuf,
                        "You no longer can talk in group %s (no thanks to %s)",
                        u_tab[g_tab[gi].mod].group, u_tab[g_tab[gi].mod].nickname);
                sendstatus(dest,"TALK",mbuf);
            }
        }
    }

    return (deleted);
}

void talk_report (int n, int gi)
{
    if ( (nlpresent(u_tab[n].nickname, *g_tab[gi].n_talk) > 0) ||
         (nlpresent(u_tab[n].nickname, *g_tab[gi].nr_talk) &&
          (strlen(u_tab[n].realname) > 0)) )
    {
        sprintf (mbuf, "Talk permission in: %s",
                 g_tab[gi].name);
        sends_cmdout (n, mbuf);
    }
}


/*
 * s_talk()
 * - function to modify the group list of nicknames with permission to
 *   talk within a controlled group
 */
int s_talk(int n, int argc)
{
    int is_public,
        is_moderator;
    int gi;
    int dest;
    int r = -1,
        delete = -1,
        addmode = -1;
    int i;
    int quiet = -1;
    char *who;
    char *flags;
    const char *talk_usage = "Usage: talk {-a} {-q} {-r} {-d} nickname";

    if (argc == 2)
    {
        /* constraints:
           group is public & "talk"er is mod => 
           messages and change in invite list
           side-effects:
           if talkee is signed on, send status message
other:
fields[1] is nickname of person to invite
         */

        gi = find_group(u_tab[n].group);

        is_public = (g_tab[gi].control == PUBLIC);
        is_moderator = (g_tab[gi].mod == n);
        who = fields[1];
        while (who[0] == '-') 
        {
            flags = getword(who);
            for (i = 1; i < strlen(flags); i++)
                switch (flags[i]) {
                    case 'q':
                        if (quiet == -1)
                            quiet = 1;
                        else {
                            senderror(n, talk_usage);
                            return -1;
                        }
                        break;
                    case 'a':
                        if ( addmode == -1 )
                            addmode = 1;
                        else
                        {
                            senderror (n, talk_usage);
                            return -1;
                        }
                        break;

                    case 'd':
                        if ( delete == -1 )
                            delete = 1;
                        else
                        {
                            senderror (n, talk_usage);
                            return -1;
                        }
                        break;

                    case 'r':
                        if (r == -1)
                            r = 1;
                        else {
                            senderror(n, talk_usage);
                            return -1;
                        }
                        break;
                    default:
                        senderror(n, talk_usage);
                        return -1;
                }

            who = get_tail(who);
        }

        if (r == -1) r = 0;
        if (quiet == -1) quiet = 0;

        if ( who[0] == '\0' )
        {
            (void) senderror (n, talk_usage);
        }
        else if ( is_public )
        {
            (void) senderror (n,
                              "The group is public.");
        }
        else if ( !is_moderator )
        {
            (void) senderror (n, "You aren't the moderator.");
        }
        else /* !is_public && is_moderator */
        {
            /* if delete == 1, then we are removing this user */
            if ( delete == 1 )
            {
                int    deleted;

                deleted = talk_delete (who, gi);

                if ( deleted == 0 )
                {
                    sprintf (mbuf, "%s is not in talk list", who);
                    senderror (n, mbuf);
                }
                else if ( deleted > 0 )
                {
                    if ( quiet == 0 )
                    {
                        sprintf (mbuf,
                                 "%s removed from talk list", who);
                        sendstatus (n, "FYI", mbuf);
                    }

                }
            }
            else
            {
                /* if not adding to, remove all other entries */
                if ( addmode == -1 )
                {
                    NAMLIST    *nl;
                    int        count,
                            i;
                    char    *cp;

                    nl = g_tab[gi].n_talk;
                    count = nlcount(*nl);
                    for ( i = 0; i < count; i++)
                    {
                        cp = nlget (nl);
                        if ( talk_delete (cp, gi) > 0
                             && quiet == 0 )
                        {
                            sprintf (mbuf, "%s removed from talk list", cp);
                            sendstatus (n, "FYI", mbuf);
                        }
                    }

                    nl = g_tab[gi].nr_talk;
                    count = nlcount(*nl);
                    for ( i = 0; i < count; i++)
                    {
                        cp = nlget (nl);
                        if ( talk_delete (cp, gi) > 0
                             && quiet == 0 )
                        {
                            sprintf (mbuf,
                                     "%s removed from registered talk list", cp);
                            sendstatus (n, "FYI", mbuf);
                        }
                    }
                }

                if ( r == 0 )
                {
                    nlput(g_tab[gi].n_talk, who);

                    if (quiet == 0)
                    {
                        sprintf(mbuf, "%s can talk", who);
                        sendstatus(n,"FYI",mbuf);
                    }
                }
                else /* ( r == 1 ) */
                {
                    nlput(g_tab[gi].nr_talk, who);
                    if (quiet == 0)
                    {
                        sprintf(mbuf, "%s can talk (registered only)", who);
                        sendstatus(n,"FYI",mbuf);
                    }
                }

                if ( (dest = find_user(who)) >= 0 )
                {
                    if ( (strlen (u_tab[dest].realname) > 0) || (r == 0) )
                    {
                        sprintf(mbuf,
                                "You can talk in group %s (courtesy of %s)",
                                u_tab[n].group, u_tab[n].nickname);
                        sendstatus(dest,"TALK",mbuf);
                    }
                }
                else
                {
                    sprintf(mbuf, "%s is not signed on.", who);
                    sendstatus(n, "Warning", mbuf);
                }
            }
        }
    }
    else
    {
        mdb(MSG_INFO, "invite: wrong number of parz");
    }
    return 0;
}


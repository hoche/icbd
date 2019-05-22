/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include "server.h"
#include "externs.h"
#include "mdb.h"
#include "namelist.h"
#include "users.h"
#include "murgil/murgil.h" /* for disconnectuser() */
#include "murgil/globals.h"


#define mask(s) (1 << ((s)-1))

static int fdbits = howmany(FD_SETSIZE, NFDBITS);

const char *dumpfile = "icbd.dump";


/* icbopenlogs()
 *
 * open the log file with append
 */

void icbopenlogs(int sig)
{
    /* used to have non-portable O_FSYNC */
    if((icbd_log = open(ICBDLOG,O_WRONLY|O_CREAT|O_APPEND, 0644)) < 0) {
        fprintf(stderr, "icbd: could not open log file.\n");
        perror("ICBDLOG open");
        exit(1);
    }
}

/* icbcloselogs
 *
 * close the current log file
 */
void icbcloselogs(int sig)
{
    close(icbd_log);
    icbd_log = -1;
}

/* icbcyclelogs
 *
 * close and reopen the log file
 */
void icbcyclelogs(int sig)
{
    icbcloselogs(sig);
    icbopenlogs(sig);
}

/* icbexit
 *
 * disconnect all users and exit
 */
void icbexit(int sig)
{
    int user;

    mdb(MSG_ALL, "Someone told us to exit!");
    for (user=0; user < MAX_REAL_USERS; user++) {
        if (u_tab[user].login > LOGIN_FALSE) {
            disconnectuser(user);
        }
    }
    exit(0);
}


/* icbdump
 *
 * dump the current state to a file so if we restart we can pick up
 * where we left off.
 */
void icbdump(int sig)
{
    FILE *dump;
    int i, j, k;

    vmdb(MSG_ALL, "dumping current state to %s...", dumpfile);
    if ((dump = fopen(dumpfile, "w")) == NULL) {
        vmdb(MSG_ERR, "%s open: %s", dumpfile, strerror(errno));
        return;
    }

    for (i = 0; i < fdbits; i++)
#ifdef HAVE_XOPEN_FDS_BITS
        fprintf(dump, "%ld\n", rfdset.__fds_bits[i]);
#else
    fprintf(dump, "%ld\n", (long) rfdset.fds_bits[i]);
#endif

    for (i = 0; i < fdbits; i++)
#ifdef HAVE_XOPEN_FDS_BITS
        fprintf(dump, "%ld\n", ignorefdset.__fds_bits[i]);
#else
    fprintf(dump, "%ld\n", (long) ignorefdset.fds_bits[i]);
#endif

    fprintf(dump, "%d\n", port_fd);
#ifdef HAVE_SSL
    fprintf(dump, "%d\n", sslport);
    fprintf(dump, "%d\n", sslport_fd);
#endif
    fprintf(dump, "%d\n", highestfd);

    for (i = 0; i < MAX_USERS; i++)
        fprintf (dump, "%d\n", S_kill[i]);

    for (i = 0; i < MAX_USERS; i++)
    {
        fprintf(dump, "%sX\n", u_tab[i].loginid);
        fprintf(dump, "%sX\n", u_tab[i].nodeid);
        fprintf(dump, "%sX\n", u_tab[i].nickname);
        fprintf(dump, "%sX\n", u_tab[i].password);
        fprintf(dump, "%sX\n", u_tab[i].realname);
        fprintf(dump, "%sX\n", u_tab[i].group);
        fprintf(dump, "%sX\n", u_tab[i].awaymsg);
        fprintf(dump, "%d\n", u_tab[i].lastaway);
        fprintf(dump, "%ld\n", u_tab[i].lastawaytime);
        fprintf(dump, "%d\n", u_tab[i].login);
        fprintf(dump, "%d\n", u_tab[i].echoback);
        fprintf(dump, "%d\n", u_tab[i].nobeep);
        fprintf(dump, "%ld\n", u_tab[i].perms);
        fprintf(dump, "%d\n", u_tab[i].t_notify);
        fprintf(dump, "%ld\n", u_tab[i].t_on);
        fprintf(dump, "%ld\n", u_tab[i].t_sent);
        fprintf(dump, "%ld\n", u_tab[i].t_recv);
        fprintf(dump, "%ld\n", u_tab[i].t_group);
        k = nlcount(*u_tab[i].pri_n_hushed);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].pri_n_hushed));
        k = nlcount(*u_tab[i].pub_n_hushed);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].pub_n_hushed));
        k = nlcount(*u_tab[i].pri_s_hushed);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].pri_s_hushed));
        k = nlcount(*u_tab[i].pub_s_hushed);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].pub_s_hushed));
        k = nlcount(*u_tab[i].n_notifies);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].n_notifies));
        k = nlcount(*u_tab[i].s_notifies);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(u_tab[i].s_notifies));
    }

    for (i = 0; i < MAX_GROUPS; i++)
    {
        fprintf(dump, "%sX\n", g_tab[i].name);
        fprintf(dump, "%sX\n", g_tab[i].topic);
        fprintf(dump, "%d\n", g_tab[i].visibility);
        fprintf(dump, "%d\n", g_tab[i].control);
        fprintf(dump, "%d\n", g_tab[i].mod);
        fprintf(dump, "%ld\n", g_tab[i].modtimeout);
        fprintf(dump, "%sX\n", g_tab[i].missingmod);
        fprintf(dump, "%d\n", g_tab[i].volume);
        k = nlcount(*g_tab[i].n_invites);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].n_invites));
        k = nlcount(*g_tab[i].nr_invites);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].nr_invites));
        k = nlcount(*g_tab[i].s_invites);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].s_invites));
        k = nlcount(*g_tab[i].sr_invites);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].sr_invites));
        k = nlcount(*g_tab[i].n_bars);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].n_bars));
        k = nlcount(*g_tab[i].n_nr_bars);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].n_nr_bars));
        k = nlcount(*g_tab[i].s_bars);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].s_bars));
        k = nlcount(*g_tab[i].s_nr_bars);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].s_nr_bars));

        k = nlcount(*g_tab[i].n_talk);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].n_talk));

        k = nlcount(*g_tab[i].nr_talk);
        fprintf(dump, "%d\n", k);
        if (k > 0)
            for (j = 0; j < k; j++)
                fprintf(dump, "%sX\n", nlget(g_tab[i].nr_talk));

        fprintf(dump, "%d\n", g_tab[i].size);

        fprintf(dump, "%d\n", g_tab[i].idleboot);
        fprintf(dump, "%sX\n", g_tab[i].idleboot_msg);
        fprintf(dump, "%d\n", g_tab[i].idlemod);

    }

    fclose(dump);
    mdb(MSG_ALL, "state dumped.");
}

/* icbload
 *
 * load a server state from a file created by icbdump
 */
void icbload(int sig)
{
    FILE *dump;
    int i,j;
    long k;
    char name[256], name2[256];

    vmdb(MSG_ALL, "loading state from %s...", dumpfile);
    if ((dump = fopen(dumpfile, "r")) == NULL) {
        vmdb(MSG_ERR, "%s open: %s", dumpfile, strerror(errno));
        return;
    }

    init_groups();
    clear_users();

    for (i = 0; i < fdbits; i++) {
        fscanf(dump, "%ld\n", &k);
#ifdef HAVE_XOPEN_FDS_BITS
        rfdset.__fds_bits[i] = k;
#else
        rfdset.fds_bits[i] = k;
#endif
    }

    for (i = 0; i < fdbits; i++) {
        fscanf(dump, "%ld\n", &k);
#ifdef HAVE_XOPEN_FDS_BITS
        ignorefdset.__fds_bits[i] = k;
#else
        ignorefdset.fds_bits[i] = k;
#endif
    }

    fscanf(dump, "%d\n", &port_fd);
#ifdef HAVE_SSL
    fscanf(dump, "%d\n", &sslport);
    fscanf(dump, "%d\n", &sslport_fd);
#endif
    fscanf(dump, "%d\n", &highestfd);

    for (i = 0; i < MAX_USERS; i++) {
        fscanf(dump, "%ld\n", &k);
        S_kill[i] = k;
    }

    for (i = 0; i < MAX_USERS; i++)
    {
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].loginid, name, strlen(name) - 2);
        u_tab[i].loginid[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].nodeid, name, strlen(name) - 2);
        u_tab[i].nodeid[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].nickname, name, strlen(name) - 2);
        u_tab[i].nickname[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].password, name, strlen(name) - 2);
        u_tab[i].password[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].realname, name, strlen(name) - 2);
        u_tab[i].realname[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].group, name, strlen(name) - 2);
        u_tab[i].group[strlen(name)-2] = '\0';
        fgets(name, sizeof(name), dump);
        strncpy(u_tab[i].awaymsg, name, strlen(name) - 2);
        u_tab[i].awaymsg[strlen(name)-2] = '\0';
        fscanf(dump, "%d\n", &j);
        u_tab[i].lastaway = j;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].lastawaytime = j;
        fscanf(dump, "%d\n", &j);
        u_tab[i].login = j;
        fscanf(dump, "%d\n", &j);
        u_tab[i].echoback = j;
        fscanf(dump, "%d\n", &j);
        u_tab[i].nobeep = j;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].perms = k;
        fscanf(dump, "%d\n", &j);
        u_tab[i].t_notify = j;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].t_on = k;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].t_sent = k;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].t_recv = k;
        fscanf(dump, "%ld\n", &k);
        u_tab[i].t_group = k;
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].pri_n_hushed, name2);
        }
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].pub_n_hushed, name2);
        }
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].pri_s_hushed, name2);
        }
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].pub_s_hushed, name2);
        }
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].n_notifies, name2);
        }
        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(u_tab[i].s_notifies, name2);
        }
    }

    for (i = 0; i < MAX_GROUPS; i++)
    {
        fgets(name, sizeof (name), dump);
        strncpy(g_tab[i].name, name, strlen(name) - 2);
        g_tab[i].name[strlen(name)-2] = '\0';

        fgets(name, sizeof (name), dump);
        strncpy(g_tab[i].topic, name, strlen(name) - 2);
        g_tab[i].topic[strlen(name)-2] = '\0';

        fscanf(dump, "%d\n", &j);
        g_tab[i].visibility = j;

        fscanf(dump, "%d\n", &j);
        g_tab[i].control = j;

        fscanf(dump, "%d\n", &j);
        g_tab[i].mod = j;

        fscanf(dump, "%ld\n", &k);
        g_tab[i].modtimeout = k;

        fgets(name, sizeof (name), dump);
        strncpy(g_tab[i].missingmod, name, strlen(name) - 2);
        g_tab[i].missingmod[strlen(name)-2] = '\0';

        fscanf(dump, "%d\n", &j);
        g_tab[i].volume = j;

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].n_invites, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].nr_invites, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].s_invites, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].sr_invites, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].n_bars, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].n_nr_bars, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].s_bars, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--) {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].s_nr_bars, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--)
        {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].n_talk, name2);
        }

        fscanf(dump, "%d\n", &j);
        for (; j > 0; j--)
        {
            fgets(name, sizeof(name), dump);
            memset(name2, 0, sizeof(name2));
            strncpy(name2, name, strlen(name) - 2);
            nlput(g_tab[i].nr_talk, name2);
        }

        fscanf(dump, "%d\n", &j);
        g_tab[i].size = j;

        fscanf(dump, "%d\n", &j);
        g_tab[i].idleboot = j;

        fgets(name, sizeof(name), dump);
        strncpy(g_tab[i].idleboot_msg, name, strlen(name) - 2);
        g_tab[i].idleboot_msg[strlen(name)-2] = '\0';

        fscanf(dump, "%d\n", &j);
        g_tab[i].idlemod = j;
    }

    fclose(dump);
    unlink(dumpfile);
    mdb(MSG_ALL, "state loaded.");
}


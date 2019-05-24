/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <signal.h>
#include <string.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#if defined(HAVE_SYS_TIME_H) && defined(TIME_WITH_SYS_TIME)
#include <sys/time.h>
#endif
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <fcntl.h>

#include "version.h"
#include "server.h"
#include "externs.h"
#include "groups.h"
#include "protocol.h"
#include "mdb.h"
#include "icbutil.h"
#include "unix.h"
#include "users.h"
#include "access.h"
#include "send.h"
#include "s_stats.h"    /* for server_stats */

#include "murgil/murgil.h"
#include "murgil/globals.h"
#ifdef HAVE_SSL
#include "murgil/sslconf.h"
#endif

void trapsignals(void)
{
    /* cycle the logs on a restart signal */
    signal(SIGHUP, icbcyclelogs);

    /* exit on a hangup or terminate signal */
    signal(SIGTERM, icbexit);
    signal(SIGINT, icbexit);

    /* special internal signals to restart the server */
    signal(SIGUSR1, icbdump);
    signal(SIGUSR2, icbload);
}

int main(int argc, char* argv[])
{
    int c;
    extern char *optarg;
    int clearargsflg = 0;
    int forkflag = 0;
    int i;
    long TheTime;
    int pidf;
    char *bindhost = (char *) NULL;
    struct rlimit rlp;
    int quiet_restart = 0;
    int port;
#ifdef HAVE_SSL
    char *pem;
#endif

    icbd_log = -1; /* unopened logfile */
    log_level = 0; /* no messages */
    port = DEFAULT_PORT;
#ifdef HAVE_SSL
    sslport = 0;
    pem = ICBPEMFILE;
#endif

    /* save off args that we need in /restart */
    restart_argc = argc+2;    /* room for the -R and NULL args */
    restart_argv = (char **) malloc (restart_argc * sizeof (char *));

    if ( restart_argv == (char **)NULL )
    {
        perror ("malloc(restart_args)");
        exit (1);
    }

    for ( i = 0; i <= argc; i++ )
        restart_argv[i] = ((argv[i] == NULL) ? NULL : strdup(argv[i]));

    restart = 0;

    setbuf(stdout, (char *) 0);

    while ((c = getopt(argc, argv, "cl:p:s:fRqb:")) != EOF) {

        switch (c) {

            case 'l':
                log_level = atoi(optarg);
                break;

            case 'b':
                bindhost = optarg;
                break;

            case 'R':
                restart++;
                break;

            case 'q':
                quiet_restart++;
                break;

            case 'f':
                forkflag++;
                break;

            case 'c':
                clearargsflg++;
                break;

            case 'p':
                port = atoi(optarg);
                break;

            case 's':
#ifdef HAVE_SSL
                sslport = atoi(optarg);
                if (sslport == 0)
                    sslport = DEFAULT_SSLPORT;
#else
                puts("This version not compiled for SSL.");
#endif
                break;


            case '?':
            default:
                puts("usage: icbd [-b host] [-p port] [-cRfq]");
                puts("-c     wipe args from command line");
                puts("-R     restart mode");
                puts("-q     quiet mode (for restart)");
                puts("-l N     log level (none=0, err=1, warn=2, info=3, debug=4, verbose=5)");
                puts("-f     don't fork");
                puts("-p port     listen port (default is 7326)");
                puts("-s port     ssl listen port (default is 7327)");
                puts("-b host     bind socket to \"host\"");
                exit(-1);
                break;
        }
    }


#ifdef DEBUG
#warning "no-fork flag is set"
    forkflag++;
#ifdef HAVE_SSL
#warning "ssl is on"
    sslport = DEFAULT_SSLPORT;
#endif
#warning "log level is set to 5"
    log_level = 5;
#endif




#ifdef RLIMIT_NOFILE
    getrlimit(RLIMIT_NOFILE, &rlp);
    rlp.rlim_cur = MAX_USERS + 1;
    if (setrlimit(RLIMIT_NOFILE, &rlp) < 0)
    {
        perror("setrlimit");
        exit(1);
    }
#endif

    memset (thishost, '\0', sizeof (thishost));
    memset (&server_stats.start_time, '\0', sizeof (server_stats));
    time (&server_stats.start_time);

    if (restart == 0)
    {

        if (log_level >= 0)
            icbopenlogs(0);

        /* get our hostname */
        if ((gethostname(thishost,MAXHOSTNAMELEN)) < 0) {
            vmdb(MSG_ERR, "gethostname failed: %s", strerror(errno));
        }

#if defined(SHORT_HOSTNAME) && defined(FQDN)
        if (strcmp(thishost, SHORT_HOSTNAME) == 0)
            strncpy(thishost, FQDN, MAXHOSTNAMELEN);
#endif	/* SHORT_HOSTNAME && FQDN */

        /*
         * here's the scoop with this code:
         *
         * 1- don't want to remove rich's special case SHORT/FQDN code yet
         * 2- if BIND_HOSTNAME is set in config.h, bind to only that ip
         * 3- if -b arg is used, override with its value
         */

#ifdef BIND_HOSTNAME
        /* if bindhost is null (unset), let BIND_HOSTNAME set it */
        if ( (char *)NULL == bindhost )
        {
            bindhost = BIND_HOSTNAME;
            strncpy (thishost, BIND_HOSTNAME, MAXHOSTNAMELEN);
        }
#endif    /* BIND_HOSTNAME */

        if ( (char *) NULL != bindhost )
        {
            /*
             * if an actual host has been listed, reset thishost to it,
             * otherwise, we are mapping to all interfaces and using the
             * existing thishost value for the name
             */
            if ( '\0' != *bindhost )
                strncpy (thishost, bindhost, MAXHOSTNAMELEN);
        }

        if ((port_fd = makeport(bindhost, port)) < 0)
        {
            vmdb (MSG_ERR, "makeport failed: %s", strerror(errno));
            exit (-1);
        }

#ifdef HAVE_SSL
        if (sslport) {
            if (create_ssl_context(pem, &ctx) != 0) {
                mdb(MSG_ERR, "couldn't create SSL context. Do you have a valid PEM file?");
                exit (-1);
            }
            vmdb(MSG_INFO, "Using SSL PEM file %s.", pem);
            if ((sslport_fd = makeport(bindhost, sslport)) < 0) {
                vmdb(MSG_ERR, "makeport failed: %s", strerror(errno));
                exit (-1);
            }
            vmdb(MSG_INFO, "Using SSL port %d.", sslport);
        }
#endif

        if (forkflag == 0)
            if (fork()) exit(0);

        if (clearargsflg)
            clearargs(argc, argv);

        /* initialize out tables  */
        init_groups();
        clear_users();

        for (i = 0; i < MAX_REAL_USERS; i++)
            S_kill[i] = 0;

        /* invent our resident automagical user */
        TheTime = time(NULL);
        fill_user_entry(NICKSERV, "server",thishost,
                        "Server", "", "ICB", "", TheTime, 0, 1, PERM_NULL);
        u_tab[NICKSERV].login = LOGIN_COMPLETE;
        u_tab[NICKSERV].t_on = TheTime;
        u_tab[NICKSERV].t_recv = TheTime;
        strcpy(u_tab[NICKSERV].realname, "registered");
        fill_group_entry(0, "ICB", "...here to serve you!", SUPERSECRET,
                         RESTRICTED, NICKSERV, QUIET);
        nickwritetime(NICKSERV, 0, NULL);

        vmdb(MSG_INFO, "ICB revision %s on %s.", VERSION, thishost);
        vmdb(MSG_INFO, "There are %d users possible.", MAX_USERS);
        vmdb(MSG_INFO, "Of those, %d are real.", MAX_REAL_USERS);

        trapsignals();
    }
    else
    {
        extern int port_fd;
        extern char *getlocalname(int socketfd);
        char    *th = (char *)NULL;

        if (log_level >= 0)
            icbopenlogs(0);

        /* get our hostname */
        if ((gethostname(thishost,MAXHOSTNAMELEN)) < 0) {
            vmdb(MSG_ERR, "gethostname failed: %s", strerror(errno));
        }

#if defined(SHORT_HOSTNAME) && defined(FQDN)
        if (strcmp(thishost, SHORT_HOSTNAME) == 0)
            strncpy(thishost, FQDN, MAXHOSTNAMELEN);
#endif    /* SHORT_HOSTNAME && FQDN */

        init_groups();
        clear_users();
        trapsignals(); 
        mdb(MSG_ALL, "[RESTART] Reloading server data");
        icbload(0);

        if ( (th = getlocalname (port_fd)) != (char *) NULL )
            strncpy(thishost, th, MAXHOSTNAMELEN);

        for (i = 0; i < MAX_USERS; i++)
            if (u_tab[i].login > LOGIN_FALSE)
            {
                if ( ! quiet_restart ||
                     (strcasecmp (u_tab[i].nickname, "admin") == 0) )
                {
                    sendimport (i,
                                quiet_restart == 0 ? "Restart" : "Quiet-Restart",
                                "Server is back up.");
                }
            }
    }

    if ((pidf = open(PID_FILE, O_WRONLY|O_CREAT|O_TRUNC,
                         S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) >= 0)
    {
        /* write the process id on line 1 */
        sprintf(mbuf, "%d\n", (int)getpid());
        write(pidf, mbuf, strlen(mbuf));
        fchmod (pidf, 0644);
        close(pidf);
    }

#ifdef RLIMIT_NOFILE
    getrlimit(RLIMIT_NOFILE, &rlp);
    vmdb(MSG_INFO, "Max fds set to %d", (int)(rlp.rlim_cur));
#endif

    for (i = 0; i < MAX_REAL_USERS; i++)
    {
        lpriv_id[i] = -1;
        pong_req[i] = -1;
        timerclear (&ping_time[i]);
    }

    /* start the serve loop */
    serverserve();

    /* all's well that ends */
    return 0;
}


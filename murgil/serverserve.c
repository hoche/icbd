/*
 * serverserve.c
 *
 * main event loop for an SSL-enabled select-based server.
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2000, 2001, Michel Hoche-Mong and jon r. luini
 *
 * Released under the GPL license.
 *
 * Loosely based on code originally created by Carrick Sean Casey, as well
 *     as code provided by W. Richard Stevens (RIP).
 *
 * $Id: serverserve.c,v 1.18 2001/08/04 11:03:06 hoche Exp $
 *
 */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <signal.h>
#include <errno.h>
#include <string.h>

#include "globals.h"
#include "cbuf.h"
#include "murgil.h"
#include "server/strlist.h"
#include "server/msgs.h" /* for ok2read() */
#include "server/ipcf.h" /* for s_didpoll() */
#include "server/mdb.h"  /* for mdb() */

/* globals and file static vars */
extern int restart;

int _newconnect(int s, int is_ssl);
int _readpacket(int user, struct cbuf_t *p);
int _writepacket(struct cbuf_t* cbuf);

fd_set rfdr, wfdr, efdr;

void clear_exceptions(void)
{
    register int x;

    /* disconnect users in the exception set */
    for (x = 1; x <= highestfd; x++) {
	if (FD_ISSET(x, &efdr))
	    disconnectuser(x);
    }
}

void handle_fd_activity(void)
{
    int n;
    register int x;

    /* handle any users in the exception set */
    clear_exceptions();

    /* look for a new user on our listen socket */
    if (FD_ISSET(port_fd, &rfdr)) {
	if ((n = _newconnect(port_fd, 0 )) > 0) {
	    s_new_user(n); /* let server init the user */
	}
	FD_CLR(port_fd, &rfdr);
    }

#ifdef HAVE_SSL
    /* look for a new user on our SSL listen socket */
    if (FD_ISSET(sslport_fd, &rfdr)) {
	if ((n = _newconnect(sslport_fd, 1 )) > 0) {
	    s_new_user(n); /* let server init the user */
	}
	FD_CLR(sslport_fd, &rfdr);
    }
#endif

    /* examine read and write set of file descriptors */
    for (x = 1; x <= highestfd; x++)
    {
	/* is there a pending write? */
	if (FD_ISSET(x, &wfdr)) {

	    /* if we're still waiting for a successful SSL_accept, we
	     * shunt off to _newconnect again */
	    if (cbufs[x].want_ssl_accept) {
	      if ((n = _newconnect(x, 1 )) > 0) {
		  s_new_user(n); /* let server init the user */
	      }
	      break;
	    }

	    if (_writepacket(&cbufs[x]) < 0) {
		vmdb(MSG_WARN, "fd%d send error. disconnecting user.", x);
		disconnectuser(x);
		break;
	    }
	}

	/* is there a pending read? */
	if (FD_ISSET(x, &rfdr)) {

	    /* if we're still waiting for a successful SSL_accept, we
	     * shunt off to _newconnect again */
	    if (cbufs[x].want_ssl_accept) {
	      if ((n = _newconnect(x, 1 )) > 0) {
		  s_new_user(n); /* let server init the user */
	      }
	      break;
	    }

	    if ( ok2read(x) == 1 ) {
		switch(_readpacket(x, &cbufs[x])) {
		    case  1:
			/* complete packet */
			s_packet(x, cbufs[x].rbuf->data);
			break;
		    case  0:
			/* incomplete packet */
			break;
		    case -1:
		    /* error */
			{
			    int saveerr = errno;
			    vmdb(MSG_ERR, "fd#%d: _readpacket error: %s", x,
				    strerror (saveerr));
			    disconnectuser(x);
			}
			break;
		    case -2:
			/* close connection */
			disconnectuser(x);
			break;
		}
	    } else {
		/* add to ignore and remove from fdset */
		ignore(x);
	    }
	}
    }
}


/* handle any idle-time chores
 *
 */
void handle_idle(void)
{
    register int i;

    s_didpoll(0);

    /* for all users, check ignore/held state */
    for (i = 0; i < MAX_USERS; i++) {
	if (FD_ISSET(i, &ignorefdset)) {
	    /* if it's held and it's ok to read from it again, unignore it */
	    if ( ok2read(i) == 1 )
		unignore(i);
	}
    }
}

/* clear out cbufs. this should go elsewhere. */
void init_cbufs(void)
{
  register int i;

  for (i = 0; i < MAX_USERS; i++) {
      cbufs[i].newmsg = 1;
      TAILQ_INIT(&(cbufs[i].wlist));
  }
}

/* serverserver()
 *
 * The main server loop. Polls the set of file descriptors looking for 
 * active sockets. Sockets with activity get handed off to handle_fd_activity().
 * If there is no activity after a period of time, handle_idle() gets called.
 *
 */

void serverserve(void)
{
    int ret;
    long loopcount = 0; /* used to break loop for periodic s_didpoll() */
    struct timeval t;
    struct timeval *poll_timeout;

    /* initialize our fdsets and cbuf array */
    FD_ZERO(&wfdset);
    if (restart == 0) {
	FD_ZERO(&rfdset);
	FD_ZERO(&ignorefdset);
	FD_SET(port_fd, &rfdset);
#ifdef HAVE_SSL
	FD_SET(sslport_fd, &rfdset);
#endif
    }

    init_cbufs();

    if (port_fd > highestfd)
	highestfd = port_fd;

#ifdef HAVE_SSL
    if (sslport_fd > highestfd)
	highestfd = sslport_fd;
#endif

    for (;;) {
	loopcount++;
	memcpy(&rfdr, &rfdset, sizeof(rfdset));
	memcpy(&efdr, &rfdset, sizeof(rfdset));
	memcpy(&wfdr, &wfdset, sizeof(wfdset));

	/*
	 * must re-set this each time through the loop because 
	 * select isn't guaranteed to not mangle it
	 */
	if (POLL_TIMEOUT < 0) 
	    poll_timeout = NULL;
	else {
	    t.tv_sec = POLL_TIMEOUT;
	    t.tv_usec = 0;
	    poll_timeout = &t;
	}

	ret = select(highestfd+1, &rfdr, &wfdr, &efdr, poll_timeout);
	if (ret < 0) {
	    if (errno == EINTR)
		ret = 0;
	    else
		vmdb(MSG_ERR, "select: %s", strerror(errno));
	}

	if (ret > 0)
	    handle_fd_activity();

	if (ret == 0 || loopcount%10==0 ) {
	    loopcount = 0;
	    handle_idle();
	}
    }
}


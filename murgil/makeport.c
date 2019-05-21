/* Copyright (c) 1988 Carrick Sean Casey. All rights reserved. */

#include "config.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <netinet/in.h>
#include <netdb.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <string.h>

#include "globals.h"


/* start listening on the port designated by port_number */
/* returns the listener socket number */
int makeport(char *host_name, int port_number)
{
	struct sockaddr_in saddr;
	int one = 1;
	int s;
	int flags;

	memset(&saddr, 0, sizeof(saddr));

	/*
	 * if host_name is NULL or empty, we bind to any address, otherwise
	 * we bind to just the one listed 
	 */
	if ( (char *)NULL != host_name && '\0' != host_name[0] )
	{
	    struct hostent *hp;

	    if ((hp = gethostbyname(host_name)) == (struct hostent *) 0) {
		    perror("makeport: gethostbyname");
		    return(-1);
	    }

	    memcpy (&saddr.sin_addr, hp->h_addr_list[0], hp->h_length);
	}
	else
	{
	    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
	}

	/* insert host_name into address */
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons(port_number);

	/* create a socket */
	if ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("makeport: gethostbyname");
		return(-1);
	}

	/* bind it to the inet address */
	if (bind(s, (struct sockaddr *) &saddr, sizeof(saddr)) < 0) {
		perror("makeport: bind");
		perror("perror says:");
		return(-1);
	}

	/* start listening for connections */
	listen(s, 5);

	/* force occasional connection check */
	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&one, sizeof(one)) < 0) {
		puts("makeport:setsockopt (makeport)");
		/* return(-1);*/
	}

	/* make it non-blocking */
	if (fcntl(s, F_SETFL, O_NONBLOCK) < 0) {
		perror("makeport: fcntl");
		return(-1);
	}

	/* Don't close on exec */
	flags = fcntl(s, F_GETFD, 0);
	flags = flags & ~ FD_CLOEXEC;
	if (fcntl(s, F_SETFD, flags) < 0) {
		perror("CLOEXEC");
		exit (-1);
	}

	one = 24576;
	if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&one, sizeof(one)) < 0) {
		perror("SO_SNDBUF");
	}

	/* allow us to handle problems gracefully */
	signal(SIGPIPE, SIG_IGN);

	return s;
}


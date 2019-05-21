/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to handle information requests */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include "server.h"
#include "externs.h"
#include "mdb.h"
#include "send.h"

#define MAX_NEWS_FILES 10

int s_news(int n, int argc)
{
	int             i, j, first;
	int             news_fd;
	char            fname[80], c, tbuf[255];

	first = 0;
	/*
	 * I don't know where clients get this (null) from, but we might as
	 * well take care of it
	 */
	if ((strlen(fields[1]) == 0) || (strcmp(fields[1], "(null)") == 0)) {
		for (i = 1; i < MAX_NEWS_FILES; i++) {
			memset(tbuf, 0, 255);
			sprintf(fname, "news.%d", i);
			if ((news_fd = open(fname, O_RDONLY)) >= 0) {
				if (first == 0) {
					first++;
					sends_cmdout(n, "--------------------------------------");
				}
				while ((j = read(news_fd, &c, 1)) > 0) {
					if (c == '\012') {
						sends_cmdout(n, tbuf);
						memset(tbuf, 0, 255);
						}
					else strncat(tbuf, &c, 1);
				}
				if (close(news_fd) != 0) {
					sprintf(mbuf, "News File Close: %s", strerror(errno));
					mdb(MSG_ERR, mbuf);
					}
				sends_cmdout(n, "--------------------------------------");
			}
		}
		if (first == 0)
			sendstatus(n, "News", "No news.");
	} else {
		sprintf(fname, "news.%s", fields[1]);
		if ((news_fd = open(fname, O_RDONLY)) >= 0) {
			sends_cmdout(n, "--------------------------------------");
			memset(tbuf, 0, 255);
			while ((j = read(news_fd, &c, 1)) > 0) {
				if (c == '\012') {
					sends_cmdout(n, tbuf);
					memset(tbuf, 0, 255);
					}
				else strncat(tbuf, &c, 1);
			}
			sends_cmdout(n, "--------------------------------------");
			if (close(news_fd) != 0) {
				sprintf(mbuf, "News File Close: %s", strerror(errno));
				mdb(MSG_ERR, mbuf);
				}
		} else
			senderror(n, "Entry not found.");
	}
	return 0;
}

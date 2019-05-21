/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to print the version */

#include "config.h"

#include <stdio.h>
#include <string.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "server.h"
#include "externs.h"
#include "send.h"
#include "mdb.h"

int s_shuttime(int n, int argc)
{
	extern long TimeToDie;
	long TheTime, TimeLeft;
	char line[256];

        if (argc == 2)
		if (TimeToDie < 0.0)
			sendstatus(n, "Shutdown", "No shutdown time scheduled.");
		else {
			memset(line, 0, 256);
			TheTime = time(NULL);
			TimeLeft = TimeToDie - TheTime;
			if (TimeLeft >= 3600.0) 
				sprintf(line, "%d hour(s), %d minute(s) left until scheduled shutdown.", (int) (TimeLeft / 3600.0), (int) (((int) TimeLeft % 3600) / 60.0));
			else if (TimeLeft > 60.0)
				sprintf(line, "%d minute(s) left until scheduled shutdown.", (int) (TimeLeft / 60.0));
			else
				sprintf(line, "%d second(s) left until scheduled shutdown!", (int) TimeLeft);
			sendstatus(n, "Shutdown", line);
		}
        else {
                mdb(MSG_INFO, "version: wrong number of parz");
        }
	return 0;
}

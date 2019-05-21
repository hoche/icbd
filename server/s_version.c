/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to print the version */

#include "config.h"

#include <stdio.h>
#include <string.h>

#include "version.h"
#include "server.h"
#include "externs.h"
#include "send.h"
#include "mdb.h"

int s_version(int n, int argc)
{
        if (argc == 2) {
		sprintf(mbuf, "%s", VERSION);
		sends_cmdout(n, mbuf);
		sprintf(mbuf, "Proto Level: %d Max Users: %d Max Groups: %d", 
			PROTO_LEVEL, MAX_REAL_USERS, MAX_GROUPS);
		sends_cmdout(n, mbuf);
        } else {
			mdb(MSG_INFO, "version: wrong number of parz");
        }

	return 0;
}

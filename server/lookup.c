/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#include <string.h>


#include "lookup.h"

/* look up a string in the command table */
/* case insensitive compares */

int lookup(char *s, char *table[])
{
	int i;
	int result = -1;

        for (i = 0; table[i] != 0; i++) {
		if(strcasecmp(s, table[i]) == 0) {
			result = i;
			break;
		}
	}
	return result;
}


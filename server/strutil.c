/* Copyright (c) 1990 by Carrick Sean Casey. */
/* Modified 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#include <ctype.h>
#include <varargs.h>

#include "server.h"
#include "externs.h"

extern char *charmap;

#define OKGROUPCHARS	"-.!'$+,/?_~"
#define OKNICKCHARS	"-.!'$+,/?_@"

/* replace illegal characters in a nickname */

void filternickname(char *txt)
{
	for (; *txt != '\0'; txt++) {
		if ((*txt >= 'A' && *txt <= 'Z') ||
		  (*txt >= 'a' && *txt <= 'z') ||
		  (*txt >= '0' && *txt <= '9'))
			continue;
		if (!strchr(OKNICKCHARS, *txt)) {
			if (*txt == ' ')
				*txt = '_';
			else
				*txt = '?';
		}
	}
}


/* replace illegal characters in a regular line of text */

void filtertext(char *s)
{
	for (;*s != '\0'; s++)
		if (!(*s >= ' ' && *s < '\177'))
			*s = '?';
}

/* replace illegal characters from a groupname */

void filtergroupname(char *txt)
{
	for (; *txt != '\0'; txt++) {
		if ((*txt >= 'A' && *txt <= 'Z') ||
		  (*txt >= 'a' && *txt <= 'z') ||
		  (*txt >= '0' && *txt <= '9'))
			continue;

		if (!strchr(OKGROUPCHARS, *txt)) {
			if (*txt == ' ')
				*txt = '_';
			else
				*txt = '?';
		}
	}
}

/*
 * filterfmt() - checks newstr for n occurances of %s
 *   n is replaced with the number of %s's upon return
 *      (-1 is used for existence of other % substitutions)
 *   returns NULL if it's ok
 *   otherwise returns error string
 *
 * used by: SET_IDLEBOOT_MSG and s_boot
 */
char *filterfmt (char *newstr, int *n)
{
    char	*cp;
    int num_s = 0, num_bad = 0;
    static char	one[] = "Message must contain (only) %d %%s for the username.",
		bad[] = "Message can't include %d, %f and such. Use %% to quote a single %.",
		errstr[256];

    /* make sure there's 1 and only one %s */
    cp = strchr(newstr, '%');
    while ( cp != NULL )
    {
	++cp;
	if ( *cp == 's' )
	    num_s++;
	else if ( *cp == '%' )
	    ++cp;
	else
	    num_bad++;
	cp = strchr(cp, '%');
    }

    if ( num_bad > 0 )
    {
	*n = -1;
	return (bad);
    }

    if ( num_s != *n )
    {
	if ( *n == 0 )
	    strcpy (errstr, bad);
	else
	    sprintf (errstr, one, *n);
	*n = num_s;
	return (errstr);
    }

    return (NULL);
}


/* return 1 if a string is a number */
/* else return 0 */
int numeric(char *txt)
{
	for (;*txt != '\0'; txt++)
		if (!(*txt >= '0' && *txt <= '9'))
			return(0);
	return(1);
}

#define UC(x) ((x >= 'a' && x <= 'z') ? x & ~040 : x)


/* convert a string to lower case */
void lcaseit(char *s)
{
	for (; *s; s++)
		if (*s >= 'A' && *s <= 'Z')
			 *s |= 040;
}

/* convert a string to upper case */
void ucaseit(char *s)
{
	for (; *s; s++)
		if (*s >= 'a' && *s <= 'z')
			 *s ^= 040;
}

/* return how many characters in string1 matched string2 */
int cimatch(char *s1, char *s2)
{
	int count = 0;

	for (; *s1 && *s2 && (*s1 == *s2); s1++, s2++)
		count++;
	return(count);

}

/* put a string in quotes */
/* puts backslashes before all special chars appearing in the string */
/* doesn't interfere with already backslashed chars */

const char *special = "{}[]\";$\\";

void quoteify(char *a, char *b)
{
	while (*a != '\0') {
		if (strchr(special, *a)) {
			*b++ = '\\';
			*b++ = *a;
		} else
			*b++ = *a;
		a++;
	}
	*b = '\0';
}


void catargs(char **argv)
{
	char *s, *p = mbuf;

	while (*argv != NULL) {
		s = *argv;
		while (*s) *p++ = *s++;
		if (*(argv+1) != NULL)
			*p++ = ' ';
		argv++;
	}
	*p = '\0';
}

int wordcmp(char *s1, char *s2)
{
	while(*s1 == *s2++)
		if (*s1 == '\0' || *s1 == ' ' || *s1++ == '\t')
			return(0);
	if (*s1 == ' ' && *(s2-1) == '\0')
		return(0);
	return(*s1 - *--s2);
}

char *getword(char *s)
{
	static char word[64];
	char *w = word;
	while (*s != ' ' && *s != '\t' && *s != '\0' && ((w - word) < (sizeof(word) - 1)))
		*w++ = *s++;
	*w = '\0';
	return(word);
}

char *get_tail(char *s)
{
	/* skip first word */
        while (*s != ' ' && *s != '\t' && *s != '\0' )
                s++;
	/* skip white space */
        while ((*s == ' ' || *s == '\t') && *s != '\0' )
                s++;
        return(s);
}


/* Read a line containing zero or more colons. Split the string into */
/* an array of strings, and return the number of fields found. */

char *fields[MAX_FIELDS];

int split(char *s)
{
	char *p = s;
	int i = 0;

	fields[i] = s;
	for(;;) {

		i++;

                /*
		 * prevent writing off the end of our memory!
                 * normally we'd return an error code of some
                 * so the calling routine could know something bad
                 * happened, but some code still calls this without
                 * bothering to check the return values, so until that
                 * is all cleaned up, we just stop at the MAX_FIELDS
                 * which is only slightly better.
                 */
                if ( i >= MAX_FIELDS )
                    return (MAX_FIELDS);

		/* find delim or EOS */
		while(*p != '\001' && *p != '\0') p++;

		if (*p == '\001') {
			/* got delim */	
			*p = '\0';
			fields[i] = ++p;
		} else
			return(i);
	}
}

/*
 * Copyright (c) 1990 by Carrick Sean Casey.
 * Modified 1991 by John Atwood deVries II.
 * Modified 2019 by Michel Hoche-Mong, stripping out a bunch of archaic
 *   unused stuff and adding in utf-8 capabilities.
 *
 * We allow utf-8 in messages (if enabled), but we still only allow a
 * subset of characters for nick and group names. The main reason for
 * this is that we've always been case-insensitive for those.
 * 
 */

#include "config.h"

#include <ctype.h>
#include <stdarg.h>

#include "utf8.h"

#include "server.h"
#include "externs.h"

extern char *charmap;

#define OKGROUPCHARS    "-.!'$+,/?_~"
#define OKNICKCHARS    "-.!'$+,/?_@"


/* replace illegal characters in a nickname.
 */
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

#if ALLOW_UTF8_MESSAGES
void filtertext(char *text)
{
    uint32_t state = UTF8_ACCEPT;
    size_t len = strlen(text);

    /* still do the old-style scan, prohibiiting things below 0x20.  */
    char *s = text;
    for (;*s != '\0'; s++)
        if (*s >= 0x00 && *s < 0x20)
            *s = '?';

    if (validate_utf8(&state, text, len) == UTF8_REJECT) {
        snprintf(s, len, "%.*s", (int)len, "?????????????????????????????????????");
    }
}
#else
/* replace illegal characters in a regular line of text. Assumes
 * the line is null-terminated.
 */
void filtertext(char *text)
{
    char *s = text;
    for (;*s != '\0'; s++)
        if (!(*s >= 0x20 && *s < 0x7E))
            *s = '?';
}
#endif

/* replace illegal characters in a groupname.
 */
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
    char    *cp;
    int num_s = 0, num_bad = 0;
    static char    one[] = "Message must contain (only) %d %%s for the username.",
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

char *getword(char *s)
{
    static char word[64];
    char *w = word;
    while (*s != ' ' && *s != '\t' && *s != '\0' && ((w - word) < 64))
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


/* Read a line containing zero or more \001's (^A's). Split the
 * string into an array of strings, and return the number of fields
 * found.
 */

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
        if ( i > MAX_FIELDS )
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

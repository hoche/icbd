#pragma once

#include <ctype.h>
#include <stdarg.h>


/* replace illegal characters in a nickname */
void filternickname(char *txt);

/* replace illegal characters in a regular line of text */
void filtertext(char *s);

/* replace illegal characters from a groupname */
void filtergroupname(char *txt);

/*
 * filterfmt() - checks newstr for n occurances of %s
 *   n is replaced with the number of %s's upon return
 *      (-1 is used for existence of other % substitutions)
 *   returns NULL if it's ok
 *   otherwise returns error string
 */
char *filterfmt (char *newstr, int *n);

/* convert a string to lower case */
void lcaseit(char *s);

/* convert a string to upper case */
void ucaseit(char *s);

char *getword(char *s);

char *get_tail(char *s);

/* Read a line containing zero or more colons. Split the string into */
/* an array of strings, and return the number of fields found. */

int split(char *s);

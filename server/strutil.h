#ifndef _S_STRINGS_H_
#define _S_STRINGS_H_

#include <ctype.h>
#include <varargs.h>


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

/* return 1 if a string is a number */
/* else return 0 */
int numeric(char *txt);

/* convert a string to lower case */
void lcaseit(char *s);

/* convert a string to upper case */
void ucaseit(char *s);

/* return how many characters in string1 matched string2 */
int cimatch(char *s1, char *s2);

/* put a string in quotes */
/* puts backslashes before all special chars appearing in the string */
/* doesn't interfere with already backslashed chars */


void quoteify(char *a, char *b);


void catargs(char **argv);

int *wordcmp(char *s1, char *s2);

char *getword(char *s);

char *get_tail(char *s);

/* Read a line containing zero or more colons. Split the string into */
/* an array of strings, and return the number of fields found. */

int split(char *s);

#endif /* #define _S_STRINGS_H_ */

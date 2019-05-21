#ifndef _STRLIST_H_
#define _STRLIST_H_


/* routines to maintain a generic linked list of strings */

typedef struct Strlist {
	struct Strlist *next;
	struct Strlist *prev;
	char str[1];
} STRLIST;


STRLIST *strmakenode(int size);

/* link node s to the head of the list */
void strlinkhead(STRLIST *s, STRLIST **head, STRLIST **tail);

/* link node s to the tail of the list */
void strlinktail(STRLIST *s, STRLIST **head, STRLIST **tail);

/* link node s in after node i */
/* node i must be defined */
void strlinkafter(STRLIST *s, STRLIST *i, STRLIST **head, STRLIST **tail);


/* link node s in before node i */
/* node i must be defined */
void strlinkbefore(STRLIST *s, STRLIST *i, STRLIST **head, STRLIST **tail);


/* unlink node s */
void strunlink(STRLIST *s, STRLIST **head, STRLIST **tail);

/* link s into the list such that it is inserted in alphabetical order */
/* if caseindep != 0, it is done case independently */
void strlinkalpha(STRLIST *s, STRLIST **head, STRLIST **tail, int caseindep);

/* get the node in a STRLIST containing str */
/* if caseindep != 0, the searching is done case insensitive */
/* returns pointer on success, 0 on failure */
STRLIST *strgetnode(char *str, STRLIST *head, int caseindep, int pattern);

#endif /* #ifndef _STRLIST_H_ */

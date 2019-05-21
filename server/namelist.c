/* Copyright (c) 1990 by Carrick Sean Casey. */
/* Modifications Copyright (c) 1991 by John Atwood deVries II */
/* For copying and distribution information, see the file COPYING. */

#include "config.h"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "namelist.h"
#include "strutil.h"
#include "mdb.h"

/* name list routines */

/* add a name to the list */

void nlput(NAMLIST *nl, char *name)
{
	STRLIST *sp;

	/* hunt for user within list */
	for (sp = nl->head; sp; sp = sp->next)
		if (!strcasecmp(name, sp->str)) {
			/* found user -- put at head of list */
			strunlink(sp, &nl->head, &nl->tail);
			strlinkhead(sp, &nl->head, &nl->tail);
			nl->p = nl->head;
			return;
		}

	/* user wasn't found */
	if (nl->num < nl->max) {
		/* make a new entry for the user */
		if ((sp = (STRLIST *) malloc (sizeof(STRLIST) + strlen(name))) == NULL) {
			mdb(MSG_ERR, "nlput out of memory for names");
			return;
		}
		strcpy(sp->str, name);
		strlinkhead(sp, &nl->head, &nl->tail);
		nl->num++;
	} else {
		/* name list full, link user to head, remove tail */
		sp = nl->tail;
		strunlink(sp, &nl->head, &nl->tail);
		free(sp);
		nl->num--;
		if ((sp = (STRLIST *)
		 malloc (sizeof(STRLIST) + strlen(name))) == NULL) {
			mdb(MSG_ERR, "nlput out of memory for names");
			return;
		}
		strcpy(sp->str, name);
		strlinkhead(sp, &nl->head, &nl->tail);
		nl->num++;
	}
	nl->p = nl->head;
}

/* return a name */
/* repeatedly called, will cycle through entries */

char *nlget(NAMLIST *nl)
{
	STRLIST *p = nl->p;

	nl->p = nl->p->next;
	if (nl->p == 0)
		nl->p = nl->head;
	return(p->str);
}
	

/* return number of names in name list */

unsigned int nlcount(NAMLIST nl)
{
	return(nl.num);
}

void nlclear(NAMLIST *nl)
{
	STRLIST *tmp, *p;

	if (nl) {
		p = nl->p;
		while (p) {
			tmp = p->next;
			free(p);
			p = tmp;
		}
	}
	nl->num = 0;
	nl->p = nl->head = nl->tail = 0;
}

void nlinit(NAMLIST *nl, int max)
{
        nl->num = 0;
        nl->p = nl->head = nl->tail = 0;
	nl->max = max;
}

int nldelete(NAMLIST *nl, char *name)
{
	STRLIST * namep;
	namep = strgetnode(name, nl->head, 1, 0);
	if (namep != NULL) {
		strunlink(namep, &nl->head, &nl->tail);
		nl->p= nl->head;
		if (nl->num > 0) {
			nl->num--;
			if (nl->num == 0)
				nl->p = 0; /* just in case */
		} else {
			mdb(MSG_ERR, "tried to make nl.num negative");
		}
		return 0;
	} else {
		return -1;
	}
}

int nlpresent(char name[], NAMLIST nl)
{
	if (nl.num > 0) return (strgetnode(name, nl.head, 1, 0) != NULL);
		else	return (NULL != NULL);
}

int nlmatch(char name[], NAMLIST nl)
{
	if (nl.num > 0) return (strgetnode(name, nl.head, 1, 1) != NULL);
		else	return (NULL != NULL);
}

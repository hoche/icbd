#include "server.h"
#include "externs.h"
#include "config.h"
#include "icbdb.h"
#include "strutil.h"
#include "mdb.h"
#include <strings.h>
#ifdef HAVE_NDBM_H
#include <ndbm.h>
#elif defined (HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined (HAVE_DB1_NDBM_H)
#include <db1/ndbm.h>
#endif
#include <fcntl.h>
#include <time.h>
#include <stdlib.h>
#include <errno.h>

#ifndef DBLKSIZ
#define DBLKSIZ	4096
#endif

static DBM	*db = NULL;
static char	databuf[DBLKSIZ];

static int	open_count = 0;

/*
 * This can be set to 1 by an the caller to force a dbm_close()
 * after every operation.  One way to do that would be via a
 * signal that an external process could send to inform us that
 * they're messing with the db, and another to say they're done
 * (which would set icbdb_multiuser back to 0).
 */
static int	icbdb_multiuser = 0;

void
icbdb_set_multiuser (int value)
{
    vmdb (MSG_DEBUG, "setting icbd_multiuser to %d", value); /* DeBuG */
    icbdb_multiuser = value;

    //
    // We want to close the database if it's currently being held open.
    // We accomplish this by adding one to the reference count and
    // closing the database, which will really close it if no one
    // else is currently using it.
    //
    open_count++;
    icbdb_close();
}

int
icbdb_open (void)
{
    open_count++;

    if (db == NULL)
    {
	if ((db = dbm_open (USERDB, O_RDWR, ICBDB_MODE)) == NULL)
	    vmdb (MSG_ERR, "User Database Open: %s", strerror(errno));
    }

    return (db != NULL);
}

void
icbdb_close (void)
{
    if (--open_count > 0 || !icbdb_multiuser)
    {
	return;
    }

    if (db != NULL)
    {
	dbm_close (db);
    }

    db = NULL;
}

#define ICBDB_OPEN()	if (icbdb_open() == 0) return (0)

#define ICBDB_DONE(r)	icbdb_close (); return (r)

static void
icbdb_make_key (const char *category, const char *attribute, datum *key)
{
    static char	keybuf[DBLKSIZ];

    if (category == NULL)
    {
	/* This handles the terminating NULL byte better than strncpy. */
	snprintf (keybuf, sizeof (keybuf), "%s", attribute);
    }
    else
    {
	snprintf (keybuf, sizeof (keybuf), "%s.%s", category, attribute);
    }

    lcaseit (keybuf);

    key->dptr = keybuf;
    key->dsize = strlen (keybuf);
}

int
icbdb_get (const char *category, const char *attribute, icbdb_type type,
	void *value)
{
    datum	key;
    datum	data;
    size_t	copy_len;
    int		result = 0;

    ICBDB_OPEN();

    icbdb_make_key (category, attribute, &key);
    data = dbm_fetch (db, key);

    if (data.dptr == NULL)
    {
	vmdb (MSG_DEBUG, "icbdb_get: %s.%s: NOT FOUND", category, attribute);
	result = 0;
    }
    else
    {
	vmdb (MSG_DEBUG, "icbdb_get: %s.%s: '%.*s'", category, attribute,
		data.dsize, data.dptr);
	copy_len = (size_t)data.dsize;
	if (copy_len >= sizeof(databuf))
	{
	    vmdb(MSG_WARN, "icbdb_get: truncating oversized value (%d bytes)",
		data.dsize);
	    copy_len = sizeof(databuf) - 1;
	}
	memcpy(databuf, data.dptr, copy_len);
	databuf[copy_len] = '\0';

	switch (type)
	{
	    case ICBDB_STRING:
		if (value != NULL) *((char **) value) = databuf;
		break;

	    case ICBDB_INT:
		if (value != NULL) *((int *) value) = atoi (databuf);
		break;
	}

	result = 1;
    }

    ICBDB_DONE(result);
}

int
icbdb_set (const char *category, const char *attribute, icbdb_type type,
	const void *value)
{
    datum	key;
    datum	data;
    int		result;

    ICBDB_OPEN();

    icbdb_make_key (category, attribute, &key);

    switch (type)
    {
	case ICBDB_STRING:
	    data.dptr = (char *) value;
	    break;

	case ICBDB_INT:
	    snprintf (databuf, sizeof (databuf), "%d", (int) value);
	    data.dptr = databuf;
	    break;
    }

    data.dsize = strlen (data.dptr);

    vmdb (MSG_DEBUG, "icbdb_set: '%.*s' --> '%.*s'", key.dsize, key.dptr,
	    data.dsize, data.dptr);
    result = dbm_store (db, key, data, DBM_REPLACE);

    ICBDB_DONE(result);
}

int
icbdb_delete (const char *category, const char *attribute)
{
    datum	key;
    int		result;

    ICBDB_OPEN();

    vmdb (MSG_DEBUG, "icbdb_delete (%s, %s)", category, attribute);
    icbdb_make_key (category, attribute, &key);
    result = dbm_delete (db, key);

    ICBDB_DONE(result);
}

//
// Handle lists (like message lists).  The basic structure is that there's
// a .max entry with the highest current value, and the actual values are
// .0 through .max.
//
int
icbdb_list_get_index (const char *category, const char *attribute, int index,
    icbdb_type type, void *value)
{
    char	tmp[DBLKSIZ];

    if (index == ICBDB_LIST_INDEX_MAX)
    {
	snprintf (tmp, sizeof (tmp), "%s.max", attribute);
    }
    else
    {
	snprintf (tmp, sizeof (tmp), "%s.%d", attribute, index);
    }

    vmdb (MSG_DEBUG, "icbdb_list_get_index: looking up '%s.%s'", category, tmp);
    return (icbdb_get (category, tmp, type, value));
}

int
icbdb_list_set_index (const char *category, const char *attribute, int index,
    icbdb_type type, void *value)
{
    char	tmp[DBLKSIZ];

    if (index == ICBDB_LIST_INDEX_MAX)
    {
	snprintf (tmp, sizeof (tmp), "%s.max", attribute);
    }
    else
    {
	snprintf (tmp, sizeof (tmp), "%s.%d", attribute, index);
    }

    return (icbdb_set (category, tmp, type, value));
}

int
icbdb_list_delete_index (const char *category, const char *attribute, int index)
{
    char	tmp[DBLKSIZ];

    if (index == ICBDB_LIST_INDEX_MAX)
    {
	snprintf (tmp, sizeof (tmp), "%s.max", attribute);
    }
    else
    {
	snprintf (tmp, sizeof (tmp), "%s.%d", attribute, index);
    }

    return (icbdb_delete (category, tmp));
}

int
icbdb_list_find (const char *category, const char *attribute, icbdb_type type, void *value,
    int *index, int *max_index, int *first_empty, int *last_valid)
{
    int		found = 0;

    ICBDB_OPEN();

    *index = -1;
    *max_index = -1;
    if (first_empty) *first_empty = -1;
    if (last_valid) *last_valid = -1;

    vmdb (MSG_DEBUG, "icbdb_list_find (%s, %s)", category, attribute);

    icbdb_list_get_index (category, attribute, ICBDB_LIST_INDEX_MAX, ICBDB_INT,
	max_index);

    vmdb (MSG_DEBUG, "icbdb_list_find: max_index = %d", *max_index);

    for (*index = 0; *index <= *max_index; (*index)++)
    {
	int	empty = 1;

	switch (type)
	{
	    case ICBDB_STRING:
		{
		    char	*compare;

		    empty = !icbdb_list_get_index (category, attribute, *index,
			type, &compare);

		    vmdb (MSG_DEBUG, "icbdb_list_find: comparing '%s' with '%s'",
			    compare, (char *) value);
		    if (!empty && strcmp (compare, (char *) value) == 0)
		    {
			found = 1;
		    }
		}
		break;

	    case ICBDB_INT:
		{
		    int		compare;

		    empty = !icbdb_list_get_index (category, attribute, *index,
			type, &compare);

		    vmdb (MSG_DEBUG, "icbdb_list_find: comparing %d with %d",
			    compare, (int) value);
		    if (!empty && compare == (int) value)
		    {
			found = 1;
		    }
		}
		break;
	    // TODO: Need error for unknown type.
	}

	if (empty)
	{
	    mdb (MSG_DEBUG, "icbdb_list_find: empty");

	    if (first_empty)
	    {
		if (*first_empty == -1)
		{
		    *first_empty = 0;
		}
	    }

	    continue;
	}

	if (found)
	{
	    mdb (MSG_DEBUG, "icbdb_list_find: match");
	    break;
	}

	if (last_valid) *last_valid = *index;
    }

    if (first_empty)
    {
	if (*first_empty == -1)
	{
	    *first_empty = *max_index + 1;
	}
    }

    vmdb (MSG_DEBUG, "icbdb_list_find: returning %d, index = %d, max_index = %d, first_empty = %d, last_valid = %d",
	    found, *index, *max_index, (first_empty) ? *first_empty : -1,
	    (last_valid) ? *last_valid : -1);

    ICBDB_DONE(found);
}

int
icbdb_list_add (const char *category, const char *attribute, icbdb_type type, void *value)
{
    int		index = -1;
    int		max_index = -1;
    int		first_empty = -1;

    ICBDB_OPEN();

    if (icbdb_list_find (category, attribute, type, value, &index, &max_index,
	&first_empty, NULL))
    {
	/* Already there. */
	ICBDB_DONE(1);
    }

    icbdb_list_set_index (category, attribute, first_empty, type, value);

    if (first_empty > max_index)
    {
	icbdb_list_set_index (category, attribute, ICBDB_LIST_INDEX_MAX,
	    ICBDB_INT, (void *) first_empty);
    }

    ICBDB_DONE(1);
}

int
icbdb_list_delete (const char *category, const char *attribute, icbdb_type type,
    void *value)
{
    int		index = -1;
    int		max_index = -1;
    int		last_valid = -1;

    ICBDB_OPEN();

    vmdb (MSG_DEBUG, "icbdb_list_delete (%s, %s)", category, attribute);

    if (!icbdb_list_find (category, attribute, type, value, &index, &max_index,
	NULL, &last_valid))
    {
	/* Already gone. */
	ICBDB_DONE(1);
    }

    icbdb_list_delete_index (category, attribute, index);

    if (index == max_index)
    {
	if (last_valid == -1)
	{
	    mdb (MSG_DEBUG, "icbdb_list_delete: deleting entire list");
	    icbdb_list_delete_index (category, attribute,
		ICBDB_LIST_INDEX_MAX);
	}
	else
	{
	    vmdb (MSG_DEBUG, "icbdb_list_delete: setting max to %d", last_valid);
	    icbdb_list_set_index (category, attribute, ICBDB_LIST_INDEX_MAX,
		ICBDB_INT, (void *) last_valid);
	}
    }

    ICBDB_DONE(1);
}

int
icbdb_list_load (const char *category, const char *attribute, NAMLIST *nl)
{
    int		index;
    int		max_index = -1;
    char	*value;

    ICBDB_OPEN();

    vmdb (MSG_DEBUG, "icbdb_list_load (%s, %s)", category, attribute);
    icbdb_list_get_index (category, attribute, ICBDB_LIST_INDEX_MAX, ICBDB_INT,
	&max_index);

    vmdb (MSG_DEBUG, "icbdb_list_load: max_index = %d", max_index);
    for (index = 0; index <= max_index; index++)
    {
	if (icbdb_list_get_index (category, attribute, index, ICBDB_STRING,
	    &value))
	{
	    vmdb (MSG_DEBUG, "icbdb_list_load: adding '%s'", value);
	    nlput (nl, value);
	}
    }

    ICBDB_DONE(1);
}

int
icbdb_list_save (const char *category, const char *attribute, NAMLIST *nl)
{
    int		index;

    ICBDB_OPEN();

    vmdb (MSG_DEBUG, "icbdb_list_save (%s, %s)", category, attribute);
    for (index = 0; index < nlcount (*nl); index++)
    {
	icbdb_list_add (category, attribute, ICBDB_STRING, nlget(nl));
    }

    ICBDB_DONE(1);
}

int
icbdb_list_clear (const char *category, const char *attribute)
{
    int		index;
    int		max_index = -1;

    ICBDB_OPEN();

    vmdb (MSG_DEBUG, "icbdb_list_clear (%s, %s)", category, attribute);
    icbdb_list_get_index (category, attribute, ICBDB_LIST_INDEX_MAX, ICBDB_INT,
	&max_index);
    icbdb_list_delete_index (category, attribute, ICBDB_LIST_INDEX_MAX);

    vmdb (MSG_DEBUG, "icbdb_list_clear: max_index = %d", max_index);
    for (index = 0; index <= max_index; index++)
    {
	icbdb_list_delete_index (category, attribute, index);
    }

    ICBDB_DONE(1);
}

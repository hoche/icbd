/*
 * dbm_gdbm.c
 *
 * Implements an ndbm-like API (dbm_open/dbm_fetch/...) on top of the native
 * gdbm API, for systems that have <gdbm.h> but not ndbm-compat headers.
 *
 * Important: gdbm_fetch() allocates memory for returned values; we copy them
 * into an internal buffer and free the gdbm allocation to avoid leaks and to
 * mimic the "static buffer" behavior expected by older DBM interfaces.
 */

#include "config.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>

#include "dbm.h"

#ifdef ICBD_HAVE_GDBM_API

struct DBM {
    GDBM_FILE f;
};

static int gdbm_flags_from_open_flags(int flags)
{
    /* Best-effort mapping. icbd uses O_RDWR. */
#ifdef O_RDONLY
    if ((flags & O_ACCMODE) == O_RDONLY)
        return GDBM_READER;
#endif
#ifdef O_WRONLY
    if ((flags & O_ACCMODE) == O_WRONLY)
        return GDBM_WRCREAT;
#endif
#ifdef O_RDWR
    if ((flags & O_ACCMODE) == O_RDWR)
        return GDBM_WRCREAT;
#endif

    return GDBM_WRCREAT;
}

DBM *dbm_open(const char *file, int flags, int mode)
{
    DBM *db = (DBM *)calloc(1, sizeof(DBM));
    if (!db) {
        errno = ENOMEM;
        return NULL;
    }

    int gflags = gdbm_flags_from_open_flags(flags);
    db->f = gdbm_open((char *)file, 0, gflags, mode, NULL);
    if (!db->f) {
        free(db);
        return NULL;
    }

    return db;
}

void dbm_close(DBM *db)
{
    if (!db) return;
    if (db->f) gdbm_close(db->f);
    free(db);
}

datum dbm_fetch(DBM *db, datum key)
{
    static char *buf = NULL;
    static size_t bufsz = 0;

    datum out = {0};
    if (!db || !db->f)
        return out;

    datum got = gdbm_fetch(db->f, key);
    if (!got.dptr)
        return out;

    if ((size_t)got.dsize + 1 > bufsz) {
        size_t nsz = (size_t)got.dsize + 1;
        char *nb = (char *)realloc(buf, nsz);
        if (!nb) {
            free(got.dptr);
            return out;
        }
        buf = nb;
        bufsz = nsz;
    }

    memcpy(buf, got.dptr, (size_t)got.dsize);
    buf[got.dsize] = '\0';
    free(got.dptr);

    out.dptr = buf;
    out.dsize = got.dsize;
    return out;
}

int dbm_store(DBM *db, datum key, datum content, int flags)
{
    if (!db || !db->f)
        return -1;

    int gflag = (flags == DBM_REPLACE) ? GDBM_REPLACE : GDBM_REPLACE;
    return gdbm_store(db->f, key, content, gflag);
}

int dbm_delete(DBM *db, datum key)
{
    if (!db || !db->f)
        return -1;
    return gdbm_delete(db->f, key);
}

#endif /* ICBD_HAVE_GDBM_API */


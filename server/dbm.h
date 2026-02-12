#pragma once

/*
 * DBM compatibility layer.
 *
 * icbd historically used ndbm / gdbm-compat. We also support systems that
 * only have native <gdbm.h> (no ndbm-compat headers) via a small wrapper.
 */

#include "config.h"

#if defined(__has_include)
#  if __has_include(<ndbm.h>)
#    include <ndbm.h>
#    define ICBD_HAVE_SYSTEM_DBM 1
#  elif __has_include(<gdbm/ndbm.h>)
#    include <gdbm/ndbm.h>
#    define ICBD_HAVE_SYSTEM_DBM 1
#  elif __has_include(<gdbm-ndbm.h>)
#    include <gdbm-ndbm.h>
#    define ICBD_HAVE_SYSTEM_DBM 1
#  elif __has_include(<gdbm.h>)
#    include <gdbm.h>
#    define ICBD_HAVE_GDBM_API 1
#  endif
#endif

#if defined(ICBD_HAVE_GDBM_API)
/*
 * Native gdbm API is available, but ndbm-compat headers are not.
 * Provide a tiny ndbm-like wrapper implemented in dbm_gdbm.c.
 */
typedef struct DBM DBM;

#ifndef DBM_REPLACE
#define DBM_REPLACE 1
#endif

DBM  *dbm_open(const char *file, int flags, int mode);
void  dbm_close(DBM *db);
datum dbm_fetch(DBM *db, datum key);
int   dbm_store(DBM *db, datum key, datum content, int flags);
int   dbm_delete(DBM *db, datum key);

#elif !defined(ICBD_HAVE_SYSTEM_DBM)
#error "No DBM backend found. Install ndbm/gdbm-compat headers (e.g. libgdbm-compat-dev) or native gdbm headers (libgdbm-dev)."
#endif /* dbm backend selection */


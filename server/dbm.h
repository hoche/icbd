#pragma once

/*
 * icb_dbm – lightweight, zero-dependency drop-in replacement for ndbm/gdbm.
 *
 * Provides the classic ndbm API (dbm_open / dbm_fetch / dbm_store /
 * dbm_delete / dbm_close) backed by an in-memory hash table that is
 * persisted to a single flat file ("<name>.db").
 *
 * File format:
 *   Header (8 bytes):
 *     magic[4]   = "IDB\x01"
 *     count[4]   = number of entries (little-endian uint32)
 *   Repeated <count> times:
 *     klen[4]    = key length   (little-endian uint32)
 *     vlen[4]    = value length (little-endian uint32)
 *     key[klen]  = key bytes
 *     val[vlen]  = value bytes
 *
 * Mutations are buffered in memory and flushed atomically on dbm_close()
 * (write-to-tmp + rename) so a crash never corrupts the file.
 */

#include <stddef.h>

/* ---- datum (matches the classic ndbm / gdbm definition) ---- */
typedef struct {
    char *dptr;
    int   dsize;
} datum;

/* ---- opaque DBM handle ---- */
typedef struct icb_dbm DBM;

/* ---- store flags ---- */
#define DBM_INSERT  0
#define DBM_REPLACE 1

/* ---- public API ---- */
DBM  *dbm_open(const char *file, int flags, int mode);
void  dbm_close(DBM *db);
datum dbm_fetch(DBM *db, datum key);
int   dbm_store(DBM *db, datum key, datum content, int flags);
int   dbm_delete(DBM *db, datum key);

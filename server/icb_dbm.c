/*
 * icb_dbm.c – lightweight drop-in replacement for ndbm / gdbm.
 *
 * Implements the classic ndbm API using an in-memory hash table backed by a
 * single flat file ("<name>.db").  No external library dependency.
 *
 * Design
 * ------
 * - dbm_open()  reads the file into an in-memory chained hash table.
 * - dbm_fetch() looks up in memory; returns a datum whose dptr points to an
 *               internal buffer valid until the next fetch.
 * - dbm_store() updates the in-memory table and marks it dirty.
 * - dbm_delete() removes from memory and marks dirty.
 * - dbm_close() flushes dirty state to disk (write-to-tmp + rename for
 *               crash safety), then frees all memory.
 *
 * File format (little-endian, ".db" suffix appended to the base name):
 *   Header  = magic[4] + entry_count[4]          (8 bytes)
 *   Entry_i = key_len[4] + val_len[4] + key + val
 */

#include "dbm.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

/* ================================================================
 * Constants
 * ================================================================ */
static const unsigned char ICB_DBM_MAGIC[4] = { 'I', 'D', 'B', 0x01 };
#define ICB_DBM_HDR_SIZE     8
#define ICB_DBM_INIT_BUCKETS 128
#define ICB_DBM_MAX_RECSIZE  (1u << 20)   /* 1 MiB sanity cap per key/value */

/* ================================================================
 * Internal types
 * ================================================================ */
typedef struct icb_dbm_entry {
    struct icb_dbm_entry *next;
    char  *key;
    int    klen;
    char  *val;
    int    vlen;
} entry_t;

struct icb_dbm {
    char        *path;          /* full path with ".db" suffix */
    int          mode;          /* file-creation permission bits */
    int          dirty;         /* 1 ⇒ in-memory state differs from disk */
    entry_t    **buckets;
    unsigned     nbuckets;
    unsigned     nentries;
    char        *fetch_buf;     /* returned by dbm_fetch; grows as needed */
    size_t       fetch_cap;
};

/* ================================================================
 * Little-endian uint32 helpers
 * ================================================================ */
static void put32(unsigned char *b, unsigned v)
{
    b[0] = (unsigned char)(v);
    b[1] = (unsigned char)(v >> 8);
    b[2] = (unsigned char)(v >> 16);
    b[3] = (unsigned char)(v >> 24);
}

static unsigned get32(const unsigned char *b)
{
    return (unsigned)b[0]
         | ((unsigned)b[1] << 8)
         | ((unsigned)b[2] << 16)
         | ((unsigned)b[3] << 24);
}

/* ================================================================
 * FNV-1a hash
 * ================================================================ */
static unsigned fnv1a(const char *data, int len)
{
    unsigned h = 2166136261u;
    for (int i = 0; i < len; i++) {
        h ^= (unsigned char)data[i];
        h *= 16777619u;
    }
    return h;
}

/* ================================================================
 * Entry helpers
 * ================================================================ */
static entry_t *entry_new(const char *k, int klen, const char *v, int vlen)
{
    entry_t *e = calloc(1, sizeof *e);
    if (!e) return NULL;
    e->key = malloc(klen > 0 ? (size_t)klen : 1);
    e->val = malloc(vlen > 0 ? (size_t)vlen : 1);
    if (!e->key || !e->val) {
        free(e->key);
        free(e->val);
        free(e);
        return NULL;
    }
    if (klen > 0) memcpy(e->key, k, (size_t)klen);
    e->klen = klen;
    if (vlen > 0) memcpy(e->val, v, (size_t)vlen);
    e->vlen = vlen;
    return e;
}

static void entry_free(entry_t *e)
{
    if (e) {
        free(e->key);
        free(e->val);
        free(e);
    }
}

/* ================================================================
 * Hash-table operations
 * ================================================================ */
static int ht_resize(struct icb_dbm *db, unsigned new_n)
{
    entry_t **nb = calloc(new_n, sizeof *nb);
    if (!nb) return -1;
    for (unsigned i = 0; i < db->nbuckets; i++) {
        entry_t *e = db->buckets[i];
        while (e) {
            entry_t *next = e->next;
            unsigned h = fnv1a(e->key, e->klen) % new_n;
            e->next = nb[h];
            nb[h] = e;
            e = next;
        }
    }
    free(db->buckets);
    db->buckets = nb;
    db->nbuckets = new_n;
    return 0;
}

/* Find an entry.  Optionally returns the predecessor (for unlinking)
 * and the bucket index. */
static entry_t *ht_find(struct icb_dbm *db,
                         const char *k, int klen,
                         entry_t **prev_out, unsigned *bucket_out)
{
    unsigned h = fnv1a(k, klen) % db->nbuckets;
    if (bucket_out) *bucket_out = h;
    entry_t *prev = NULL;
    for (entry_t *e = db->buckets[h]; e; prev = e, e = e->next) {
        if (e->klen == klen && memcmp(e->key, k, (size_t)klen) == 0) {
            if (prev_out) *prev_out = prev;
            return e;
        }
    }
    return NULL;
}

/* ================================================================
 * Full-read / full-write helpers (handle short reads/writes)
 * ================================================================ */
static int xread(int fd, void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t r = read(fd, (char *)buf + done, n - done);
        if (r <= 0) return -1;   /* EOF or error */
        done += (size_t)r;
    }
    return 0;
}

static int xwrite(int fd, const void *buf, size_t n)
{
    size_t done = 0;
    while (done < n) {
        ssize_t w = write(fd, (const char *)buf + done, n - done);
        if (w <= 0) return -1;
        done += (size_t)w;
    }
    return 0;
}

/* ================================================================
 * Persist / load
 * ================================================================ */
static int load_file(struct icb_dbm *db)
{
    int fd = open(db->path, O_RDONLY);
    if (fd < 0)
        return (errno == ENOENT) ? 0 : -1;   /* missing file → empty db */

    unsigned char hdr[ICB_DBM_HDR_SIZE];
    if (xread(fd, hdr, sizeof hdr) < 0)          { close(fd); return -1; }
    if (memcmp(hdr, ICB_DBM_MAGIC, 4) != 0)      { close(fd); errno = EINVAL; return -1; }

    unsigned count = get32(hdr + 4);

    for (unsigned i = 0; i < count; i++) {
        unsigned char rh[8];
        if (xread(fd, rh, 8) < 0) { close(fd); return -1; }

        unsigned klen = get32(rh);
        unsigned vlen = get32(rh + 4);
        if (klen > ICB_DBM_MAX_RECSIZE || vlen > ICB_DBM_MAX_RECSIZE) {
            close(fd); errno = EINVAL; return -1;
        }

        char *k = malloc(klen > 0 ? klen : 1);
        char *v = malloc(vlen > 0 ? vlen : 1);
        if (!k || !v)                                  { free(k); free(v); close(fd); return -1; }
        if (klen > 0 && xread(fd, k, klen) < 0)       { free(k); free(v); close(fd); return -1; }
        if (vlen > 0 && xread(fd, v, vlen) < 0)       { free(k); free(v); close(fd); return -1; }

        /* Grow the hash table before inserting if load factor > 0.75 */
        if (db->nentries * 4 >= db->nbuckets * 3) {
            if (ht_resize(db, db->nbuckets * 2) < 0) {
                free(k); free(v); close(fd); return -1;
            }
        }

        entry_t *e = entry_new(k, (int)klen, v, (int)vlen);
        free(k);
        free(v);
        if (!e) { close(fd); return -1; }

        unsigned bkt = fnv1a(e->key, e->klen) % db->nbuckets;
        e->next = db->buckets[bkt];
        db->buckets[bkt] = e;
        db->nentries++;
    }

    close(fd);
    return 0;
}

static int flush_file(struct icb_dbm *db)
{
    if (!db->dirty) return 0;

    /* Build tmp path:  <path>.tmp */
    size_t plen = strlen(db->path);
    char *tmp = malloc(plen + 5);          /* ".tmp\0" */
    if (!tmp) return -1;
    memcpy(tmp, db->path, plen);
    memcpy(tmp + plen, ".tmp", 5);

    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, db->mode);
    if (fd < 0) { free(tmp); return -1; }

    /* Header */
    unsigned char hdr[ICB_DBM_HDR_SIZE];
    memcpy(hdr, ICB_DBM_MAGIC, 4);
    put32(hdr + 4, db->nentries);
    if (xwrite(fd, hdr, sizeof hdr) < 0) goto fail;

    /* Entries */
    for (unsigned i = 0; i < db->nbuckets; i++) {
        for (entry_t *e = db->buckets[i]; e; e = e->next) {
            unsigned char rh[8];
            put32(rh, (unsigned)e->klen);
            put32(rh + 4, (unsigned)e->vlen);
            if (xwrite(fd, rh, 8) < 0)                                 goto fail;
            if (e->klen > 0 && xwrite(fd, e->key, (size_t)e->klen) < 0) goto fail;
            if (e->vlen > 0 && xwrite(fd, e->val, (size_t)e->vlen) < 0) goto fail;
        }
    }

    if (fsync(fd) < 0)   goto fail;
    close(fd);

    if (rename(tmp, db->path) < 0) { free(tmp); return -1; }
    free(tmp);
    db->dirty = 0;
    return 0;

fail:
    close(fd);
    unlink(tmp);
    free(tmp);
    return -1;
}

/* ================================================================
 * Public API
 * ================================================================ */

DBM *dbm_open(const char *file, int flags, int mode)
{
    (void)flags;   /* we always allow read + write + create-if-absent */

    struct icb_dbm *db = calloc(1, sizeof *db);
    if (!db) { errno = ENOMEM; return NULL; }

    /* Build path with ".db" suffix (standard ndbm convention) */
    size_t flen = strlen(file);
    db->path = malloc(flen + 4);                   /* ".db\0" */
    if (!db->path) { free(db); errno = ENOMEM; return NULL; }
    memcpy(db->path, file, flen);
    memcpy(db->path + flen, ".db", 4);             /* includes '\0' */

    db->mode = mode ? mode : 0600;
    db->nbuckets = ICB_DBM_INIT_BUCKETS;
    db->buckets = calloc(db->nbuckets, sizeof(entry_t *));
    if (!db->buckets) {
        free(db->path);
        free(db);
        errno = ENOMEM;
        return NULL;
    }

    if (load_file(db) < 0) {
        int save_errno = errno;
        /* Free everything we allocated */
        free(db->buckets);
        free(db->path);
        free(db);
        errno = save_errno;
        return NULL;
    }

    return db;
}

void dbm_close(DBM *db)
{
    if (!db) return;
    flush_file(db);   /* best-effort; ignores errors on close */
    for (unsigned i = 0; i < db->nbuckets; i++) {
        entry_t *e = db->buckets[i];
        while (e) {
            entry_t *next = e->next;
            entry_free(e);
            e = next;
        }
    }
    free(db->buckets);
    free(db->path);
    free(db->fetch_buf);
    free(db);
}

datum dbm_fetch(DBM *db, datum key)
{
    datum out = { NULL, 0 };
    if (!db || !key.dptr) return out;

    entry_t *e = ht_find(db, key.dptr, key.dsize, NULL, NULL);
    if (!e) return out;

    /* Copy to internal buffer so caller doesn't hold entry pointers. */
    size_t need = (size_t)e->vlen + 1;   /* +1 for convenience NUL */
    if (need > db->fetch_cap) {
        char *nb = realloc(db->fetch_buf, need);
        if (!nb) return out;
        db->fetch_buf = nb;
        db->fetch_cap = need;
    }
    if (e->vlen > 0)
        memcpy(db->fetch_buf, e->val, (size_t)e->vlen);
    db->fetch_buf[e->vlen] = '\0';

    out.dptr  = db->fetch_buf;
    out.dsize = e->vlen;
    return out;
}

int dbm_store(DBM *db, datum key, datum content, int flags)
{
    if (!db || !key.dptr) return -1;
    if (!content.dptr && content.dsize > 0) return -1;

    entry_t *e = ht_find(db, key.dptr, key.dsize, NULL, NULL);
    if (e) {
        if (flags == DBM_INSERT)
            return 1;          /* key already exists; insert refused */

        /* Replace value in place */
        char *nv = malloc(content.dsize > 0 ? (size_t)content.dsize : 1);
        if (!nv) return -1;
        if (content.dsize > 0)
            memcpy(nv, content.dptr, (size_t)content.dsize);
        free(e->val);
        e->val  = nv;
        e->vlen = content.dsize;
        db->dirty = 1;
        return 0;
    }

    /* New entry – grow the table if load factor > 0.75 */
    if (db->nentries * 4 >= db->nbuckets * 3) {
        if (ht_resize(db, db->nbuckets * 2) < 0)
            return -1;
    }

    e = entry_new(key.dptr, key.dsize,
                  content.dptr ? content.dptr : "", content.dsize);
    if (!e) return -1;

    unsigned bkt = fnv1a(key.dptr, key.dsize) % db->nbuckets;
    e->next = db->buckets[bkt];
    db->buckets[bkt] = e;
    db->nentries++;
    db->dirty = 1;
    return 0;
}

int dbm_delete(DBM *db, datum key)
{
    if (!db || !key.dptr) return -1;

    entry_t *prev = NULL;
    unsigned bkt = 0;
    entry_t *e = ht_find(db, key.dptr, key.dsize, &prev, &bkt);
    if (!e) return -1;         /* not found */

    if (prev)
        prev->next = e->next;
    else
        db->buckets[bkt] = e->next;

    entry_free(e);
    db->nentries--;
    db->dirty = 1;
    return 0;
}

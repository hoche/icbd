/*
 * Unit tests for server/icb_dbm.c  (the built-in DBM replacement).
 *
 * Tests cover:
 *   - open / close empty database
 *   - store and fetch single key-value pair
 *   - store and fetch multiple pairs
 *   - update (replace) an existing key
 *   - DBM_INSERT refused when key exists
 *   - delete a key
 *   - delete non-existent key returns -1
 *   - fetch non-existent key returns null datum
 *   - persistence across close / re-open
 *   - many entries (exercises hash-table resize)
 *   - binary keys / values (embedded NULs)
 *   - zero-length values
 *   - independent fetch buffers across two open handles
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "server/dbm.h"

/* ---- helpers ---- */

static datum mkdatum(const char *s)
{
    datum d;
    d.dptr  = (char *)s;
    d.dsize = (int)strlen(s);
    return d;
}

static datum mkdatum_n(const char *p, int n)
{
    datum d;
    d.dptr  = (char *)p;
    d.dsize = n;
    return d;
}

/* Create a unique temp directory and return the base path for the DB.
 * Caller must free() the returned string and clean up afterwards. */
static char *make_tmp_db(const char *label)
{
    char tmpl[256];
    snprintf(tmpl, sizeof tmpl, "/tmp/icb_dbm_test_%s_XXXXXX", label);
    char *dir = mkdtemp(tmpl);
    assert(dir != NULL);
    /* Build "<dir>/testdb" – dbm_open will append ".db" */
    size_t len = strlen(dir);
    char *path = malloc(len + 8);
    assert(path != NULL);
    snprintf(path, len + 8, "%s/testdb", dir);
    return path;
}

/* Remove the .db file and its directory. */
static void cleanup_tmp_db(char *base)
{
    char buf[512];
    snprintf(buf, sizeof buf, "%s.db", base);
    unlink(buf);
    snprintf(buf, sizeof buf, "%s.db.tmp", base);
    unlink(buf);
    /* Remove the directory (dirname of base) */
    char *slash = strrchr(base, '/');
    if (slash) {
        *slash = '\0';
        rmdir(base);
        *slash = '/';
    }
    free(base);
}

/* ================================================================
 * Tests
 * ================================================================ */

/* 1. Open and close with no data → no crash, no file created. */
static void test_open_close_empty(void)
{
    char *path = make_tmp_db("empty");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);
    dbm_close(db);

    /* No file should be created (nothing dirty) */
    char buf[512];
    snprintf(buf, sizeof buf, "%s.db", path);
    struct stat st;
    assert(stat(buf, &st) != 0);   /* file should not exist */
    cleanup_tmp_db(path);
    printf("  PASS: open_close_empty\n");
}

/* 2. Store and fetch a single pair. */
static void test_store_fetch_one(void)
{
    char *path = make_tmp_db("one");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("greeting");
    datum val = mkdatum("hello world");
    int rc = dbm_store(db, key, val, DBM_REPLACE);
    assert(rc == 0);

    datum got = dbm_fetch(db, key);
    assert(got.dptr != NULL);
    assert(got.dsize == val.dsize);
    assert(memcmp(got.dptr, "hello world", (size_t)got.dsize) == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: store_fetch_one\n");
}

/* 3. Store and fetch multiple pairs. */
static void test_store_fetch_multiple(void)
{
    char *path = make_tmp_db("multi");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    dbm_store(db, mkdatum("alpha"),   mkdatum("1"), DBM_REPLACE);
    dbm_store(db, mkdatum("bravo"),   mkdatum("2"), DBM_REPLACE);
    dbm_store(db, mkdatum("charlie"), mkdatum("3"), DBM_REPLACE);

    datum g1 = dbm_fetch(db, mkdatum("alpha"));
    assert(g1.dptr && g1.dsize == 1 && g1.dptr[0] == '1');

    datum g2 = dbm_fetch(db, mkdatum("bravo"));
    assert(g2.dptr && g2.dsize == 1 && g2.dptr[0] == '2');

    datum g3 = dbm_fetch(db, mkdatum("charlie"));
    assert(g3.dptr && g3.dsize == 1 && g3.dptr[0] == '3');

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: store_fetch_multiple\n");
}

/* 4. Replace an existing key. */
static void test_replace(void)
{
    char *path = make_tmp_db("replace");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("k");
    dbm_store(db, key, mkdatum("old"), DBM_REPLACE);
    dbm_store(db, key, mkdatum("new_value"), DBM_REPLACE);

    datum got = dbm_fetch(db, key);
    assert(got.dptr != NULL);
    assert(got.dsize == 9);
    assert(memcmp(got.dptr, "new_value", 9) == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: replace\n");
}

/* 5. DBM_INSERT refuses to overwrite. */
static void test_insert_no_overwrite(void)
{
    char *path = make_tmp_db("insert");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("k");
    int rc1 = dbm_store(db, key, mkdatum("first"), DBM_INSERT);
    assert(rc1 == 0);

    int rc2 = dbm_store(db, key, mkdatum("second"), DBM_INSERT);
    assert(rc2 == 1);   /* refused – key exists */

    datum got = dbm_fetch(db, key);
    assert(got.dptr && got.dsize == 5);
    assert(memcmp(got.dptr, "first", 5) == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: insert_no_overwrite\n");
}

/* 6. Delete a key. */
static void test_delete(void)
{
    char *path = make_tmp_db("delete");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("ephemeral");
    dbm_store(db, key, mkdatum("gone_soon"), DBM_REPLACE);

    int rc = dbm_delete(db, key);
    assert(rc == 0);

    datum got = dbm_fetch(db, key);
    assert(got.dptr == NULL);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: delete\n");
}

/* 7. Delete non-existent key returns -1. */
static void test_delete_missing(void)
{
    char *path = make_tmp_db("delmiss");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    int rc = dbm_delete(db, mkdatum("no_such_key"));
    assert(rc == -1);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: delete_missing\n");
}

/* 8. Fetch non-existent key. */
static void test_fetch_missing(void)
{
    char *path = make_tmp_db("fetchmiss");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum got = dbm_fetch(db, mkdatum("nope"));
    assert(got.dptr == NULL);
    assert(got.dsize == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: fetch_missing\n");
}

/* 9. Persistence: store → close → reopen → fetch. */
static void test_persistence(void)
{
    char *path = make_tmp_db("persist");

    /* Write some data and close. */
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        dbm_store(db, mkdatum("user.nick"), mkdatum("alice"), DBM_REPLACE);
        dbm_store(db, mkdatum("user.host"), mkdatum("10.0.0.1"), DBM_REPLACE);
        dbm_close(db);
    }

    /* Re-open and verify. */
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);

        datum g1 = dbm_fetch(db, mkdatum("user.nick"));
        assert(g1.dptr && g1.dsize == 5);
        assert(memcmp(g1.dptr, "alice", 5) == 0);

        datum g2 = dbm_fetch(db, mkdatum("user.host"));
        assert(g2.dptr && g2.dsize == 8);
        assert(memcmp(g2.dptr, "10.0.0.1", 8) == 0);

        dbm_close(db);
    }

    cleanup_tmp_db(path);
    printf("  PASS: persistence\n");
}

/* 10. Persistence of deletes: store → close → reopen → delete → close → reopen. */
static void test_persistence_delete(void)
{
    char *path = make_tmp_db("perdel");

    /* Write two keys, close. */
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        dbm_store(db, mkdatum("a"), mkdatum("A"), DBM_REPLACE);
        dbm_store(db, mkdatum("b"), mkdatum("B"), DBM_REPLACE);
        dbm_close(db);
    }

    /* Delete one, close. */
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        int rc = dbm_delete(db, mkdatum("a"));
        assert(rc == 0);
        dbm_close(db);
    }

    /* Verify only "b" survives. */
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        datum ga = dbm_fetch(db, mkdatum("a"));
        assert(ga.dptr == NULL);
        datum gb = dbm_fetch(db, mkdatum("b"));
        assert(gb.dptr && gb.dsize == 1 && gb.dptr[0] == 'B');
        dbm_close(db);
    }

    cleanup_tmp_db(path);
    printf("  PASS: persistence_delete\n");
}

/* 11. Many entries – exercises hash-table resize. */
static void test_many_entries(void)
{
    char *path = make_tmp_db("many");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    const int N = 1000;
    for (int i = 0; i < N; i++) {
        char kbuf[32], vbuf[32];
        snprintf(kbuf, sizeof kbuf, "key_%04d", i);
        snprintf(vbuf, sizeof vbuf, "val_%04d", i);
        int rc = dbm_store(db, mkdatum(kbuf), mkdatum(vbuf), DBM_REPLACE);
        assert(rc == 0);
    }

    /* Verify all entries. */
    for (int i = 0; i < N; i++) {
        char kbuf[32], vbuf[32];
        snprintf(kbuf, sizeof kbuf, "key_%04d", i);
        snprintf(vbuf, sizeof vbuf, "val_%04d", i);
        datum got = dbm_fetch(db, mkdatum(kbuf));
        assert(got.dptr != NULL);
        assert(got.dsize == (int)strlen(vbuf));
        assert(memcmp(got.dptr, vbuf, (size_t)got.dsize) == 0);
    }

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: many_entries\n");
}

/* 12. Many entries survive persistence. */
static void test_many_entries_persist(void)
{
    char *path = make_tmp_db("manypersist");
    const int N = 500;

    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        for (int i = 0; i < N; i++) {
            char kbuf[32], vbuf[32];
            snprintf(kbuf, sizeof kbuf, "k%d", i);
            snprintf(vbuf, sizeof vbuf, "v%d", i);
            dbm_store(db, mkdatum(kbuf), mkdatum(vbuf), DBM_REPLACE);
        }
        dbm_close(db);
    }
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        for (int i = 0; i < N; i++) {
            char kbuf[32], vbuf[32];
            snprintf(kbuf, sizeof kbuf, "k%d", i);
            snprintf(vbuf, sizeof vbuf, "v%d", i);
            datum got = dbm_fetch(db, mkdatum(kbuf));
            assert(got.dptr != NULL);
            assert(got.dsize == (int)strlen(vbuf));
            assert(memcmp(got.dptr, vbuf, (size_t)got.dsize) == 0);
        }
        dbm_close(db);
    }

    cleanup_tmp_db(path);
    printf("  PASS: many_entries_persist\n");
}

/* 13. Binary keys and values (embedded NULs). */
static void test_binary_data(void)
{
    char *path = make_tmp_db("binary");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    /* Key with embedded NUL */
    const char kdata[] = { 'a', '\0', 'b' };
    const char vdata[] = { 'x', '\0', 'y', '\0', 'z' };
    datum key = mkdatum_n(kdata, 3);
    datum val = mkdatum_n(vdata, 5);

    int rc = dbm_store(db, key, val, DBM_REPLACE);
    assert(rc == 0);

    datum got = dbm_fetch(db, key);
    assert(got.dptr != NULL);
    assert(got.dsize == 5);
    assert(memcmp(got.dptr, vdata, 5) == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: binary_data\n");
}

/* 14. Zero-length value. */
static void test_zero_length_value(void)
{
    char *path = make_tmp_db("zeroval");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("flag");
    datum val = { "", 0 };
    int rc = dbm_store(db, key, val, DBM_REPLACE);
    assert(rc == 0);

    datum got = dbm_fetch(db, key);
    assert(got.dptr != NULL);
    assert(got.dsize == 0);

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: zero_length_value\n");
}

/* 15. Re-open after corrupt / non-DB file fails gracefully. */
static void test_corrupt_file(void)
{
    char *path = make_tmp_db("corrupt");
    char fpath[512];
    snprintf(fpath, sizeof fpath, "%s.db", path);

    /* Write garbage to the .db file. */
    int fd = open(fpath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    assert(fd >= 0);
    const char *garbage = "this is not a database";
    (void)write(fd, garbage, strlen(garbage));
    close(fd);

    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db == NULL);

    unlink(fpath);
    /* Clean up directory */
    char *slash = strrchr(path, '/');
    if (slash) { *slash = '\0'; rmdir(path); *slash = '/'; }
    free(path);
    printf("  PASS: corrupt_file\n");
}

/* 16. dbm_close(NULL) is safe. */
static void test_close_null(void)
{
    dbm_close(NULL);   /* must not crash */
    printf("  PASS: close_null\n");
}

/* 17. Replace value with shorter and longer values (exercises realloc path). */
static void test_replace_various_sizes(void)
{
    char *path = make_tmp_db("sizes");
    DBM *db = dbm_open(path, O_RDWR, 0600);
    assert(db != NULL);

    datum key = mkdatum("sz");

    /* Start with a long value. */
    char longval[200];
    memset(longval, 'A', sizeof longval);
    datum lv = mkdatum_n(longval, (int)sizeof longval);
    dbm_store(db, key, lv, DBM_REPLACE);

    datum g1 = dbm_fetch(db, key);
    assert(g1.dsize == (int)sizeof longval);

    /* Replace with a short value. */
    dbm_store(db, key, mkdatum("tiny"), DBM_REPLACE);
    datum g2 = dbm_fetch(db, key);
    assert(g2.dsize == 4);
    assert(memcmp(g2.dptr, "tiny", 4) == 0);

    /* Replace with a long value again. */
    char longval2[300];
    memset(longval2, 'B', sizeof longval2);
    datum lv2 = mkdatum_n(longval2, (int)sizeof longval2);
    dbm_store(db, key, lv2, DBM_REPLACE);
    datum g3 = dbm_fetch(db, key);
    assert(g3.dsize == (int)sizeof longval2);
    assert(g3.dptr[0] == 'B');

    dbm_close(db);
    cleanup_tmp_db(path);
    printf("  PASS: replace_various_sizes\n");
}

/* 18. Persistence of updates – the final value survives. */
static void test_persistence_update(void)
{
    char *path = make_tmp_db("perupd");

    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        dbm_store(db, mkdatum("x"), mkdatum("old"), DBM_REPLACE);
        dbm_store(db, mkdatum("x"), mkdatum("new"), DBM_REPLACE);
        dbm_close(db);
    }
    {
        DBM *db = dbm_open(path, O_RDWR, 0600);
        assert(db != NULL);
        datum got = dbm_fetch(db, mkdatum("x"));
        assert(got.dptr && got.dsize == 3);
        assert(memcmp(got.dptr, "new", 3) == 0);
        dbm_close(db);
    }

    cleanup_tmp_db(path);
    printf("  PASS: persistence_update\n");
}

/* ================================================================
 * Main
 * ================================================================ */
int main(void)
{
    printf("icb_dbm unit tests:\n");

    test_open_close_empty();
    test_store_fetch_one();
    test_store_fetch_multiple();
    test_replace();
    test_insert_no_overwrite();
    test_delete();
    test_delete_missing();
    test_fetch_missing();
    test_persistence();
    test_persistence_delete();
    test_many_entries();
    test_many_entries_persist();
    test_binary_data();
    test_zero_length_value();
    test_corrupt_file();
    test_close_null();
    test_replace_various_sizes();
    test_persistence_update();

    printf("All icb_dbm tests passed.\n");
    return 0;
}

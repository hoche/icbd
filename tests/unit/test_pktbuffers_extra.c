/*
 * Additional unit tests for pktserv/pktbuffers.c
 *
 * Tests: _msgbuf_free, _msgbuf_alloc edge cases, cbufs_init, cbufs_reset
 */

#include <assert.h>
#include <string.h>
#include <stdarg.h>

#include "server/mdb.h"
#include "pktserv/pktbuffers.h"

/* Stubs for mdb */
int icbd_log = -1;
int log_level = 0;
void mdb(int level, const char *message) { (void)level; (void)message; }
void vmdb(int level, const char *fmt, ...) { (void)level; (void)fmt; }
int sslmdb(const char *str, size_t len, void *u) { (void)str; (void)len; (void)u; return 0; }

/* ------------------------------------------------------------------ */
/* _msgbuf_free                                                        */
/* ------------------------------------------------------------------ */

static void test_msgbuf_free_basic(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 32);
    assert(b != NULL);
    assert(b->data != NULL);

    /* Write some data to verify it gets zeroed on free */
    memset(b->data, 'A', 32);
    b->len = 32;

    _msgbuf_free(b);
    /* If we got here without crashing, it worked */
}

static void test_msgbuf_alloc_null_returns_fresh(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 64);
    assert(b != NULL);
    assert(b->data != NULL);
    assert(b->sz == 64);
    assert(b->len == 0);
    assert(b->pos == b->data);
    _msgbuf_free(b);
}

static void test_msgbuf_alloc_zero_size(void) {
    /* Allocate with size 0 — implementation should still allocate the struct */
    msgbuf_t *b = _msgbuf_alloc(NULL, 0);
    /* malloc(0) is implementation-defined; may return NULL or a unique pointer.
     * Either way, the struct should be allocated. */
    if (b != NULL) {
        _msgbuf_free(b);
    }
}

static void test_msgbuf_realloc_smaller_reuses(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 128);
    assert(b != NULL);
    assert(b->sz == 128);

    /* Re-alloc with smaller size should reuse the existing buffer */
    msgbuf_t *b2 = _msgbuf_alloc(b, 64);
    assert(b2 == b);
    assert(b2->sz == 128); /* unchanged — buffer is big enough */
    assert(b2->len == 0);  /* reset */
    _msgbuf_free(b2);
}

static void test_msgbuf_realloc_same_size_reuses(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 64);
    assert(b != NULL);

    memset(b->data, 'X', 64);
    b->len = 50;
    b->pos = b->data + 10;

    msgbuf_t *b2 = _msgbuf_alloc(b, 64);
    assert(b2 == b);
    assert(b2->len == 0);
    assert(b2->pos == b2->data);
    /* Data should be zeroed */
    for (size_t i = 0; i < 64; i++) {
        assert(b2->data[i] == '\0');
    }
    _msgbuf_free(b2);
}

static void test_msgbuf_realloc_larger_grows(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 16);
    assert(b != NULL);
    assert(b->sz == 16);

    msgbuf_t *b2 = _msgbuf_alloc(b, 256);
    assert(b2 == b);
    assert(b2->sz == 256);
    assert(b2->len == 0);
    assert(b2->pos == b2->data);
    _msgbuf_free(b2);
}

/* ------------------------------------------------------------------ */
/* cbufs_init / cbufs_reset                                            */
/* ------------------------------------------------------------------ */

static void test_cbufs_init(void) {
    int ret = cbufs_init();
    assert(ret == 0);

    /* Calling again should still succeed (idempotent) */
    ret = cbufs_init();
    assert(ret == 0);
}

static void test_cbufs_reset_after_init(void) {
    int ret = cbufs_init();
    assert(ret == 0);

    /* Reset should not crash */
    cbufs_reset();
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(void) {
    /* _msgbuf_free */
    test_msgbuf_free_basic();

    /* _msgbuf_alloc edge cases */
    test_msgbuf_alloc_null_returns_fresh();
    test_msgbuf_alloc_zero_size();
    test_msgbuf_realloc_smaller_reuses();
    test_msgbuf_realloc_same_size_reuses();
    test_msgbuf_realloc_larger_grows();

    /* cbufs_init / cbufs_reset */
    test_cbufs_init();
    test_cbufs_reset_after_init();

    return 0;
}

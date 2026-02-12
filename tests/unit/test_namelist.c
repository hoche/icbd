/*
 * Unit tests for server/namelist.c
 *
 * Tests: nlinit, nlput, nlget, nlcount, nlclear, nldelete, nlpresent, nlmatch
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "server/mdb.h"
#include "server/namelist.h"

/*
 * namelist.c logs through server/mdb.c. Provide stubs.
 */
int icbd_log = -1;
int log_level = 0;

void mdb(int level, const char *message) {
    (void)level;
    (void)message;
}

void vmdb(int level, const char *fmt, ...) {
    (void)level;
    (void)fmt;
}

/* ------------------------------------------------------------------ */
/* nlinit / basic state                                                */
/* ------------------------------------------------------------------ */
static void test_nlinit(void) {
    NAMLIST nl;
    nlinit(&nl, 5);
    assert(nl.num == 0);
    assert(nl.max == 5);
    assert(nl.head == NULL);
    assert(nl.tail == NULL);
    assert(nl.p == NULL);
}

/* ------------------------------------------------------------------ */
/* nlput / nlcount / nlget                                             */
/* ------------------------------------------------------------------ */
static void test_nlput_and_count(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    assert(nlcount(nl) == 1);

    nlput(&nl, "bob");
    assert(nlcount(nl) == 2);

    nlput(&nl, "carol");
    assert(nlcount(nl) == 3);

    nlclear(&nl);
}

static void test_nlput_duplicate_moves_to_head(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    nlput(&nl, "bob");
    nlput(&nl, "carol");

    /* head should be carol (last added) */
    char *first = nlget(&nl);
    assert(strcmp(first, "carol") == 0);

    /* Now add alice again - should move to head */
    nlput(&nl, "alice");
    first = nlget(&nl);
    assert(strcmp(first, "alice") == 0);

    /* Count shouldn't change */
    assert(nlcount(nl) == 3);

    nlclear(&nl);
}

static void test_nlput_overflow_replaces_tail(void) {
    NAMLIST nl;
    nlinit(&nl, 2);

    nlput(&nl, "alice");
    nlput(&nl, "bob");
    assert(nlcount(nl) == 2);

    /* Adding a third should replace the tail (alice) */
    nlput(&nl, "carol");
    assert(nlcount(nl) == 2);

    /* Verify alice is gone */
    assert(nlpresent("alice", nl) == 0);
    assert(nlpresent("bob", nl) == 1);
    assert(nlpresent("carol", nl) == 1);

    nlclear(&nl);
}

static void test_nlget_cycles(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    nlput(&nl, "bob");

    /* nlget should cycle: bob, alice, bob, alice, ... */
    char *a = nlget(&nl);
    char *b = nlget(&nl);
    char *c = nlget(&nl);

    assert(strcmp(a, "bob") == 0);
    assert(strcmp(b, "alice") == 0);
    assert(strcmp(c, "bob") == 0);

    nlclear(&nl);
}

/* ------------------------------------------------------------------ */
/* nlclear                                                             */
/* ------------------------------------------------------------------ */
static void test_nlclear(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    nlput(&nl, "bob");
    nlput(&nl, "carol");
    assert(nlcount(nl) == 3);

    nlclear(&nl);
    assert(nlcount(nl) == 0);
    assert(nl.head == NULL);
    assert(nl.tail == NULL);
    assert(nl.p == NULL);
}

static void test_nlclear_after_nlget(void) {
    /* This tests the fix for bug 2: nlclear should free all nodes
     * even if nlget has advanced the cursor. */
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    nlput(&nl, "bob");
    nlput(&nl, "carol");

    /* Advance the cursor past head */
    nlget(&nl);
    nlget(&nl);

    /* nlclear should still free everything (starts from head, not p) */
    nlclear(&nl);
    assert(nlcount(nl) == 0);
    assert(nl.head == NULL);
}

static void test_nlclear_null(void) {
    /* Should not crash */
    nlclear(NULL);
}

/* ------------------------------------------------------------------ */
/* nldelete                                                            */
/* ------------------------------------------------------------------ */
static void test_nldelete_existing(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    nlput(&nl, "bob");
    nlput(&nl, "carol");

    int ret = nldelete(&nl, "bob");
    assert(ret == 0);
    assert(nlcount(nl) == 2);
    assert(nlpresent("bob", nl) == 0);
    assert(nlpresent("alice", nl) == 1);
    assert(nlpresent("carol", nl) == 1);

    nlclear(&nl);
}

static void test_nldelete_nonexistent(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");

    int ret = nldelete(&nl, "bob");
    assert(ret == -1);
    assert(nlcount(nl) == 1);

    nlclear(&nl);
}

static void test_nldelete_last_item(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "alice");
    int ret = nldelete(&nl, "alice");
    assert(ret == 0);
    assert(nlcount(nl) == 0);

    nlclear(&nl);
}

static void test_nldelete_null(void) {
    assert(nldelete(NULL, "x") == -1);
}

/* ------------------------------------------------------------------ */
/* nlpresent                                                           */
/* ------------------------------------------------------------------ */
static void test_nlpresent_case_insensitive(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "Alice");

    /* nlpresent uses case-insensitive comparison */
    assert(nlpresent("alice", nl) == 1);
    assert(nlpresent("ALICE", nl) == 1);
    assert(nlpresent("bob", nl) == 0);

    nlclear(&nl);
}

static void test_nlpresent_empty(void) {
    NAMLIST nl;
    nlinit(&nl, 10);
    assert(nlpresent("anything", nl) == 0);
}

/* ------------------------------------------------------------------ */
/* nlmatch                                                             */
/* ------------------------------------------------------------------ */
static void test_nlmatch_wildcard(void) {
    NAMLIST nl;
    nlinit(&nl, 10);

    nlput(&nl, "*.example.com");

    assert(nlmatch("user@host.example.com", nl) == 1);
    assert(nlmatch("user@other.net", nl) == 0);

    nlclear(&nl);
}

static void test_nlmatch_empty(void) {
    NAMLIST nl;
    nlinit(&nl, 10);
    assert(nlmatch("anything", nl) == 0);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    test_nlinit();

    test_nlput_and_count();
    test_nlput_duplicate_moves_to_head();
    test_nlput_overflow_replaces_tail();
    test_nlget_cycles();

    test_nlclear();
    test_nlclear_after_nlget();
    test_nlclear_null();

    test_nldelete_existing();
    test_nldelete_nonexistent();
    test_nldelete_last_item();
    test_nldelete_null();

    test_nlpresent_case_insensitive();
    test_nlpresent_empty();

    test_nlmatch_wildcard();
    test_nlmatch_empty();

    return 0;
}

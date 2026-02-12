/*
 * Unit tests for server/strlist.c
 *
 * Tests: strmakenode, strlinkhead, strlinktail, strlinkafter,
 *        strlinkbefore, strunlink, strlinkalpha, strgetnode
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "server/strlist.h"

/* helper: make a node with a string */
static STRLIST *mknode(const char *s) {
    STRLIST *n = strmakenode((int)strlen(s));
    assert(n != NULL);
    strcpy(n->str, s);
    return n;
}

/* helper: free a whole list */
static void freelist(STRLIST *head) {
    while (head) {
        STRLIST *tmp = head->next;
        free(head);
        head = tmp;
    }
}

/* helper: count nodes in list from head */
static int listlen(STRLIST *head) {
    int n = 0;
    for (STRLIST *p = head; p; p = p->next)
        n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* strmakenode                                                         */
/* ------------------------------------------------------------------ */
static void test_strmakenode(void) {
    STRLIST *n = strmakenode(10);
    assert(n != NULL);
    assert(n->next == NULL);
    assert(n->prev == NULL);
    assert(n->str[0] == '\0');
    free(n);
}

/* ------------------------------------------------------------------ */
/* strlinkhead                                                         */
/* ------------------------------------------------------------------ */
static void test_strlinkhead_empty_list(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");

    strlinkhead(a, &head, &tail);

    assert(head == a);
    assert(tail == a);
    assert(a->prev == NULL);
    assert(a->next == NULL);

    freelist(head);
}

static void test_strlinkhead_prepend(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinkhead(b, &head, &tail);

    assert(head == b);
    assert(tail == a);
    assert(b->next == a);
    assert(a->prev == b);
    assert(listlen(head) == 2);

    freelist(head);
}

/* ------------------------------------------------------------------ */
/* strlinktail                                                         */
/* ------------------------------------------------------------------ */
static void test_strlinktail_empty_list(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");

    strlinktail(a, &head, &tail);

    assert(head == a);
    assert(tail == a);

    freelist(head);
}

static void test_strlinktail_append(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");

    strlinktail(a, &head, &tail);
    strlinktail(b, &head, &tail);

    assert(head == a);
    assert(tail == b);
    assert(a->next == b);
    assert(b->prev == a);

    freelist(head);
}

/* ------------------------------------------------------------------ */
/* strlinkafter                                                        */
/* ------------------------------------------------------------------ */
static void test_strlinkafter_at_tail(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinkafter(b, a, &head, &tail);

    assert(head == a);
    assert(tail == b);
    assert(a->next == b);
    assert(b->prev == a);

    freelist(head);
}

static void test_strlinkafter_middle(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *c = mknode("c");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinktail(c, &head, &tail);
    strlinkafter(b, a, &head, &tail);

    assert(head == a);
    assert(tail == c);
    assert(a->next == b);
    assert(b->next == c);
    assert(c->prev == b);
    assert(listlen(head) == 3);

    freelist(head);
}

/* ------------------------------------------------------------------ */
/* strlinkbefore                                                       */
/* ------------------------------------------------------------------ */
static void test_strlinkbefore_at_head(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *b = mknode("b");
    STRLIST *a = mknode("a");

    strlinkhead(b, &head, &tail);
    strlinkbefore(a, b, &head, &tail);

    assert(head == a);
    assert(tail == b);
    assert(a->next == b);
    assert(b->prev == a);
    assert(a->prev == NULL);

    freelist(head);
}

static void test_strlinkbefore_middle(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *c = mknode("c");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinktail(c, &head, &tail);
    strlinkbefore(b, c, &head, &tail);

    assert(head == a);
    assert(tail == c);
    assert(a->next == b);
    assert(b->next == c);
    assert(c->prev == b);
    assert(b->prev == a);
    assert(listlen(head) == 3);

    freelist(head);
}

/* ------------------------------------------------------------------ */
/* strunlink                                                           */
/* ------------------------------------------------------------------ */
static void test_strunlink_head(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinktail(b, &head, &tail);

    strunlink(a, &head, &tail);
    assert(head == b);
    assert(tail == b);
    assert(b->prev == NULL);

    free(a);
    freelist(head);
}

static void test_strunlink_tail(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");

    strlinkhead(a, &head, &tail);
    strlinktail(b, &head, &tail);

    strunlink(b, &head, &tail);
    assert(head == a);
    assert(tail == a);
    assert(a->next == NULL);

    free(b);
    freelist(head);
}

static void test_strunlink_middle(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");
    STRLIST *b = mknode("b");
    STRLIST *c = mknode("c");

    strlinkhead(a, &head, &tail);
    strlinktail(b, &head, &tail);
    strlinktail(c, &head, &tail);

    strunlink(b, &head, &tail);
    assert(head == a);
    assert(tail == c);
    assert(a->next == c);
    assert(c->prev == a);
    assert(listlen(head) == 2);

    free(b);
    freelist(head);
}

static void test_strunlink_only(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("a");

    strlinkhead(a, &head, &tail);
    strunlink(a, &head, &tail);
    assert(head == NULL);
    assert(tail == NULL);

    free(a);
}

/* ------------------------------------------------------------------ */
/* strlinkalpha                                                        */
/* ------------------------------------------------------------------ */
static void test_strlinkalpha_sorted(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *b = mknode("banana");
    STRLIST *a = mknode("apple");
    STRLIST *c = mknode("cherry");

    strlinkalpha(b, &head, &tail, 0);
    strlinkalpha(a, &head, &tail, 0);
    strlinkalpha(c, &head, &tail, 0);

    assert(head == a);
    assert(a->next == b);
    assert(b->next == c);
    assert(tail == c);
    assert(listlen(head) == 3);

    freelist(head);
}

static void test_strlinkalpha_case_independent(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *b = mknode("Banana");
    STRLIST *a = mknode("apple");
    STRLIST *c = mknode("CHERRY");

    strlinkalpha(b, &head, &tail, 1);
    strlinkalpha(a, &head, &tail, 1);
    strlinkalpha(c, &head, &tail, 1);

    assert(head == a);
    assert(a->next == b);
    assert(b->next == c);
    assert(tail == c);

    freelist(head);
}

/* ------------------------------------------------------------------ */
/* strgetnode                                                          */
/* ------------------------------------------------------------------ */
static void test_strgetnode_exact(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("alice");
    STRLIST *b = mknode("bob");

    strlinkhead(a, &head, &tail);
    strlinktail(b, &head, &tail);

    assert(strgetnode("alice", head, 0, 0) == a);
    assert(strgetnode("bob", head, 0, 0) == b);
    assert(strgetnode("carol", head, 0, 0) == NULL);

    freelist(head);
}

static void test_strgetnode_case_insensitive(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("Alice");

    strlinkhead(a, &head, &tail);

    assert(strgetnode("alice", head, 1, 0) == a);
    assert(strgetnode("ALICE", head, 1, 0) == a);
    /* case-sensitive should NOT match */
    assert(strgetnode("alice", head, 0, 0) == NULL);

    freelist(head);
}

static void test_strgetnode_pattern(void) {
    STRLIST *head = NULL, *tail = NULL;
    STRLIST *a = mknode("*.example.com");

    strlinkhead(a, &head, &tail);

    /* pattern mode uses wildmat */
    assert(strgetnode("user@host.example.com", head, 1, 1) == a);
    assert(strgetnode("user@other.net", head, 1, 1) == NULL);

    freelist(head);
}

static void test_strgetnode_empty_list(void) {
    assert(strgetnode("anything", NULL, 0, 0) == NULL);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    test_strmakenode();

    test_strlinkhead_empty_list();
    test_strlinkhead_prepend();

    test_strlinktail_empty_list();
    test_strlinktail_append();

    test_strlinkafter_at_tail();
    test_strlinkafter_middle();

    test_strlinkbefore_at_head();
    test_strlinkbefore_middle();

    test_strunlink_head();
    test_strunlink_tail();
    test_strunlink_middle();
    test_strunlink_only();

    test_strlinkalpha_sorted();
    test_strlinkalpha_case_independent();

    test_strgetnode_exact();
    test_strgetnode_case_insensitive();
    test_strgetnode_pattern();
    test_strgetnode_empty_list();

    return 0;
}

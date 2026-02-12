/*
 * Unit tests for server/strutil.c
 *
 * Tests: filternickname, filtertext, filtergroupname, filterfmt,
 *        lcaseit, ucaseit, getword, get_tail, split
 */

#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "server/mdb.h"
#include "server/strutil.h"

/* strutil.c uses the fields[] global from externs, we must provide it. */
extern char *fields[];

/*
 * utf8.c and strutil.c log through server/mdb.c. Provide stubs.
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
/* filternickname                                                      */
/* ------------------------------------------------------------------ */
static void test_filternickname_passthrough(void) {
    char buf[] = "Alice01";
    filternickname(buf);
    assert(strcmp(buf, "Alice01") == 0);
}

static void test_filternickname_space_becomes_underscore(void) {
    char buf[] = "hi there";
    filternickname(buf);
    assert(strcmp(buf, "hi_there") == 0);
}

static void test_filternickname_illegal_becomes_question(void) {
    char buf[] = "bad\x01guy";
    filternickname(buf);
    assert(strcmp(buf, "bad?guy") == 0);
}

static void test_filternickname_special_chars(void) {
    /* These are all in OKNICKCHARS: -.!'$+,/?_@ */
    char buf[] = "-.'@_";
    filternickname(buf);
    assert(strcmp(buf, "-.'@_") == 0);
}

/* ------------------------------------------------------------------ */
/* filtertext                                                          */
/* ------------------------------------------------------------------ */
static void test_filtertext_normal_ascii(void) {
    char dest[32];
    filtertext("Hello World!", dest, sizeof(dest));
    assert(strcmp(dest, "Hello World!") == 0);
}

static void test_filtertext_replaces_control_chars(void) {
    char dest[32];
    filtertext("hi\x01there", dest, sizeof(dest));
#if ALLOW_UTF8_MESSAGES
    /* In UTF-8 mode, control chars (< 0x20) are silently dropped */
    assert(strcmp(dest, "hithere") == 0);
#else
    /* In non-UTF8 mode, control chars are replaced with '?' */
    assert(dest[2] == '?');
    assert(dest[3] == 't');
#endif
}

static void test_filtertext_truncates_to_len(void) {
    /* Use a large buffer so outer memset guarantees a null after len bytes */
    char dest[32];
    memset(dest, 0, sizeof(dest));
    filtertext("Hello World!", dest, 6);
#if ALLOW_UTF8_MESSAGES
    /* UTF-8 path: processes up to len (6) source bytes -> "Hello " */
    assert(strncmp(dest, "Hello ", 6) == 0);
    /* Null termination from outer memset at pos 6 */
    assert(dest[6] == '\0');
#else
    /* Non-UTF-8 path: copies len-1 (5) chars + null terminator */
    assert(strlen(dest) == 5);
    assert(strcmp(dest, "Hello") == 0);
#endif
}

static void test_filtertext_empty(void) {
    char dest[8];
    memset(dest, 'X', sizeof(dest));
    filtertext("", dest, sizeof(dest));
    assert(dest[0] == '\0');
}

#if ALLOW_UTF8_MESSAGES
static void test_filtertext_utf8_2byte(void) {
    /* é = U+00E9 = 0xC3 0xA9 (2 bytes) */
    char dest[32];
    filtertext("caf\xc3\xa9", dest, sizeof(dest));
    /* Should pass through the UTF-8 bytes */
    assert(dest[0] == 'c');
    assert(dest[1] == 'a');
    assert(dest[2] == 'f');
    assert((unsigned char)dest[3] == 0xC3);
    assert((unsigned char)dest[4] == 0xA9);
}

static void test_filtertext_utf8_invalid_byte(void) {
    char dest[32];
    /* 0xFF is never valid in UTF-8 */
    filtertext("a\xff" "b", dest, sizeof(dest));
    assert(dest[0] == 'a');
    assert(dest[1] == '?');
    assert(dest[2] == 'b');
}
#endif

/* ------------------------------------------------------------------ */
/* filtergroupname                                                     */
/* ------------------------------------------------------------------ */
static void test_filtergroupname_passthrough(void) {
    char buf[] = "Test01";
    filtergroupname(buf);
    assert(strcmp(buf, "Test01") == 0);
}

static void test_filtergroupname_special_chars(void) {
    /* OKGROUPCHARS: -.!'$+,/?_~ */
    char buf[] = "~grp!";
    filtergroupname(buf);
    assert(strcmp(buf, "~grp!") == 0);
}

static void test_filtergroupname_illegal(void) {
    char buf[] = "bad\x01g";
    filtergroupname(buf);
    assert(strcmp(buf, "bad?g") == 0);
}

static void test_filtergroupname_space(void) {
    char buf[] = "a b";
    filtergroupname(buf);
    assert(strcmp(buf, "a_b") == 0);
}

/* ------------------------------------------------------------------ */
/* filterfmt                                                           */
/* ------------------------------------------------------------------ */
static void test_filterfmt_one_s(void) {
    char str[] = "User %s was booted";
    int n = 1;
    char *err = filterfmt(str, &n);
    assert(err == NULL);
}

static void test_filterfmt_wrong_count(void) {
    char str[] = "No subs here";
    int n = 1;
    char *err = filterfmt(str, &n);
    assert(err != NULL);
    assert(n == 0); /* actual count is 0 */
}

static void test_filterfmt_bad_specifier(void) {
    char str[] = "This has %d in it";
    int n = 1;
    char *err = filterfmt(str, &n);
    assert(err != NULL);
    assert(n == -1);
}

static void test_filterfmt_double_percent(void) {
    /* %% should be ok and not count as a format specifier */
    char str[] = "100%% done %s";
    int n = 1;
    char *err = filterfmt(str, &n);
    assert(err == NULL);
}

/* ------------------------------------------------------------------ */
/* lcaseit / ucaseit                                                   */
/* ------------------------------------------------------------------ */
static void test_lcaseit(void) {
    char buf[] = "Hello WORLD 123";
    lcaseit(buf);
    assert(strcmp(buf, "hello world 123") == 0);
}

static void test_ucaseit(void) {
    char buf[] = "Hello world 123";
    ucaseit(buf);
    assert(strcmp(buf, "HELLO WORLD 123") == 0);
}

static void test_lcaseit_empty(void) {
    char buf[] = "";
    lcaseit(buf);
    assert(strcmp(buf, "") == 0);
}

/* ------------------------------------------------------------------ */
/* getword                                                             */
/* ------------------------------------------------------------------ */
static void test_getword_single(void) {
    char *w = getword("hello");
    assert(strcmp(w, "hello") == 0);
}

static void test_getword_first_of_many(void) {
    char *w = getword("first second third");
    assert(strcmp(w, "first") == 0);
}

static void test_getword_empty(void) {
    char *w = getword("");
    assert(strcmp(w, "") == 0);
}

/* ------------------------------------------------------------------ */
/* get_tail                                                            */
/* ------------------------------------------------------------------ */
static void test_get_tail_normal(void) {
    char buf[] = "first second third";
    char *t = get_tail(buf);
    assert(strcmp(t, "second third") == 0);
}

static void test_get_tail_single(void) {
    char buf[] = "only";
    char *t = get_tail(buf);
    assert(strcmp(t, "") == 0);
}

/* ------------------------------------------------------------------ */
/* split                                                               */
/* ------------------------------------------------------------------ */
static void test_split_one_field(void) {
    char buf[] = "hello";
    int n = split(buf);
    assert(n == 1);
    assert(strcmp(fields[0], "hello") == 0);
}

static void test_split_three_fields(void) {
    char buf[] = "a\001b\001c";
    int n = split(buf);
    assert(n == 3);
    assert(strcmp(fields[0], "a") == 0);
    assert(strcmp(fields[1], "b") == 0);
    assert(strcmp(fields[2], "c") == 0);
}

static void test_split_empty_fields(void) {
    char buf[] = "\001\001";
    int n = split(buf);
    assert(n == 3);
    assert(strcmp(fields[0], "") == 0);
    assert(strcmp(fields[1], "") == 0);
    assert(strcmp(fields[2], "") == 0);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    test_filternickname_passthrough();
    test_filternickname_space_becomes_underscore();
    test_filternickname_illegal_becomes_question();
    test_filternickname_special_chars();

    test_filtertext_normal_ascii();
    test_filtertext_replaces_control_chars();
    test_filtertext_truncates_to_len();
    test_filtertext_empty();
#if ALLOW_UTF8_MESSAGES
    test_filtertext_utf8_2byte();
    test_filtertext_utf8_invalid_byte();
#endif

    test_filtergroupname_passthrough();
    test_filtergroupname_special_chars();
    test_filtergroupname_illegal();
    test_filtergroupname_space();

    test_filterfmt_one_s();
    test_filterfmt_wrong_count();
    test_filterfmt_bad_specifier();
    test_filterfmt_double_percent();

    test_lcaseit();
    test_ucaseit();
    test_lcaseit_empty();

    test_getword_single();
    test_getword_first_of_many();
    test_getword_empty();

    test_get_tail_normal();
    test_get_tail_single();

    test_split_one_field();
    test_split_three_fields();
    test_split_empty_fields();

    return 0;
}

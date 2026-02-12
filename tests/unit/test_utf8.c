/*
 * Unit tests for server/utf8.c
 *
 * Tests: decode, countCodePoints
 */

#include <assert.h>
#include <string.h>
#include <stdint.h>

#include "server/mdb.h"
#include "server/utf8.h"

/*
 * utf8.c uses vmdb() in printCodePoints. Provide stubs.
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
/* decode: single byte ASCII                                           */
/* ------------------------------------------------------------------ */
static void test_decode_ascii(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    /* 'A' = 0x41 */
    uint32_t result = decode(&state, &codepoint, 0x41);
    assert(result == UTF8_ACCEPT);
    assert(codepoint == 0x41);
}

/* ------------------------------------------------------------------ */
/* decode: 2-byte sequence (é = U+00E9)                                */
/* ------------------------------------------------------------------ */
static void test_decode_2byte(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    /* é = 0xC3 0xA9 */
    decode(&state, &codepoint, 0xC3);
    assert(state != UTF8_ACCEPT); /* needs more bytes */
    assert(state != UTF8_REJECT);

    uint32_t result = decode(&state, &codepoint, 0xA9);
    assert(result == UTF8_ACCEPT);
    assert(codepoint == 0x00E9);
}

/* ------------------------------------------------------------------ */
/* decode: 3-byte sequence (€ = U+20AC)                                */
/* ------------------------------------------------------------------ */
static void test_decode_3byte(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    /* € = 0xE2 0x82 0xAC */
    decode(&state, &codepoint, 0xE2);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);
    decode(&state, &codepoint, 0x82);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);
    uint32_t result = decode(&state, &codepoint, 0xAC);
    assert(result == UTF8_ACCEPT);
    assert(codepoint == 0x20AC);
}

/* ------------------------------------------------------------------ */
/* decode: 4-byte sequence (𝄞 = U+1D11E)                              */
/* ------------------------------------------------------------------ */
static void test_decode_4byte(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    /* 𝄞 = 0xF0 0x9D 0x84 0x9E */
    decode(&state, &codepoint, 0xF0);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);
    decode(&state, &codepoint, 0x9D);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);
    decode(&state, &codepoint, 0x84);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);
    uint32_t result = decode(&state, &codepoint, 0x9E);
    assert(result == UTF8_ACCEPT);
    assert(codepoint == 0x1D11E);
}

/* ------------------------------------------------------------------ */
/* decode: invalid byte 0xFF                                           */
/* ------------------------------------------------------------------ */
static void test_decode_invalid(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    uint32_t result = decode(&state, &codepoint, 0xFF);
    assert(result == UTF8_REJECT);
}

/* ------------------------------------------------------------------ */
/* decode: truncated 2-byte sequence                                   */
/* ------------------------------------------------------------------ */
static void test_decode_truncated(void) {
    uint32_t state = UTF8_ACCEPT;
    uint32_t codepoint = 0;

    /* Start a 2-byte sequence but feed an invalid continuation */
    decode(&state, &codepoint, 0xC3);
    assert(state != UTF8_ACCEPT && state != UTF8_REJECT);

    /* 0x41 is not a valid continuation byte (should be 10xxxxxx) */
    uint32_t result = decode(&state, &codepoint, 0x41);
    assert(result == UTF8_REJECT);
}

/* ------------------------------------------------------------------ */
/* countCodePoints: ASCII string                                       */
/* ------------------------------------------------------------------ */
static void test_countCodePoints_ascii(void) {
    size_t count = 0;
    int err = countCodePoints((uint8_t *)"Hello", &count);
    assert(err == 0);
    assert(count == 5);
}

/* ------------------------------------------------------------------ */
/* countCodePoints: mixed ASCII + multibyte                            */
/* ------------------------------------------------------------------ */
static void test_countCodePoints_mixed(void) {
    size_t count = 0;
    /* "café" = 'c','a','f',U+00E9 = 4 code points, 5 bytes */
    int err = countCodePoints((uint8_t *)"caf\xc3\xa9", &count);
    assert(err == 0);
    assert(count == 4);
}

/* ------------------------------------------------------------------ */
/* countCodePoints: empty string                                       */
/* ------------------------------------------------------------------ */
static void test_countCodePoints_empty(void) {
    size_t count = 99;
    int err = countCodePoints((uint8_t *)"", &count);
    assert(err == 0);
    assert(count == 0);
}

/* ------------------------------------------------------------------ */
/* countCodePoints: invalid UTF-8                                      */
/* ------------------------------------------------------------------ */
static void test_countCodePoints_invalid(void) {
    size_t count = 0;
    /* 0xFF is never valid */
    int err = countCodePoints((uint8_t *)"\xff", &count);
    assert(err != 0); /* returns non-zero for malformed */
}

/* ------------------------------------------------------------------ */
/* countCodePoints: 4-byte emoji                                       */
/* ------------------------------------------------------------------ */
static void test_countCodePoints_emoji(void) {
    size_t count = 0;
    /* 😀 = U+1F600 = F0 9F 98 80 */
    int err = countCodePoints((uint8_t *)"\xf0\x9f\x98\x80", &count);
    assert(err == 0);
    assert(count == 1);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
int main(void) {
    test_decode_ascii();
    test_decode_2byte();
    test_decode_3byte();
    test_decode_4byte();
    test_decode_invalid();
    test_decode_truncated();

    test_countCodePoints_ascii();
    test_countCodePoints_mixed();
    test_countCodePoints_empty();
    test_countCodePoints_invalid();
    test_countCodePoints_emoji();

    return 0;
}

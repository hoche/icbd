#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdarg.h>

#include "server/mdb.h"
#include "pktserv/pktbuffers.h"

/*
 * pktserv logs through server/mdb.c, but unit tests shouldn't need the full
 * server globals/logfile setup. Provide minimal stubs to satisfy linkage.
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

int sslmdb(const char *str, size_t len, void *u) {
    (void)str;
    (void)len;
    (void)u;
    return 0;
}

static void test_msgbuf_alloc_reuse_clears(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 16);
    assert(b != NULL);
    assert(b->data != NULL);
    assert(b->sz == 16);
    assert(b->len == 0);
    assert(b->pos == b->data);

    memset(b->data, 'x', b->sz);
    b->len = 9;
    b->pos = b->data + 3;

    msgbuf_t *b2 = _msgbuf_alloc(b, 8); /* should reuse and clear */
    assert(b2 == b);
    assert(b->sz == 16);
    assert(b->len == 0);
    assert(b->pos == b->data);
    for (size_t i = 0; i < b->sz; i++) {
        assert(b->data[i] == '\0');
    }

    _msgbuf_free(b);
}

static void test_msgbuf_alloc_grows(void) {
    msgbuf_t *b = _msgbuf_alloc(NULL, 8);
    assert(b != NULL);
    assert(b->sz == 8);
    assert(b->data != NULL);

    char *old_data = b->data;
    msgbuf_t *b2 = _msgbuf_alloc(b, 64);
    assert(b2 == b);
    assert(b->sz == 64);
    assert(b->data != NULL);
    assert(b->data != old_data);
    assert(b->len == 0);
    assert(b->pos == b->data);

    _msgbuf_free(b);
}

static void test_socket_state_str(void) {
    assert(strcmp(socketStateStr(DISCONNECTED), "DISCONNECTED") == 0);
    assert(strcmp(socketStateStr(WANT_HEADER), "WANT_HEADER") == 0);
    assert(strcmp(socketStateStr(LISTEN_SOCKET_SSL), "LISTEN_SOCKET_SSL") == 0);
    assert(strcmp(socketStateStr((SocketState)9999), "UNKNOWN") == 0);
}

int main(void) {
    test_msgbuf_alloc_reuse_clears();
    test_msgbuf_alloc_grows();
    test_socket_state_str();
    return 0;
}


/*
 * sslconf.c
 *
 * set up an SSL ctx
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong
 * All rights reserved.
 *
 */

#include "config.h"

#ifdef HAVE_SSL

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#if (SSLEAY_VERSION_NUMBER >= 0x0907000L)
#include <openssl/conf.h>
#endif

#include "server/mdb.h"

SSL_CTX *ctx = NULL;

int init_openssl_library(void)
{
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
#else
    OPENSSL_init_ssl(0, NULL);
#endif

    SSL_load_error_strings();
    /* OPENSSL_config(NULL); */

    OpenSSL_add_ssl_algorithms();

    return(1);
}

/* init the SSL library and create a new SSL_CTX, using the passed-in
 * PEM file.
 *  * use SSL_CTX_free(SSL_CTX *ctx) to free the created ctx.
 */
int init_ssl(char *pem)
{
    int result;
    const SSL_METHOD *method;

    init_openssl_library();

#if HAVE_TLS_SERVER_METHOD
    method = TLS_server_method();
#else
    /* TLS_server_method() is unavailable; use the generic server method and
     * disable legacy protocols via SSL_CTX_set_options below. */
    method = SSLv23_server_method();
#endif

    if (ctx != NULL) {
        SSL_CTX_free(ctx);
    }

    ctx = SSL_CTX_new(method);
    if (!ctx) {
        vmdb(MSG_ERR, "Unable to create SSL ctx.");
        ERR_print_errors_cb(sslmdb, NULL);
        return -1;
    }

    /* Harden defaults */
    SSL_CTX_set_options(ctx,
                        SSL_OP_NO_SSLv2 |
                        SSL_OP_NO_SSLv3 |
                        SSL_OP_NO_COMPRESSION |
                        SSL_OP_CIPHER_SERVER_PREFERENCE);

#if HAVE_SSL_CTX_SET_MIN_PROTO_VERSION
#ifdef TLS1_2_VERSION
    if (!SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION)) {
        vmdb(MSG_WARN, "Could not set minimum TLS version to 1.2");
    }
#endif
#else
    /* Best-effort legacy hardening when set_min_proto_version is unavailable */
    SSL_CTX_set_options(ctx,
                        SSL_OP_NO_TLSv1 |
                        SSL_OP_NO_TLSv1_1);
#endif

    /* TLS 1.2 and below cipher list */
    if (!SSL_CTX_set_cipher_list(ctx, "HIGH:!aNULL:!eNULL:!MD5:!RC4:!3DES:!DES:!EXP:!PSK:!SRP")) {
        vmdb(MSG_WARN, "Could not set TLS cipher list; using library defaults");
        ERR_print_errors_cb(sslmdb, NULL);
    }

#if HAVE_SSL_CTX_SET_CIPHERSUITES
    /* TLS 1.3 cipher suites (OpenSSL 1.1.1+) */
    if (!SSL_CTX_set_ciphersuites(ctx, "TLS_AES_256_GCM_SHA384:TLS_AES_128_GCM_SHA256:TLS_CHACHA20_POLY1305_SHA256")) {
        vmdb(MSG_WARN, "Could not set TLS 1.3 cipher suites; using library defaults");
    }
#endif

    SSL_CTX_set_verify(ctx, SSL_VERIFY_NONE, NULL);

    result = SSL_CTX_use_PrivateKey_file(ctx, pem,  SSL_FILETYPE_PEM);
    if (result <= 0) {
        vmdb(MSG_ERR, "No SSL private key found in %s.", pem);
        return -1;
    }

    result = SSL_CTX_use_certificate_file(ctx, pem, SSL_FILETYPE_PEM);
    if (result <= 0) {
        vmdb(MSG_ERR, "No SSL cert file found in %s.", pem);
        return -1;
    }

    result = SSL_CTX_check_private_key(ctx);
    if (!result) {
        vmdb(MSG_ERR, "SSL key mismatch in %s.", pem);
        return -1;
    }

    return 0;
}

#endif /* HAVE_SSL */

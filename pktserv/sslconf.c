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
    BIGNUM *bne = NULL;
    RSA *rsa = NULL;
    int bits = 2048;
    unsigned long e = RSA_F4;

    init_openssl_library();

#if HAVE_TLS_SERVER_METHOD
    method = TLS_server_method();
#else
    method = TLSv1_1_server_method();
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

    bne = BN_new();
    result = BN_set_word(bne, e);
    if (!result) {
        vmdb(MSG_ERR, "Couldn't create BIGNUM.");
        return -1;
    }

    rsa = RSA_new();
    result = RSA_generate_key_ex(rsa, bits, bne, NULL);
    if (!result) {
        vmdb(MSG_ERR, "Couldn't generate RSA key.");
        return -1;
    }

    if (!SSL_CTX_set_tmp_rsa(ctx, rsa)) {
        vmdb(MSG_ERR, "Couldn't assign temporary RSA key to SSL context.");
        return -1;
    }

    return 0;
}

#endif /* HAVE_SSL */

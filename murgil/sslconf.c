/* Copyright (c) 2001 Michel Hoche-Mong, All rights reserved. */

#include "config.h"

#ifdef HAVE_SSL

#include <openssl/err.h>
#include <openssl/ssl.h>
#include "../server/mdb.h"  /* for mdb() */


/* create a new SSL_CTX, using the pems defined in config.h.
 * use SSL_CTX_free(SSL_CTX *ctx) to free the created ctx.
 */
int create_ssl_context(char *pem, SSL_CTX **ctx)
{
    int result;
    SSL_CTX *context;
    RSA *rsa;

#if HAVE_TLS_SERVER_METHOD
    context = SSL_CTX_new(TLS_server_method());
#else
    context = SSL_CTX_new(TLSv1_1_server_method());
#endif

    SSL_CTX_set_verify(context, SSL_VERIFY_NONE, NULL);

    result = SSL_CTX_use_PrivateKey_file(context, pem,  SSL_FILETYPE_PEM);
    if (result <= 0) {
        vmdb(MSG_ERR, "No SSL private key found in %s.", pem);
        return -1;
    }

    result = SSL_CTX_use_certificate_file(context, pem, SSL_FILETYPE_PEM);
    if (result <= 0) {
        vmdb(MSG_ERR, "No SSL cert file found in %s.", pem);
        return -1;
    }

    result = SSL_CTX_check_private_key(context);

    if (!result) {
        vmdb(MSG_ERR, "SSL key mismatch in %s.", pem);
        return -1;
    }

    rsa = RSA_generate_key(512, RSA_F4, NULL, NULL);

    if (!SSL_CTX_set_tmp_rsa(context, rsa)) {
        vmdb(MSG_ERR, "Couldn't create a temporary RSA key.", pem);
        return -1;
    }

    *ctx = context;
    return 0;
}

#endif

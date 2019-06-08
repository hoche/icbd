/*
 * pktserv.h
 *
 * Entry points into a packet server.
 *
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong.
 * All rights reserved.
 *
 */

#pragma once

typedef void (pktserv_idle_cb)(int i);
typedef void (pktserv_dispatch_cb)(int i, char *data);
typedef void (pktserv_new_client_cb)(int i, int secure);
typedef void (pktserv_lost_client_cb)(int i);
typedef int  (pktserv_ok2read_cb)(int i);

/* Callback functions */
typedef struct pktserv_cb_st {
    pktserv_idle_cb              *idle;
    pktserv_dispatch_cb          *dispatch;
    pktserv_new_client_cb        *new_client;
    pktserv_lost_client_cb       *lost_client;
    pktserv_ok2read_cb           *ok2read;
} pktserv_cb_t;

int pktserv_init(char *config, pktserv_cb_t *cb);
int pktserv_addport(char *host_name, int port_number, int is_ssl);
int pktserv_send(int s, char *pkt, size_t len);
int pktserv_disconnect(int s);


void pktserv_run(void);

void pktserv_dumpsockets(FILE *dump);
void pktserv_loadsockets(FILE *dump);

#ifdef HAVE_SSL
#include <openssl/ssl.h>
int init_ssl(char *pem);
#endif /* HAVE_SSL */

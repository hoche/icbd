/*
 * sslsocket.h
 *
 * wrappers around standard SSL network connection calls, using
 * cbuf_t's.
 *
 * These shouldn't be called directly and instead should be called
 * from the functions in pktsocket.c.
 *
 * Author: Michel Hoche-Mong
 * Copyright (c) 2001-2026 Michel Hoche-Mong
 * All rights reserved.
 *
 */

#pragma once

#include "pktbuffers.h"

int sslsocket_accept(cbuf_t *cbuf);
int sslsocket_read(cbuf_t *cbuf, void* buf, size_t len);
int sslsocket_write(cbuf_t *cbuf, void* buf, size_t len);


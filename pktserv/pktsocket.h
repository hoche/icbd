/*
 * pktsocket.h
 *
 * packet server socket handlers
 *
 * Author: Michel Hoche-Mong, hoche@grok.com
 * Copyright (c) 2001-2019 Michel Hoche-Mong.
 * All rights reserved.
 *
 */

#pragma once

#include "pktbuffers.h"

/* accept and initialize a new client connection
 *
 * returns 0 if the operation was successful or would block.
 *    (sets the listen socket's disposition to BLOCKED if
 *     it would block.)
 * return -1 if the operation failed with a critical error.
 */
int pktsocket_accept(cbuf_t *listen_cbuf);

/* read a packet of data into a cbuf's rbuf
 *
 * return 0 on success or non-critical error (like EWOULDBLOCK)
 *          if the packet is complete, call s_packet with the cbuf.
 * return -1 and set the cbuf state to WANT_DISCONNECT on error
 */
int pktsocket_read(cbuf_t *cbuf);

/*
 * write a date from the cbuf's send list to socket file descriptor 
 *
 * returns -1 on connection gone, too many retries have been attempted,
 *    or there was some other serious problem.
 * returns 0 if write or partial write succeeded.
 *
 * Note: this currently tries to write everything from the cbuf's
 *   sendlist at once.
 */
int pktsocket_write(cbuf_t* cbuf);

/* public callback to send a packet to a client. adds a packet to a client's
 * list of packets that need to be sent, and then tries to call _sendpacket
 * to send 'em.
 *
 * if the client already has MAX_SENDPACKET_QUEUE packets waiting to go,
 * return -1.
 *
 */
int pktsocket_send(int s, char *pkt, size_t len);


/* Copyright 2001, Michel Hoche-Mong.
 */

#pragma once

extern int icbd_log;
extern int log_level;

#define MSG_ALL (-1)	/* special case */
#define MSG_NONE 0		/* we don't use this. it's just a placeholder */
#define MSG_ERR 1
#define MSG_WARN 2
#define MSG_INFO 3
#define MSG_DEBUG 4
#define MSG_VERBOSE 5

/* Message should be somewhat less than BUFSIZ chars, as the bufferspace
 * for message + hostname + timestamp is BUFSIZ. longer messages
 * will get truncated.
 */

void mdb(int level, const char *message);
void vmdb(int level, const char *fmt, ...);
int sslmdb(const char * str, size_t len, void *u);


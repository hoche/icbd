/* Copyright (c) 1990 by Carrick Sean Casey. */
/* For copying and distribution information, see the file COPYING. */

/* In the future, this file will include everything a user needs to */
/* know about the client-server protocol */

#pragma once

#include <sys/types.h>
#include <string.h>

/* message types */
/* note: changing these necessitates upgrading the protocol number */

#define ICB_M_LOGIN	'a'	/* login packet */
#define ICB_M_LOGINOK	'a'	/* login packet */
#define ICB_M_OPEN	'b'	/* open msg to group */
#define ICB_M_PERSONAL	'c'	/* personal msg */
#define ICB_M_STATUS	'd'	/* status update message */
#define ICB_M_ERROR	'e'	/* error message */
#define ICB_M_IMPORTANT	'f'	/* special important announcement */
#define ICB_M_EXIT	'g'	/* tell other side to exit */
#define ICB_M_COMMAND	'h'	/* send a command from user */
#define ICB_M_CMDOUT	'i'	/* output from a command */
#define ICB_M_PROTO	'j'	/* protocol version information */
#define ICB_M_BEEP	'k'	/* beeps */
#define ICB_M_PING	'l'	/* ping packet */
#define ICB_M_PONG	'm'	/* return for ping packet */

/* These are new definitions for the protocol */
#define ICB_S_PERSONAL	ICB_M_PERSONAL
#define ICB_M_NOOP	'n'	/* no-op: this is for keeping tcp connection
				   active w/out affecting idletime */


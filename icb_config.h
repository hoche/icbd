/* File:        icb_config.h
 * Created:     19/05/21
 * Author:      Michel Hoche-Mong
 *
 * Based (loosely) on the original written for icb by Mark 
 * Giaquinto, copyright (C) 1991 
 */

#pragma once

#define POLL_TIMEOUT   1   /* number of sec to wait in select before
                            * doing idle maintenance stuff. set to 0 to
                            * have no wait time, and -1 to have select
                            * block until there's activity.
                            */

/* Our packets are in the following format:
 * <unsigned length byte><cmd byte><data><NULL>
 *
 * The length does not include the length byte itself but does
 * include the trailing NULL.
 *
 * Thus, the maximum we *ever* send is 256 bytes; the length byte
 * plus 255 bytes of packet contents.
 */
#define MAX_PKT_LEN 256

/* This is the maximum data in a packet, not including the length
 * byte or the command byte, but including the trailing NULL.
 */
#define MAX_PKT_DATA (MAX_PKT_LEN - 2)

#define	MSG_BUF_SIZE	BUFSIZ	/* general purpose buffer for logging */
#define WARN		1
#define FATAL		2

/* File definitions */

#define USERDB "./icbdb"
#define ICBDLOG "./icbd.log"
#define ICBDMOTD "./motd"
#define ICBDHELP "./icbd_help"
#define ICBPEMFILE "./icbd.pem"


#undef	NO_DOUBLE_RES_LOOKUPS	/* define to disable double-reverse lookups */

#define DEFAULT_PORT 7326
#define DEFAULT_SSLPORT 7327
 
#define MAX_GROUPS	300	/* total groups */
#define MAX_FIELDS      20      /* fields in a packet */
#define MAX_GROUPLEN    8       /* chars in a group name */
#define MAX_IDLEN       12      /* chars in a login id */
#define MAX_INPUTSTR    (250 - MAX_NICKLEN - MAX_NICKLEN - 6) /* input line */
#define MAX_INVITES     50      /* invitations in a group */
#define MAX_NICKLEN     12      /* chars in a username */
#define MAX_NODELEN     64      /* chars in a node name */
#define MAX_NOPONG      600     /* seconds a client may not PONG a PING */
#define MAX_PASSWDLEN   12      /* chars in a user password */
#define MAX_REALLEN     64      /* chars in a real life name */
#define MAX_TEXTLEN     (80 - MAX_NICKLEN - 5)  /* max chars in a message */
#define MAX_TOPICLEN    32      /* chars in a group topic */
#define MAX_HUSHED      32      /* maximum number of hushed people */
#define MAX_NOTIFIES    40      /* maximum notifies allowed */

#define MAX_WRITES	20	        /* maximum writes allowed to nickname */
#define MAX_AWAY_LEN	(MAX_INPUTSTR - 17)
#define AWAY_NOSEND_TIME 10     /* don't send away messages to the same user
                                 * if you've already sent one within this 
                                 * number of seconds 
                                 */
#define MAX_SENDPACKET_QUEUE 10  /* maximum number of writes to queue up */
#define MAX_SENDPACKET_RETRIES 10  /* maximum number of retries per write */


/*
 * these are all of the idle behaviour settings
 */
/* maximum idle seconds before disconnect. 14400 = 4hrs, a good default */
#undef MAX_IDLE        /* note: undef to disable */

/* default secs before users are booted to IDLE_GROUP */
#define	DEF_IDLE_BOOT	3600

/* seconds before being disconnected that idle users are given a warning msg */
#define IDLE_WARN		240

/* the warning message displayed before being idle-disconnected */
#define	IDLE_WARN_MSG	"Your connection will be dropped in %d minutes due to idle timeout."

/* seconds before moderator in a group times out (for registered mods only) */
#define MOD_TIMEOUT		300

/* max secs before idle mods give up mod */
#define	MAX_IDLE_MOD	14400

/* maximum idle seconds that can be set for a group */
#define MAX_MOD_IDLE	600

/* default idle mod setting when a group is created */
#define	DEF_IDLE_MOD	3600

/*
 * in the following, %s is replaced with the nickname of the user for the
 * messages sent to other users in the group and "you" for the user being 
 * affected
 */

/* default msg for being idle booted from a group. */
#define	IDLE_BOOT_MSG	"A swarm of bats swoops down and carries %s away.";

/* default msg for a moderator losing mod from being too idle */
#define	IDLE_MOD_MSG		"A piano suddenly falls on %s, dislodging moderatorship of %s.";


#define PID_FILE "./icbd.pid"

#define ACCESS_FILE "./icbd.deny"

#define PERM_FILE "./icbd.perms"

/* Since sometimes gethostname doesn't do it right, if the hostname returned
   is SHORT_HOSTNAME, FQDN is used instead. These are only checked if they
   are defined.
*/
/* #define SHORT_HOSTNAME "icb"		*/
/* #define FQDN "default.icb.net"	*/

#define	IDLE_GROUP	"~IDLE~"	/* ~ will put it at the end */

#define	BOOT_GROUP	"~RESEDA~"	/* o/~ we are all going to reseda o/~ */

/* hostname to actually bind and listen as. set to null if you want to
   listen on all interfaces you have or depend on the -b flag */
/* #define BIND_HOSTNAME "default.icb.net" */

/* Allow messages to be UTF-8 ones instead of having the server substitute
 * "?" for any character that's not in the 127-bit ascii range.
 *
 * Nicks and groupnames still are restricted to a limited subset of the
 * 127-bit ascii characters.
 */
#define ALLOW_UTF8_MESSAGES 1

#ifdef BRICK
#define STARTING_BRICKS 5
#define MAX_BRICKS      20 
#endif


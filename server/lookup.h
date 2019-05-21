#ifndef _LOOKUP_H_
#define _LOOKUP_H_

#define	CMD_DROP	0
#define	CMD_SHUTDOWN	1
#define	CMD_WALL	2
#define	CMD_BEEP	3
#define	CMD_CANCEL	4
#define	CMD_G		5
#define	CMD_INVITE	6
#define	CMD_PASS	7
#define	CMD_BOOT	8
#define	CMD_STATUS	9
#define	CMD_TOPIC	10
#define	CMD_MOTD	11
#define	CMD_M		12
#define	CMD_ECHOBACK	13
#define	CMD_NAME	14
#define	CMD_V		15
#define	CMD_W		16
#define CMD_INFO	17
#define CMD_RESTART	18
#define CMD_NEWS	19
#define CMD_HUSH	20
#define CMD_SHUSH	21
#define CMD_HELP	22
#define CMD_EXCLUDE	23
#define CMD_SHUTTIME	24
#define CMD_NOTIFY	25
#define CMD_TALK	26
#define CMD_PING	27
#define CMD_NOBEEP	28
#define CMD_AWAY	29
#define CMD_NOAWAY	30
#define CMD_LOG 	31
#define CMD_STATS 	32

#define SET_RESTRICT	0
#define	SET_MODERATE	1
#define	SET_PUBLIC	2

#define	SET_INVISIBLE	3
#define	SET_SECRET	4
#define	SET_VISIBLE	5

#define SET_QUIET	6
#define SET_NORMAL	7
#define SET_LOUD	8
#define	SET_NAME	9
#define SET_QUESTION	10

/* 
 * note: we'd like to have this set up with the other controls, but
 * it's probably best to put it on the end for the sake of backwards
 * compatability until the organization methodology is better designed
 * with scalability in mind
 */
#define SET_CONTROL		11
#define SET_SIZE		12
#define SET_IDLEBOOT		13
#define SET_IDLEBOOT_MSG	14
#define SET_IDLEMOD		15

#define AUTO_READ	0
#define	AUTO_WHO	1
#define	AUTO_REGISTER	2
#define	AUTO_REAL	3
#define	AUTO_WRITE	4
#define	AUTO_TEXT	5
#define	AUTO_ADDR	6
#define	AUTO_PHONE	7
#define	AUTO_DELETE	8
#define	AUTO_INFO	9
#define	AUTO_HELP	10
#define	AUTO_QUESTION	11
#define AUTO_CP		12
#define AUTO_EMAIL	13
#define AUTO_SECURE	14
#define AUTO_NOSECURE	15
#define AUTO_WWW	16


int lookup(char *s, char *table[]);


#endif /* #ifndef _LOOKUP_H_ */

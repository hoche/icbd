#ifndef _ACCESS_H_
#define _ACCESS_H_

#ifdef HAVE_NDBM_H
#include <ndbm.h>
#elif defined (HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined (HAVE_DB1_NDBM_H)
#include <db1/ndbm.h>
#endif

int setsecure(int forWhom, int secure, DBM *openDb);
int valuser(char *user, char *password, DBM *openDb);
int check_auth(int n);

int nickdelete(int forWhom, char *password, DBM *openDb);
int nickwritemsg(int forWhom, char *user, char *message, DBM *openDb);
int nickckmsg(int forWhom, DBM *openDb);
int nickreadmsg(int forWhom, DBM *openDb);
int nickwritetime(int forWhom, int class, DBM *openDb);
int nickchinfo(int forWhom, const char *tag, char *data, unsigned int max, const char *message, DBM *openDb);
int nickchpass(int forWhom, char *oldpw, char *newpw, DBM *openDb);

/*
 * nickwrite() - register a nick, creating it's database entry if need be,
 * otherwise verifying the password on the existing record
 *
 * args:
 *  forWhom - index into the user table
 *  password - password for the nick
 *  verifyOnly - set to 1 to prevent nicks from being created. ie, return
 *    with a failure code if the nick isn't already in the db
 *
 * returns: 0 on success, -1 on failure
 */
int nickwrite(int forWhom, char *password, int verifyOnly, DBM *openDb);
int nicklookup(int forWhom, const char *theNick, DBM *openDb);

#endif /* #ifndef _ACCESS_H_ */

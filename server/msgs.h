#ifndef _S_MSGS_H_
#define _S_MSGS_H_

/* open message
 *  note that we need to know who sent the message.  it doesn't
 *  come with the sender's nick, so we have to reformat the message
 *  before sending it to the group
 *
 *   n   	socket on which they sent the message
 *   pkt	packet buffer
 */

void openmsg(int n, char *pkt);

/*
 *  they sent us a login message. we need to put their info into 
 *  the user information table and then send them back a loginok message.
 *  for any field they didn't specify, come up with a plausible default.
 *  we also need to handle "w" vs. "login"
 *  we also need to dump them if they try using a:
 *	nickname already in use -or-
 *	group they aren't allowed to enter.
 *   their loginid may NOT be blank or null
 *
 *   n   	socket on which they sent the message
 *   pkt	packet buffer
 *
 *   returns 0 if everything went ok, -1 if it was an unknown login type
 */

int loginmsg(int n, char *pkt);

/* command message
 *
 *   n   	socket on which they sent the message
 *   pkt	packet buffer
 */

void cmdmsg(int n, char *pkt);


/* pong message
 *
 *   n   	socket on which they sent the message
 *   pkt	packet buffer
 */

void pong(int n, char *pkt);


/*
 * ok2read()
 *
 * - this function is called every time a socket is ready to read some data.
 *   therefore, here is where we apply the SLOWMSGS permissions so that
 *   certain connections' data is read less frequently than normal
 *
 *   n   	socket to inspect
 */
int ok2read(int n);


#endif /* #ifndef _S_MSGS_H_ */

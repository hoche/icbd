#ifndef _DISPATCH_H_
#define _DISPATCH_H_

/* dispatch()
 *
 * dispatch to the appropriate function based on the type of message
 * (as determined by the first character in the pkt buffer).
 *
 * n = socket on which the message arrived
 * pkt = packet buffer 
 *
 */ 
void dispatch(int n, char *pkt);

#endif /* #ifndef  _DISPATCH_H_ */

#ifndef _ICBUTIL_H_
#define _ICBUTIL_H_

#include "config.h"

/* icbopenlogs(void)
 *
 * open the log file with append
 */
RETSIGTYPE icbopenlogs(int sig);

/* icbcloselogs
 *
 * close the current log file
 */
RETSIGTYPE icbcloselogs(int sig);

/* icbcyclelogs
 *
 * close and reopen the log files
 */
RETSIGTYPE icbcyclelogs(int sig);


/* icbexit
 *
 * disconnect all users and exit
 */
RETSIGTYPE icbexit(int sig);


/* icbdump
 *
 * dump the current state to a file so if we restart we can pick up
 * where we left off.
 */
RETSIGTYPE icbdump(int sig);

/* icbload
 *
 * load a server state from a file created by icbdump
 */
RETSIGTYPE icbload(int sig);

#endif /* #ifndef _ICBUTIL_H_ */

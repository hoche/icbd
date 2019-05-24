/* Copyright 2001, Michel Hoche-Mong
 *
 * Released under the GPL.
 *
 */

#include "config.h"

#include <unistd.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_STDARG_H
#include "stdarg.h"
#endif
#include "mdb.h"
#include "externs.h" /* for thishost */

int icbd_log;
int log_level;

/* update num_msg_types if you add to this list */
const char* msgtypelist[] = {
    "[ERR] ",
    "[WARN] ",
    "[INFO] ",
    "[DEBUG] ",
    "[VERBOSE] ",
    NULL
};
int num_msg_types = 5;


void mdb(int level, const char *message)
{
    char timebuf[64];
    char tmp[BUFSIZ];
    time_t now;
    const char* msgtype = NULL;

    if ((level == MSG_ALL || level <= log_level) && icbd_log >= 0) {
        if (level > 0 && level <= num_msg_types)
            msgtype = msgtypelist[level-1];
        else
            msgtype = "";

        now = time(NULL);
        strftime(timebuf, 64, "%b %d %Y %H:%M:%S", localtime(&now));
        snprintf(tmp, BUFSIZ, "%s %s %s: %s\n", 
                 msgtype, timebuf, thishost, message);

      	/* the "void"! ... is an obscure construct to get around the compiler
	 * warning that we're not checking the return value. There's not
	 * a whole lot we can do if we can't write at this point. */
        (void)!write(icbd_log, tmp, strlen(tmp));
    }
}



void vmdb (int level, const char *fmt, ...)
{
#ifdef HAVE_STDARG_H
    va_list ap;
    char tmp[BUFSIZ];

    if ( (level == MSG_ALL || level <= log_level) && icbd_log >= 0)
    {
        va_start(ap, fmt);
        vsnprintf(tmp, BUFSIZ, fmt, ap);
        va_end(ap);

        mdb(level, tmp);
    }
#endif /* HAVE_STDARG_H */
}

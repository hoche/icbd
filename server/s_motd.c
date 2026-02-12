/* Copyright 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

/* primitive to print the motd */

#include "config.h"

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "server.h"
#include "externs.h"
#include "mdb.h"
#include "send.h"
#include "icbdb.h"

int s_motd(int n, int argc)
{
    int motd_fd;
    char c;
    char temp[256];
    int i;
    size_t used = 0;

    motd_fd = open(ICBDMOTD, O_RDONLY);

    memset(temp, 0, sizeof(temp));
    /* if the file is there, list it, otherwise report error */
    if (motd_fd >= 0) 
    {
        while ((i = read(motd_fd, &c, 1)) > 0) 
        {
            if (c == '%') 
            {
                i = read(motd_fd, &c, 1);
                if (i <= 0) {
                    if (close(motd_fd) != 0) {
                        sprintf(mbuf, "MOTD File Close: %s", strerror(errno));
                        mdb(MSG_ERR, mbuf);
                    }
                }
                else if (c == 'U') {
                    char    *value;
                    if (icbdb_get ("server", "signon", ICBDB_STRING, &value)) {
                        size_t remain = sizeof(temp) - 1 - used;
                        size_t add = strnlen(value, remain);
                        if (add > 0) {
                            memcpy(temp + used, value, add);
                            used += add;
                            temp[used] = '\0';
                        }
                    }
                }
                else if (used < sizeof(temp) - 1) {
                    temp[used++] = c;
                    temp[used] = '\0';
                }
            }
            else if (c == '\012') {
                sends_cmdout(n, temp);
                memset(temp, 0, sizeof(temp));
                used = 0;
            }
            else if (used < sizeof(temp) - 1) {
                temp[used++] = c;
                temp[used] = '\0';
            }
        }
        if (close(motd_fd) != 0) {
            sprintf(mbuf, "MOTD File Close: %s",
                    strerror(errno));
            mdb(MSG_ERR, mbuf);
        }
    } else {
        sprintf(mbuf, "MOTD File Open: %s", 
                strerror(errno));
        mdb(MSG_ERR, mbuf);
        senderror(n, "No MOTD file found.");
    }

    return 0;
}

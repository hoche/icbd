/* Copyright (c) 1988 Carrick Sean Casey. All rights reserved. */

#include "config.h"

#include <sys/types.h>
#include <sys/socket.h>

#include "globals.h"

/* turn off input polling for a user */

void ignore(int user)
{
    FD_CLR(user, &rfdset);
    FD_SET(user, &ignorefdset);
}


/* restore input polling for a user */

void unignore(int user)
{
    FD_SET(user, &rfdset);
    FD_CLR(user, &ignorefdset);
}


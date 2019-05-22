#pragma once

/* stash the current time in curtime */
void gettime(void);

/* set line buffering for an open file pointer */
/* output will be flushed every newline */
void linebuffer(FILE *fp);

void clearargs(int argc, char *argv[]);


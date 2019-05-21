#ifndef _ICB_UNIX_H_
#define _ICB_UNIX_H_

/* stash the current time in curtime */
void gettime(void);

/* set line buffering for an open file pointer */
/* output will be flushed every newline */
void linebuffer(FILE *fp);

void clearargs(int argc, char *argv[]);

#endif /* #ifndef _ICB_UNIX_H_ */

#ifndef _MURGIL_H_
#define _MURGIL_H_

int makeport(char *host_name, int port_number);
void serverserve(void);
int sendpacket(int s, char *pkt, size_t len);
void disconnectuser(int userfd);
void ignore(int user);
void unignore(int user);

#endif /* #ifdef _MURGIL_H_ */

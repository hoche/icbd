#ifndef _SEND_H_
#define _SEND_H_

/* send normal open group message to the client */
void sendopen(int from, int to, const char *txt);

/* send an exit message to the client -- makes the client disconnect */
void sendexit(int to);

/* send a ping */
void sendping(int to, const char *who);

/* send an error message to the client */
void senderror(int to, const char *error_string);

/* send a status message to the client */
void sendstatus(int to, const char *class_string, const char *message_string);

/* send "end of command" to the client */
void send_cmdend(int to, const char *output_string);

/* send simple command output message to the client */
void sends_cmdout(int to, const char *output_string);

/* send personal message to a client */
void sendperson(int from, int to, const char *message_string);

/* send beep message to a client */
void sendbeep(int from, int to);

/* send a connect message
 * n  =  fd of their socket 
 */ 
void s_new_user(int n);

/* construct loginok message */
void send_loginok(int to);

/* send an important message to the client */
void sendimport(int to, const char *class_string, const char *output_string);

/* send an autobeep to the client */
void autoBeep(int to);

/* send "W" information to the client */
void user_wline(int to, 
		char *mod, 
		char *nick, 
		int idle, 
		int resp, 
		int login, 
		char *user, 
		char *site, 
		char *name);


/* XXX what the hell does this do? what are c and d? -hoche 5/10/00 */
/*
void user_wgroupline(int to, char *group, char* topic, c, d)
{ 
	sprintf(pbuf,
		"%cwg\001%s\001%s\001%s\001%s\000",
		ICB_M_CMDOUT, group, topic, c, d);
	doSend(-1, to);
}
*/

void user_whead(int to);

/* send a text message to the client */
int  doSend(int from, int to);

#endif /* #ifdef _SEND_H_ */

/* Copyright (c) 1991 by John Atwood deVries II. */
/* For copying and distribution information, see the file COPYING. */

#pragma once

void c_packet(char *pkt);
void c_didpoll(void);
void c_lostcon(void);
void s_didpoll(int n);
void c_userchar(void);
void s_lost_user(int n); 		/* n = fd of that user */
void s_packet(int x, char *pkt);
void s_new_user(int n, int secure);


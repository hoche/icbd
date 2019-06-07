/* Just a list of all the s_commands. The code for these is contained in
 * the s_foo.c files.
 */

#pragma once

int s_auto(int n, int f);
int s_drop(int n, int argc);
int s_shutdown(int n, int argc);
int s_restart(int n, int argc);
int s_wall(int n, int argc);
int s_beep(int n, int argc);
int s_nobeep(int n, int argc);
int s_away(int n, int argc);
int s_noaway(int n, int argc);
int s_brick(int n, int argc);
int s_cancel(int n, int argc);
int s_change(int n, int argc);
int s_invite(int n, int argc);
int s_pass(int n, int argc);
int s_boot(int n, int argc);
int s_status(int n, int argc);
int s_topic(int n, int argc);
int s_send_group(int n);
int s_exclude(int n, int argc);
int s_status_group(int k, 
		int tellme, int n, const char *class_string, const char *message_string);
int s_info(int n, int argc);
int s_motd(int n, int argc);
int s_news(int n, int argc);
int s_personal(int n, int argc);
int s_shuttime(int n, int argc);

int s_help(int n, int argc);
int s_hush(int n, int argc);
int s_name(int n, int argc);
int s_notify(int n, int argc);
int s_echoback(int n, int argc);

int s_version(int n, int argc);
int s_who(int n, int argc);

int s_ping (int n, int argc);

int s_log(int n, int argc);

/*
 * s_talk()
 * - function to modify the group list of nicknames with permission to
 *   talk within a controlled group
 */
int s_talk(int n, int argc);

/* not really server commands */
void talk_report(int n, int gi);
void away_handle(int src, int dest);



#include "config.h"

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_NDBM_H
#include <ndbm.h>
#elif defined (HAVE_GDBM_NDBM_H)
#include <gdbm/ndbm.h>
#elif defined (HAVE___GDBM_NDBM_H)
#include <gdbm-ndbm.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "server.h"
#include "externs.h"
#include "send.h"
#include "users.h"
#include "mdb.h"
#include "strutil.h"
#include "unix.h"

#include "s_commands.h"  /* for talk_report() */
#include "icbdb.h"

int setsecure(int forWhom, int secure, DBM *openDb)
{
    int            retval = 0;

    if (strlen(u_tab[forWhom].realname) == 0) {
        senderror(forWhom, 
                  "You must be registered to change your security.");
        return -1;
    }

    if (secure == 0) 
    {
        icbdb_delete (u_tab[forWhom].nickname, "secure");
        sends_cmdout (forWhom, "Security set to automatic.");
    }
    else if (secure == 1) 
    {
        icbdb_set (u_tab[forWhom].nickname, "secure", ICBDB_STRING,
                   "SECURED");
        sends_cmdout (forWhom, "Security set to password required.");
    }
    else 
    {
        mdb(MSG_ERR, "Illegal setsecure value");
        retval = -2;
    }

    return retval;
}

int valuser(char *user, char *password, DBM *openDb)
{
    char        *value;

    if (strlen(password) == 0)
        return -1;

    if ( strlen(password) > MAX_PASSWDLEN )
        password[MAX_PASSWDLEN] = '\0';

    if (!icbdb_get (user, "password", ICBDB_STRING, &value))
        return -1;    /* not found */

    if (strcmp(value, password) != 0)
        return -1;
    else
        return 0;
}

int check_auth(int n)
{
    int auth;

    auth = 0;
    if (strcasecmp(u_tab[n].nickname, "ADMIN") == 0)
        auth++;
    if (strcasecmp(u_tab[n].password, ADMIN_PWD) == 0)
        auth++;
    sprintf(mbuf, "Checking authorization of %s: %s",
            u_tab[n].nickname, auth == 2 ? "yes" : "no");
    mdb(MSG_INFO, mbuf);
    return (auth == 2);
}

int nickdelete(int forWhom, char *password, DBM *openDb)
{
    int            i;
    char        *nick;
    char        *value;

    if ( strlen (password) == 0 )
    {
        senderror (forWhom, "Password cannot be null");
        return -1;
    }

    if ( strlen(password) > MAX_PASSWDLEN )
        password[MAX_PASSWDLEN] = '\0';

    if (strlen(u_tab[forWhom].realname) == 0) {
        senderror(forWhom, 
                  "You must be registered to delete your entry.");
        return -1;
    }

    nick = u_tab[forWhom].nickname;
    icbdb_open ();

    if (!icbdb_get (nick, "password", ICBDB_STRING, &value)) {
        senderror (forWhom, "You don't have a password.");
        icbdb_close ();
        return -1;
    }

    if ( strcmp (value, password) != 0 ) {
        senderror (forWhom, "Password incorrect.");
        icbdb_close ();
        return -1;
    }

    icbdb_delete (nick, "secure");
    icbdb_delete (nick, "realname");
    icbdb_delete (nick, "email");
    icbdb_delete (nick, "signon");
    icbdb_delete (nick, "signoff");
    icbdb_delete (nick, "nick");
    icbdb_delete (nick, "password");
    icbdb_delete (nick, "phone");
    icbdb_delete (nick, "text");
    icbdb_delete (nick, "home");
    icbdb_delete (nick, "addr");
    icbdb_delete (nick, "nummsg");
    icbdb_delete (nick, "www");

    for (i = 1; i <= MAX_WRITES; i++) {
        char    key[80];

        sprintf (key, "header%d", i);
        icbdb_delete (nick, key);

        sprintf (key, "message%d", i);
        icbdb_delete (nick, key);

        sprintf (key, "from%d", i);
        icbdb_delete (nick, key);
    }

    sends_cmdout(forWhom, "Record Deleted");
    icbdb_close ();
    return 0;
}

int nickwritemsg(int forWhom, char *user, char *message, DBM *openDb)
{
    char           key[80];
    char           line[255], timebuf[255], msgfilterbuf[4096];
    int            count, i;
    char           *value;

    if ((strlen(user) == 0) || (strlen(message) ==0)) {
        sends_cmdout(forWhom, "Usage: write nickname message text");
        return -1;
    }

    if (strlen(u_tab[forWhom].realname) == 0) {
        senderror(forWhom, 
                  "You must be registered to write a message.");
        return -1;
    }

    icbdb_open ();

    memset(msgfilterbuf, 0, 4096);

    if (!icbdb_get (user, "nick", ICBDB_STRING, &value)) {
        sprintf(line, "%s is not registered", user);
        senderror(forWhom, line);
        icbdb_close ();
        return -1;
    }

    if (!icbdb_get (user, "nummsg", ICBDB_INT, &count)) {
        count = 0;
        i = 1;
    }
    else {
        i = 0;
    }

    count++;
    if (count > MAX_WRITES) {
        senderror(forWhom, "User mailbox full");
        icbdb_close ();
        return -1;
    }

    gettime();
    strftime(timebuf, 255, "%e-%h-%Y %H:%M %Z", localtime(&curtime));
    sprintf(line, "Message left at %s:", timebuf);

    filtertext(message, msgfilterbuf, 4096 - 1 );

    sprintf(key, "header%d", count);
    icbdb_set (user, key, ICBDB_STRING, line);

    sprintf(key, "from%d", count);
    icbdb_set (user, key, ICBDB_STRING, u_tab[forWhom].nickname);

    sprintf(key, "message%d", count);
    icbdb_set (user, key, ICBDB_STRING, msgfilterbuf);

    icbdb_set (user, "nummsg", ICBDB_INT, &count);

    sendstatus(forWhom, "Message", "Text saved to file");
    if ((i = find_user(user)) > 0) {
        sprintf(line, "%s is logged in now.", u_tab[i].nickname);
        sendstatus(forWhom, "Warning", line);
        sprintf(line, "You have %d message", count);
        if (count > 1) strcat(line, "s");
        sendstatus(i, "Message", line);
    }

    icbdb_close ();
    return 0;
}

int nickckmsg(int forWhom, DBM *openDb)
{
    char        *nick;
    int        retval;

    if (strlen(u_tab[forWhom].realname) == 0) {
        return -1;
    }

    nick = u_tab[forWhom].nickname;

    if (!icbdb_get (nick, "nummsg", ICBDB_INT, &retval)) {
        retval = 0;
        icbdb_set (nick, "nummsg", ICBDB_INT, &retval);
    }

    return(retval);
}

int nickreadmsg(int forWhom, DBM *openDb)
{
    char           key[64];
    char           from[MAX_NICKLEN+1];
    int            count, i;
    char        *nick = u_tab[forWhom].nickname;
    char        *value;

    if (strlen(u_tab[forWhom].realname) == 0) {
        senderror(forWhom, 
                  "You must be registered to read any messages.");
        return -1;
    }

    icbdb_open ();

    if (!icbdb_get (nick, "nummsg", ICBDB_INT, &count))
    {
        senderror(forWhom, "No messages");
    }
    else
    {
        if (count == 0)
        {
            senderror(forWhom, "No messages");
        }
        else for (i = 1; i <= count; i++) 
        {
            sprintf(key, "header%d", i);
            if (icbdb_get (nick, key, ICBDB_STRING, &value)) {
                sends_cmdout(forWhom, value);
            }
            icbdb_delete (nick, key);

            sprintf(key,"from%d", i);
            if (icbdb_get (nick, key, ICBDB_STRING, &value))
                strncpy(from, value, sizeof (from));
            else
                strcpy(from, "Server");
            icbdb_delete (nick, key);

            sprintf(key,"message%d", i);
            if (icbdb_get (nick, key, ICBDB_STRING, &value)) {
                send_person_stored(forWhom, from, value);
            }
            icbdb_delete (nick, key);
        }
    }

    count = 0;
    icbdb_set (nick, "nummsg", ICBDB_INT, &count);

    icbdb_close ();
    return 0;
}

int nickwritetime(int forWhom, int class, DBM *openDb)
{
    char           timebuf[255];
    char        *nick;
    char        *type;
    char        *value;

    if (strlen(u_tab[forWhom].realname) == 0) {
        return -1;  /* This shouldn't happen */
    }

    gettime();  /* update to the current time */
    strftime(timebuf, 255, "%e-%h-%Y %H:%M %Z", localtime(&curtime));

    nick = u_tab[forWhom].nickname;

    icbdb_open ();

    /* if class != 0, it's a signoff and we want to make
     * sure the nickname is still valid before storing the
     * signoff info in the db. otherwise people delete their
     * records and then when quitting this adds an orphaned db entry
     */
    if ( class != 0 )
    {
        /* no match for their .nick entry means it's not valid so return */
        if (!icbdb_get (nick, "nick", ICBDB_STRING, &value))
        {
            icbdb_close ();
            return -1;
        }
    }

    if (class == 0)
        type = "signon";
    else
        type = "signoff";

    icbdb_set (nick, type, ICBDB_STRING, timebuf);

    icbdb_close ();
    return 0;
}

int nickchinfo(int forWhom, const char *tag, char *data, unsigned int max, const char *message, DBM *openDb)
{
    char           line[255];
    char           newstr[255];

    if (strlen(u_tab[forWhom].realname) == 0) {
        senderror(forWhom, 
                  "Setting your name requites that you be registered.");
        return -1;
    }

    if (strlen(data) > max) {
        memset(newstr, 0, sizeof (newstr));
        strncpy(newstr, data, max);
        data = newstr;
        sprintf (line, "%s truncated to %u characters", message, max);
        senderror(forWhom, line);
    }

    icbdb_set (u_tab[forWhom].nickname, tag, ICBDB_STRING, data);
    sprintf (line, "%s set to '%s'", message, data);

    return 0;
}

int nickchpass(int forWhom, char *oldpw, char *newpw, DBM *openDb)
{
    char        line[255];
    char        *nick = u_tab[forWhom].nickname;
    char        *value;

    if ( strlen (oldpw) > MAX_PASSWDLEN )
        oldpw[MAX_PASSWDLEN] = '\0';

    if ( strlen (newpw) > MAX_PASSWDLEN )
        newpw[MAX_PASSWDLEN] = '\0';

    icbdb_open ();

    if (!icbdb_get (nick, "password", ICBDB_STRING, &value)) {
        /* This nick isn't registered */
        sprintf(line, "Authorization failure");
        senderror(forWhom, line);
    }
    else {
        if (strcmp(value, oldpw)) {
            sprintf(line, "Authorization failure");
            senderror(forWhom, line);
        }
        else {
            if (strlen (newpw) <= 0) {
                sprintf(line, "Missing paramater");
                senderror(forWhom,line);
            }
            else {
                char    addr[255];

                icbdb_set (nick, "password", ICBDB_STRING,
                           newpw);
                sprintf(line, "Password changed");
                sendstatus(forWhom,"Pass",line);

                snprintf (addr, sizeof (addr), "%s@%s",
                          u_tab[forWhom].loginid,
                          u_tab[forWhom].nodeid);

                icbdb_set (nick, "home", ICBDB_STRING, addr);
            }
        }
    }

    icbdb_close ();
    return 0;
}

/*
 * nickwrite() - register a nick, creating it's database entry if need be,
 * otherwise verifying the password on the existing record
 *
 * args:
 *  forWhom - index into the user table
 *  password - password for the nick
 *  verifyOnly - set to 1 to prevent nicks from being created. ie, return
 *    with a failure code if the nick isn't already in the db
 *
 * returns: 0 on success, -1 on failure
 */
int nickwrite(int forWhom, char *password, int verifyOnly, DBM *openDb)
{
    char        line[255];
    int        retval, i, j;
    char        *nick;
    char        *value;

    retval = -1;

    if (strlen(password) == 0) {
        senderror (forWhom, "Password cannot be null");
        return (retval);
    }

    if ( strlen(password) > MAX_PASSWDLEN )
        password[MAX_PASSWDLEN] = '\0';

    icbdb_open ();

    nick = u_tab[forWhom].nickname;

    if (!icbdb_get (nick, "password", ICBDB_STRING, &value))
    {
        if ( verifyOnly == 1 )
        {
            sprintf (line, "[ERROR] Nick %s not found",
                     u_tab[forWhom].nickname);
            senderror (forWhom, line);
        }
        else
        {
            char    addr[255];

            icbdb_set (nick, "password", ICBDB_STRING, password);
            icbdb_set (nick, "nick", ICBDB_STRING, nick);

            snprintf (addr, sizeof (addr), "%s@%s",
                      u_tab[forWhom].loginid,
                      u_tab[forWhom].nodeid);

            icbdb_set (nick, "home", ICBDB_STRING, addr);

            sendstatus(forWhom, "Register", "Nick registered");
            strcpy(u_tab[forWhom].realname, "registered");
            nickwritetime(forWhom, 0, NULL);
            strcpy(u_tab[forWhom].password, password); /* jonl */
            retval = 0;
        }
    }
    else
    {
        if (strcmp(value, password)) {
            sprintf(mbuf, "Authorization failure");
            senderror(forWhom, mbuf);
            memset(u_tab[forWhom].realname, 0, MAX_REALLEN + 1);
        }
        else
        {
            sendstatus(forWhom, "Register", "Nick registered");
            strcpy(u_tab[forWhom].realname, "registered");
            nickwritetime(forWhom, 0, NULL);
            strcpy(u_tab[forWhom].password, password); /* jonl */
            for (i = 1; i < MAX_GROUPS; i++)
                if ((g_tab[i].modtimeout > 0.0) &&
                    (strcmp(g_tab[i].missingmod, 
                            u_tab[forWhom].nickname)==0))
                {
                    g_tab[i].modtimeout = 0;
                    g_tab[i].mod = forWhom;
                    memset(g_tab[i].missingmod, 0, MAX_NICKLEN);
                    sprintf(mbuf, "%s is the active moderator again.",
                            u_tab[forWhom].nickname);
                    for (j = 1; j < MAX_REAL_USERS; j++)
                        if ((strcasecmp(u_tab[j].group, g_tab[i].name)
                             == 0) && (j != forWhom))
                            sendstatus(j, "Mod", mbuf);
                    sprintf(mbuf, "You are the moderator of group %s",
                            g_tab[i].name);
                    sendstatus(forWhom, "Mod", mbuf);
                }
            if ((i = nickckmsg(forWhom, NULL)) > 0) {
                if (i == 1)
                    sendstatus(forWhom, "Message", 
                               "You have 1 message");
                else {
                    sprintf(mbuf, "You have %d messages", i);
                    sendstatus(forWhom, "Message", mbuf);
                }
            }
            for ( i = 1; i < MAX_GROUPS; i++ )
                talk_report (forWhom, i);

            retval = 0;
        }
    }

    icbdb_close ();
    return (retval);
}

int nicklookup(int forWhom, const char *theNick, DBM *openDb)
{
    char           line[255];
    char           temp[255];
    char           nickstr[255];
    char           *value;
    int            retval;
    int            count = 0;
    char           *s, *p, *lastw;
    char           tmp1, tmp2;

    if (strlen(theNick) == 0)
    {
        if ((forWhom > 0) && (forWhom < MAX_REAL_USERS))
            sends_cmdout(forWhom, "Usage: whois nickname");
        return -1;
    }

    if (strlen(theNick) > MAX_NICKLEN)
    {
        senderror(forWhom, "nickname too long");
        return -1;
    }

    icbdb_open ();

    if (icbdb_get (theNick, "nick", ICBDB_STRING, &value))
    {
        strncpy(nickstr, value, sizeof (nickstr));
    }

    if (icbdb_get (theNick, "home", ICBDB_STRING, &value))
    {
        retval = 0;
        if (forWhom >= 0)
        {
            memset(line, 0, sizeof (line));
            sprintf(line, "Nickname:     %s", nickstr);
            while (strlen(line) < 36)
                strcat(line, " ");
            strcat(line, "Address:   ");
            strncat(line, value, sizeof(line)-1);
            sends_cmdout(forWhom, line);

            sprintf(line, "Phone Number: ");
            if (!icbdb_get (theNick, "phone", ICBDB_STRING, &value))
                strcat(line, "(None)");
            else
                strncat(line, value, sizeof(line)-1);

            while (strlen(line) < 36)
                strcat(line, " ");

            strcat(line, "Real Name: ");
            if (!icbdb_get (theNick, "realname", ICBDB_STRING, &value))
                strcat(line, "(None)");
            else
                strncat(line, value, sizeof(line)-1);

            sends_cmdout(forWhom, line);

            strcpy(line, "Last signon:  ");
            if (!icbdb_get (theNick, "signon", ICBDB_STRING, &value))
                strcat(line, "(unknown)");
            else
                strncat(line, value, sizeof(line)-1);

            while (strlen(line) < 36)
                strcat(line, " ");

            strcat(line, "Last signoff:  ");
            if (!icbdb_get (theNick, "signoff", ICBDB_STRING, &value))
                strcat(line, "(unknown)");
            else
                strncat(line, value, sizeof(line)-1);

            sends_cmdout(forWhom, line);

            if (icbdb_get (theNick, "email", ICBDB_STRING, &value))
            {
                strcpy(line, "E-mail addr:  ");
                strncat(line, value, sizeof(line)-1);
                sends_cmdout(forWhom, line);
            }

            if (icbdb_get (theNick, "www", ICBDB_STRING, &value))
            {
                strcpy(line, "WWW:  ");
                strncat(line, value, sizeof(line)-1);
                sends_cmdout(forWhom, line);
            }

            if (icbdb_get (theNick, "addr", ICBDB_STRING, &value))
            {
                strncpy(line, value, sizeof (line));
                sends_cmdout(forWhom, "Street Address:");
                s = line;
                strcpy(temp, "  ");
                while (strlen(s) > 0)
                {
                    if (*s == '|')
                    {
                        sends_cmdout(forWhom, temp);
                        strcpy(temp, "  ");
                    }
                    else
                        strncat(temp, s, 1);

                    s++;
                }
                sends_cmdout(forWhom, temp);
            }

            if (icbdb_get (theNick, "text", ICBDB_STRING, &value))
            {
                strncpy(line, value, sizeof (line));
                s = line;
                /* traverse s and try to break on a word */
                p = s;
                lastw = p;
                while (*p != '\0')
                {
                    if (*p == '\n')
                    {
                        *p++ = '\0';
                        strcpy(temp, "Text: ");
                        strcat(temp, s);
                        sends_cmdout(forWhom, temp);
                        count = 0;
                        lastw = p;
                        s = p;
                        continue;
                    }

                    /* remember location of last word break */
                    if (*p == ' ')
                        lastw = p;

                    /* break line if we are at max length */
                    if (count == (MAX_TEXTLEN - 1))
                    {
                        if ((p - lastw) > 40)
                        {
                            /* have to break in the middle of a word */
                            tmp1 = *(p - 1);
                            tmp2 = *p;
                            *(p - 1) = '-';
                            *p = '\0';
                            strcpy(temp, "Text: ");
                            strcat(temp, s);
                            sends_cmdout(forWhom, temp);
                            *(p - 1) = tmp1;
                            *p = tmp2;
                            p--;
                            s = p;
                        }
                        else
                        {
                            /* break at last space char */
                            tmp1 = *lastw;
                            *lastw = '\0';
                            strcpy(temp, "Text: ");
                            strcat(temp, s);
                            sends_cmdout(forWhom, temp);
                            *lastw = tmp1;
                            p = lastw + 1;
                            s = p;
                        }

                        lastw = p;
                        count = 0;
                    }
                    else
                    {
                        count++;
                        p++;
                    }
                }

                if (count > 0)
                    strcpy(temp, "Text: ");

                strcat(temp, s);
                sends_cmdout(forWhom, temp);
            }
        }
        else
        {
            /*
             * check their user@host with the user@host from
             * their database entry. if it doesn't match, not
             * auto-auth possible. if it does match, check the
             * setting of the .secure to see if they allow auto-auth
             */
            memset(temp, 0, 255);
            sprintf(temp, "%s@%s", u_tab[-forWhom].loginid,
                    u_tab[-forWhom].nodeid);
            if (strcasecmp(value, temp))
                retval = -2;
            else
            {
                if (icbdb_get (theNick, "secure", ICBDB_STRING, &value))
                    retval = -2;
            }
        }
    }
    else
    {
        if (forWhom >= 0)
        {
            sprintf(mbuf, "%s is not in the database.", theNick);
            senderror(forWhom, mbuf);
        }
        retval = -1;
    }

    icbdb_close ();
    return retval;
}

#pragma once

/* group table manipulation */

/* visibility */
#define VISIBLE 1
#define SECRET 2
#define SUPERSECRET 3

/* control */
#define PUBLIC 1
#define MODERATED 2
#define RESTRICTED 3
#define CONTROLLED 4

/* volume */
#define QUIET 1
#define NORMAL 2
#define LOUD 3

void clear_group_item(int n);

/* initialize the entire group table */
void init_groups(void);

/* find a slot in the group table with a particular name
 * return that index if found, -1 otherwise
 *
 * case insensitivoe
 *
 * name      groupname
 */


int find_group(char *name);


/* find an empty slot in the group table
 * return that index if found, -1 otherwise 
 */
int find_empty_group(void);


/* check the group table to see if user was mod of any
 * return that index if so, -1 otherwise
 *
 * case insensitive
 *
 * u_index    index in user table of this user
 */

int check_mods(int u_index);


/* fill a particular group entry
 * leave the invite list as it was 
 */
void fill_group_entry(int n, 
                      const char *name, 
                      const char *topic, 
                      int visibility, 
                      int control, 
                      int mod, 
                      int volume);

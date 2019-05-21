#define ICBDB_LIST_INDEX_MAX	-1

/*
 * This should be a nice big number; everything that uses
 * DB lists imposes its own, smaller, limit.
 */
typedef enum {
    ICBDB_INT,
    ICBDB_STRING,
} icbdb_type;

#define ICBDB_MODE 0600

int icbdb_open (void);
void icbdb_close (void);
int icbdb_get (const char *, const char *, icbdb_type, void *);
int icbdb_set (const char *, const char *, icbdb_type, const void *);
int icbdb_delete (const char *, const char *);
int icbdb_list_get_index (const char *, const char *, int, icbdb_type, void *);
int icbdb_list_set_index (const char *, const char *, int, icbdb_type, void *);
int icbdb_list_delete_index (const char *, const char *, int);
int icbdb_list_find (const char *, const char *, icbdb_type, void *, int *, int *, int *, int *);
int icbdb_list_add (const char *, const char *, icbdb_type, void *);
int icbdb_list_delete (const char *, const char *, icbdb_type, void *);
int icbdb_list_load (const char *, const char *, NAMLIST *);
int icbdb_list_save (const char *, const char *, NAMLIST *);
int icbdb_list_clear (const char *, const char *);

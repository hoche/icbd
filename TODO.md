## hoche's TODO List (updated 2/11/27):

### configuration
- ~~convert to CMake~~
- get rid of unused/obsolete #ifdefs, via ifnames
- allow specifying where gdbm/ndbm headers and lib are

### database
- rewrite icbdb api so instead of passing ints as void*'s, it passes the address of the item.
- ~~temporarily add capability of using gdbm~~
- ditch dbm and use sqlite as it's more ubiquitous these days

### socket server (murgil) rewrite and SSL support
Mostly done, renamed to pktserv.
- make cbufs be allocated as socket requests come in (use mempool?)
- add mempool for message buffers
- add queued writing
- add timed event queue for interrupting select()
- ~~test/fix TLS support~~ NOTE: This won't work on restarts.
- add configfile reader
- fix fd dump of sockets

### Features
- add in the ipv6 support written ages ago
- add in the brick support written ages ago
- ~~support utf-8~~


## jon's TODO List:
- Poke keithr until he gets me his server-side notify code modifications


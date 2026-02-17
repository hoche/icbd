#!/usr/bin/env python3

###
## dbm2icbdb.py
##
## Migrate data from a legacy dbm/ndbm/gdbm database to the new ICB
## internal .db format.
##
## Python's built-in `dbm` module can read ndbm, gdbm, and Berkeley DB
## files automatically. It also reads the old "dumb" dbm format.
##
## Usage:
##   python3 dbm2icbdb.py [options] <source_dbm_path>
##
## The source path should be given WITHOUT the file extension -- just as
## you would pass it to Perl's dbmopen() or C's dbm_open().  For example,
## if the files on disk are "icbdb.dir" and "icbdb.pag" (ndbm) or
## "icbdb.db" (gdbm), pass "icbdb" as the path.
##
## The output will be written to <dest>.db in the new ICB format.
###

import argparse
import dbm
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb


def migrate(src_path, dst_path, verbose=False):
    """Read all key/value pairs from a legacy dbm database at *src_path*
    and write them into a new ICB .db file at *dst_path*.db."""

    count = 0
    try:
        legacy_db = dbm.open(src_path, "r")
    except dbm.error as e:
        print(f"Error opening legacy database '{src_path}': {e}", file=sys.stderr)
        sys.exit(1)

    with IcbDb(dst_path) as new_db:
        key = legacy_db.firstkey()
        while key is not None:
            val = legacy_db[key]
            # Legacy databases store bytes; decode to str
            key_str = key.decode("utf-8", errors="replace")
            val_str = val.decode("utf-8", errors="replace")
            new_db[key_str] = val_str
            if verbose:
                print(f"  {key_str} = {val_str}")
            count += 1
            key = legacy_db.nextkey(key)

    legacy_db.close()
    return count


def main():
    parser = argparse.ArgumentParser(
        description="Migrate a legacy dbm/ndbm/gdbm database to ICB's internal .db format"
    )
    parser.add_argument("source",
                        help="Path to the legacy dbm database (without extension)")
    parser.add_argument("-o", "--output", default=None,
                        help="Output base path for the new .db file "
                             "(default: same as source)")
    parser.add_argument("-v", "--verbose", action="store_true",
                        help="Print each key=value pair as it is migrated")
    args = parser.parse_args()

    dst = args.output if args.output else args.source

    # Safety check: don't accidentally overwrite the destination if it
    # already exists in the new format.
    dst_file = dst + ".db"
    if os.path.exists(dst_file):
        print(f"Destination '{dst_file}' already exists.", file=sys.stderr)
        yn = input("Overwrite? [y/N] ").strip()
        if not yn or yn[0].lower() != "y":
            print("Aborted.", file=sys.stderr)
            sys.exit(1)
        os.unlink(dst_file)

    print(f"Migrating '{args.source}' -> '{dst_file}' ...")
    count = migrate(args.source, dst, verbose=args.verbose)
    print(f"Done. {count} entries migrated.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3

###
## setdb.py    falcon@icb.net
##
## assign a specific database value. useful for resetting passwords for users
## who forget them.
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} [-d database] var val", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]
    dbpath = "./icbdb"

    if len(args) >= 2 and args[0] == "-d":
        dbpath = args[1]
        args = args[2:]

    if len(args) < 2:
        print(f"Usage: {sys.argv[0]} [-d database] var val", file=sys.stderr)
        sys.exit(1)

    var, val = args[0], args[1]
    print(f"VAR[{var}] VAL[{val}]")

    with IcbDb(dbpath) as db:
        db[var] = val

if __name__ == "__main__":
    main()

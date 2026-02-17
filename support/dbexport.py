#!/usr/bin/env python3

###
## dbexport.py    falcon@icb.net
##
## this exports the database in a format that can be used by
## dbimport. it's not hammered on since it's only been used once.
## primary use is when migrating from one machine to another which
## has a different binary format for its dbm files so you can't just
## copy them over.
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def main():
    dbpath = sys.argv[1] if len(sys.argv) > 1 else "./icbdb"
    with IcbDb(dbpath) as db:
        for key, val in db.items():
            print(f"{key}|{val}")

if __name__ == "__main__":
    main()

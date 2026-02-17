#!/usr/bin/env python3

###
## countdb.py    falcon@icb.net
##
## count how many nicks there are
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def main():
    dbpath = sys.argv[1] if len(sys.argv) > 1 else "./icbdb"
    with IcbDb(dbpath) as db:
        num = sum(1 for key in db if key.endswith(".nick"))
    print(f"{num} nicknames listed.")

if __name__ == "__main__":
    main()

#!/usr/bin/env python3

###
## catdb.py    falcon@icb.net  2/1/98
##
## just cat the entire damn database to stdout
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def main():
    dbpath = sys.argv[1] if len(sys.argv) > 1 else "./icbdb"
    with IcbDb(dbpath) as db:
        for key, val in db.items():
            print(f"{key} = {val}")

if __name__ == "__main__":
    main()

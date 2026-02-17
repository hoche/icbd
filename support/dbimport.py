#!/usr/bin/env python3

###
## dbimport.py    falcon@icb.net
##
## this builds a database by importing data created by dbexport
## it's not hammered on since it's only been used once
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def main():
    dbpath = sys.argv[1] if len(sys.argv) > 1 else "./icbdb"
    with IcbDb(dbpath) as db:
        for line in sys.stdin:
            line = line.rstrip("\n")
            sep = line.find("|")
            if sep >= 0:
                key = line[:sep]
                val = line[sep + 1:]
                print(f"{key}={val}")
                db[key] = val

if __name__ == "__main__":
    main()

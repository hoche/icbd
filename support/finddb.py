#!/usr/bin/env python3

###
## finddb.py    falcon@icb.net    2/1/98
##
## find all related database fields for a nickname
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

SUFFIXES = [
    "addr",
    "email",
    "home",
    "nick",
    "nummsg",
    "password",
    "phone",
    "realname",
    "secure",
    "signoff",
    "signon",
    "text",
    "www",
]

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} [-d database] nick [nick ...]", file=sys.stderr)
        sys.exit(1)

    args = sys.argv[1:]
    dbpath = "./icbdb"

    # Simple option parsing for -d
    if len(args) >= 2 and args[0] == "-d":
        dbpath = args[1]
        args = args[2:]

    with IcbDb(dbpath) as db:
        for nick in args:
            k = f"{nick}.nick"
            if k not in db:
                print(f"{nick} not registered.")
                continue

            stored = db[k]
            if stored.lower() != nick.lower():
                print(f"Warning: Nick {nick} doesn't match $DB{{{k}}} ({stored}).")
                continue

            print(f"  {'NICKNAME:':>10s} {nick}")
            for s in SUFFIXES:
                k2 = f"{nick}.{s}"
                if k2 in db:
                    print(f"  {s + ':':>10s} {db[k2]}")

            num = int(db.get(f"{nick}.nummsg", "0") or "0")
            if num > 0:
                for i in range(1, num + 1):
                    h = f"{nick}.header{i}"
                    f = f"{nick}.from{i}"
                    m = f"{nick}.message{i}"
                    print(f"  {'message:':>10s} {i}")
                    print(f"  {'':>10s} {db.get(h, '')}")
                    print(f"  {'':>10s} {db.get(f, '')}")
                    print(f"  {'':>10s} {db.get(m, '')}")

            print()

if __name__ == "__main__":
    main()

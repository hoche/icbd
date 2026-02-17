#!/usr/bin/env python3

###
## deldb.py    falcon@icb.net    2/1/98
##
## delete nick entries from an icbd database
###

import argparse
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
    parser = argparse.ArgumentParser(description="Delete nick entries from an icbd database")
    parser.add_argument("-q", "--quiet", action="store_true", help="quiet mode")
    parser.add_argument("-d", "--database", default="./icbdb", help="database path (default: ./icbdb)")
    parser.add_argument("nicks", nargs="+", help="nicknames to delete")
    args = parser.parse_args()

    with IcbDb(args.database) as db:
        for nick in args.nicks:
            k = f"{nick}.nick"
            if db.get(k, "") == "":
                nick = nick.lower()
                k = f"{nick}.nick"
                if not args.quiet:
                    print(f"Trying with lower-cased nick {nick}")

            stored = db.get(k, "")
            if stored.lower() != nick.lower():
                print(f"Warning: Nick {nick} doesn't match $DB{{{k}}} ({stored}).")

            # Get message count before deleting fields
            num = int(db.get(f"{nick}.nummsg", "0") or "0")

            # Delete standard fields
            for s in SUFFIXES:
                k2 = f"{nick}.{s}"
                if k2 in db:
                    del db[k2]

            # Delete stored messages
            if num > 0:
                for i in range(1, num + 1):
                    for prefix in ("header", "from", "message"):
                        mk = f"{nick}.{prefix}{i}"
                        if mk in db:
                            del db[mk]

            if not args.quiet:
                print(f"{nick} deleted.")

if __name__ == "__main__":
    main()

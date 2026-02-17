#!/usr/bin/env python3

###
## nickprune.py    falcon@icb.net    11/23/98
##
## find (and perhaps delete) all database entries for nicks past
## a certain age
###

import argparse
import os
import re
import subprocess
import sys
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

MONTHS = {
    "Jan": 1, "Feb": 2, "Mar": 3, "Apr": 4,
    "May": 5, "Jun": 6, "Jul": 7, "Aug": 8,
    "Sep": 9, "Oct": 10, "Nov": 11, "Dec": 12,
}

# Default to one year
DEFAULT_DAYS = 365

DELETE_SCRIPT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "deldb.py")

SIGNOFF_RE = re.compile(r"\s*(\d+)-([A-Za-z]+)-(\d+)\s+(.*)")

def parse_signoff_date(datestr):
    """Parse a signoff/signon date like '6-Jan-1997 21:27 PST' and return
    a Unix timestamp, or None on failure."""
    m = SIGNOFF_RE.match(datestr)
    if not m:
        return None
    day = int(m.group(1))
    month_name = m.group(2)
    year = int(m.group(3))
    month = MONTHS.get(month_name)
    if month is None:
        return None
    try:
        dt = datetime(year, month, day)
        return dt.timestamp()
    except (ValueError, OverflowError):
        return None


def main():
    parser = argparse.ArgumentParser(
        description="Find (and perhaps delete) old nick entries from the database"
    )
    parser.add_argument("-y", "--yes", action="store_true",
                        help='default to "yes" for "Delete this nick?" queries')
    parser.add_argument("-d", "--days", type=int, default=DEFAULT_DAYS,
                        help=f"delete nicks older than this many days (default: {DEFAULT_DAYS})")
    parser.add_argument("-q", "--quiet", action="store_true",
                        help="quiet mode; don't display deleted nicks")
    parser.add_argument("--database", default="./icbdb",
                        help="database path (default: ./icbdb)")
    args = parser.parse_args()

    age_seconds = 60 * 60 * 24 * args.days

    if not args.quiet:
        print(f"Running with {args.days} days expiry.")

    now = time.time()

    with IcbDb(args.database) as db:
        # Collect nicks first to avoid modifying dict during iteration
        nicks_to_check = []
        for key in db:
            if key.endswith(".nick"):
                nicks_to_check.append(key)

        for nick_key in nicks_to_check:
            nick_val = db.get(nick_key, "")
            if nick_val in ("server", "admin"):
                continue

            rootkey = nick_key[:-5]  # strip ".nick"

            # Try signoff first, then signon
            so_key = f"{rootkey}.signoff"
            datestr = db.get(so_key, "")
            if not datestr:
                so_key = f"{rootkey}.signon"
                datestr = db.get(so_key, "")

            if not datestr:
                continue

            then = parse_signoff_date(datestr)
            if then is None:
                continue

            if now - then > age_seconds:
                if not args.quiet:
                    age_days = (now - then) / (60 * 60 * 24)
                    print(f"{nick_val} is {age_days:.0f} days old ({datestr})")

                if not args.yes:
                    try:
                        yn = input(f"Delete {rootkey}? [n] ").strip()
                    except EOFError:
                        break
                    if not yn:
                        yn = "no"
                    if yn[0].lower() == "q":
                        break
                    if yn[0].lower() != "y":
                        continue

                if not args.quiet:
                    print("  deleting....", end="")

                cmd = [sys.executable, DELETE_SCRIPT, "--database", args.database]
                if args.quiet:
                    cmd.append("-q")
                cmd.append("--")
                cmd.append(rootkey)

                subprocess.run(cmd)

                if not args.quiet:
                    print()

if __name__ == "__main__":
    main()

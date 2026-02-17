#!/usr/bin/env python3

###
## mkdb.py    falcon@icb.net    2/1/98
##
## initialize the user database with the required "server" nickname
###

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from icbdb import IcbDb

def prompt_user(prompt, default=None):
    if default:
        s = input(f"{prompt} [{default}]: ")
    else:
        s = input(f"{prompt}: ")
    if not s and default:
        return default
    return s

def main():
    dbpath = sys.argv[1] if len(sys.argv) > 1 else "./icbdb"

    realname = prompt_user("The server admin's name", "Server Admin")
    home     = prompt_user("The server address", "icb@yourdomain.com")
    email    = prompt_user("The server admin's email", "icbadmin@yourdomain.com")

    with IcbDb(dbpath) as db:
        db["server.realname"] = realname
        db["server.nick"]     = "server"
        db["server.home"]     = home
        db["server.email"]    = email
        db["server.text"]     = "Here to server you!"
        db["server.nummsg"]   = "0"
        db["server.www"]      = "http://www.icb.net/"

if __name__ == "__main__":
    main()

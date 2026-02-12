#!/usr/bin/env python3
"""
Integration tests for hush and notify:
  /hush (suppress messages), toggle off, /notify (signon notifications)
"""

import argparse
import time
from pathlib import Path

from icb import ICBClient, Packet, login_and_sync, with_server


def run(enable_tls: bool) -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--icbd", required=True)
    ap.add_argument("--fixtures", required=True)
    ap.add_argument("--io-timeout-s", type=float, default=3.0)
    ap.add_argument("--tls", action="store_true")
    args = ap.parse_args()

    icbd_path = Path(args.icbd)
    fixtures_dir = Path(args.fixtures)
    T = args.io_timeout_s

    server, clear_port, ssl_port = with_server(icbd_path, fixtures_dir, enable_tls=enable_tls)
    try:
        port = ssl_port if enable_tls else clear_port
        assert port is not None

        alice = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        bob = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        try:
            login_and_sync(alice, loginid="idA", nick="alice", group="1", io_timeout_s=T)
            login_and_sync(bob, loginid="idB", nick="bob", group="1", io_timeout_s=T)

            # Move to a fresh group
            alice.send_cmd("g", "HSH")
            bob.send_cmd("g", "HSH")
            time.sleep(0.3)
            alice.drain_for(0.3)
            bob.drain_for(0.3)

            # --- Test 1: /hush suppresses open messages ---
            bob.send_cmd("hush", "alice")
            time.sleep(0.1)
            bob.drain_for(0.2)

            # Alice sends an open message
            alice.send_open("can you hear me?")
            # Bob should NOT receive it
            pkts = bob.drain_for(0.5)
            for p in pkts:
                if p.ptype == "b":
                    f = p.fields()
                    if len(f) >= 2 and f[0] == b"alice":
                        raise AssertionError("bob should not receive open message from hushed alice")

            # --- Test 2: /hush also suppresses private messages ---
            alice.send_cmd("m", "bob secret message")
            # Alice should get a "Bounce" status indicating message didn't go through
            pkts = alice.drain_for(0.5)
            got_bounce = any(
                p.ptype == "d" and len(p.fields()) >= 2 and p.fields()[0] == b"Bounce"
                for p in pkts
            )
            if not got_bounce:
                raise AssertionError("alice should get Bounce for hushed message")

            # Verify bob didn't get the private message
            pkts = bob.drain_for(0.3)
            for p in pkts:
                if p.ptype == "c":
                    f = p.fields()
                    if len(f) >= 2 and f[0] == b"alice":
                        raise AssertionError("bob should not receive personal message from hushed alice")

            # --- Test 3: Calling /hush again on the same nick toggles it off ---
            bob.send_cmd("hush", "alice")
            time.sleep(0.1)
            bob.drain_for(0.3)

            alice.send_open("now can you hear me?")
            pkts = bob.drain_for(0.5)
            got_open = any(
                p.ptype == "b" and len(p.fields()) >= 2 and p.fields()[0] == b"alice"
                and b"now can you hear me" in p.fields()[1]
                for p in pkts
            )
            if not got_open:
                raise AssertionError("bob should receive open message after hush toggled off")

            # --- Test 4: /notify ---
            alice.send_cmd("notify", "carol")
            time.sleep(0.1)
            alice.drain_for(0.2)

            # Now carol logs in - alice should get a Notify-On notification
            carol = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
            try:
                login_and_sync(carol, loginid="idC", nick="carol", group="1", io_timeout_s=T)
                time.sleep(0.3)

                pkts = alice.drain_for(0.5)
                got_notify = any(
                    p.ptype == "d"
                    and len(p.fields()) >= 2
                    and p.fields()[0] == b"Notify-On"
                    and b"carol" in p.fields()[1]
                    for p in pkts
                )
                if not got_notify:
                    raise AssertionError(
                        f"alice should get Notify-On for carol; saw: {[(p.ptype, p.fields()) for p in pkts]}"
                    )
            finally:
                carol.close()

        finally:
            alice.close()
            bob.close()
    finally:
        server.stop()


def main() -> int:
    enable_tls = "--tls" in __import__("sys").argv
    run(enable_tls=enable_tls)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

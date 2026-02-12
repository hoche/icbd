#!/usr/bin/env python3
"""
Integration tests for away messages:
  /away, /noaway, away message delivery
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
            alice.send_cmd("g", "AWY")
            bob.send_cmd("g", "AWY")
            time.sleep(0.3)
            alice.drain_for(0.3)
            bob.drain_for(0.3)

            # --- Test 1: Set away message and verify delivery ---
            # NOTE: The away message MUST contain exactly one %s (for the username).
            bob.send_cmd("away", "%s is gone fishing")
            time.sleep(0.2)
            pkts = bob.drain_for(0.3)
            # Verify the away was set
            got_away_set = any(
                p.ptype == "d" and len(p.fields()) >= 2 and p.fields()[0] == b"Away"
                for p in pkts
            )
            if not got_away_set:
                raise AssertionError(f"away message not confirmed as set; saw: {[(p.ptype, p.fields()) for p in pkts]}")

            # Alice sends bob a private message
            alice.send_cmd("m", "bob hello")
            pkts = alice.drain_for(1.0)

            # Alice should receive an Away status with bob's away message
            got_away = any(
                p.ptype == "d"
                and len(p.fields()) >= 2
                and p.fields()[0] == b"Away"
                and b"gone fishing" in p.fields()[1]
                for p in pkts
            )
            if not got_away:
                raise AssertionError(f"alice should receive an Away status; saw: {[(p.ptype, p.fields()) for p in pkts]}")

            # --- Test 2: Clear away with /noaway ---
            bob.send_cmd("noaway", "")
            time.sleep(0.2)
            bob.drain_for(0.3)

            # Alice sends another message - should NOT get away reply
            alice.send_cmd("m", "bob hello again")
            pkts = alice.drain_for(0.5)
            for p in pkts:
                if p.ptype == "d":
                    f = p.fields()
                    if len(f) >= 2 and f[0] == b"Away" and b"gone fishing" in f[1]:
                        raise AssertionError("should not get away message after /noaway")

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

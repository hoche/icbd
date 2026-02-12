#!/usr/bin/env python3
"""
Integration tests for:
  /echoback (on/off), /beep, /nobeep, /name (nick change)
"""

import argparse
import time
from pathlib import Path

from icb import ICBClient, Packet, login_and_sync, with_server


def run(enable_tls: bool) -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--icbd", required=True)
    ap.add_argument("--fixtures", required=True)
    ap.add_argument("--io-timeout-s", type=float, default=2.0)
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
            alice.send_cmd("g", "EBN")
            bob.send_cmd("g", "EBN")
            time.sleep(0.15)
            alice.drain_for(0.2)
            bob.drain_for(0.2)

            # --- Test 1: Echoback off (default) - sender does NOT see own open ---
            alice.send_open("no echo")
            pkts = alice.drain_for(0.4)
            for p in pkts:
                if p.ptype == "b":
                    f = p.fields()
                    if len(f) >= 2 and f[0] == b"alice" and b"no echo" in f[1]:
                        raise AssertionError("alice should NOT see her own message with echoback off")

            # --- Test 2: Echoback on - sender DOES see own open ---
            alice.send_cmd("echoback", "on")
            alice.drain_for(0.2)

            alice.send_open("with echo")
            def alice_got_echo(p: Packet) -> bool:
                if p.ptype != "b":
                    return False
                f = p.fields()
                return len(f) >= 2 and f[0] == b"alice" and b"with echo" in f[1]
            alice.wait_for(alice_got_echo, timeout_s=T)

            # Turn it back off
            alice.send_cmd("echoback", "off")
            alice.drain_for(0.2)

            # --- Test 3: /beep ---
            alice.send_cmd("beep", "bob")
            def bob_got_beep(p: Packet) -> bool:
                # Beep packet type is 'k'
                return p.ptype == "k"
            bob.wait_for(bob_got_beep, timeout_s=T)

            # --- Test 4: /nobeep ---
            bob.send_cmd("nobeep", "on")
            bob.drain_for(0.2)

            alice.send_cmd("beep", "bob")
            # Alice should get an error about nobeep
            def alice_got_nobeep_error(p: Packet) -> bool:
                return p.ptype == "e" and b"nobeep" in p.body()
            alice.wait_for(alice_got_nobeep_error, timeout_s=T)

            # Turn nobeep off again
            bob.send_cmd("nobeep", "off")
            bob.drain_for(0.2)

            # --- Test 5: /name change ---
            alice.send_cmd("name", "alicia")

            # Bob should see a nick-change status
            def bob_sees_name_change(p: Packet) -> bool:
                if p.ptype != "d":
                    return False
                f = p.fields()
                return (len(f) >= 2
                        and f[0] == b"Name"
                        and b"alice" in f[1]
                        and b"alicia" in f[1])
            bob.wait_for(bob_sees_name_change, timeout_s=T)

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

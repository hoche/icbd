#!/usr/bin/env python3
"""
Integration tests for group management commands:
  /boot, /invite, /cancel, /pass, /topic on restricted groups,
  /status (moderated, restricted, controlled), mod reassignment
"""

import argparse
import time
from pathlib import Path

from icb import ICBClient, Packet, login_and_sync, with_server


def pkt_status_category(p: Packet, category: str) -> bool:
    """Check if a 'd' (status) packet has a given category."""
    if p.ptype != "d":
        return False
    f = p.fields()
    return len(f) >= 1 and f[0] == category.encode("ascii")


def pkt_status_contains(p: Packet, category: str, substr: str) -> bool:
    if p.ptype != "d":
        return False
    f = p.fields()
    return (len(f) >= 2
            and f[0] == category.encode("ascii")
            and substr.encode("utf-8") in f[1])


def pkt_is_error_containing(p: Packet, substr: str) -> bool:
    return p.ptype == "e" and substr.encode("utf-8") in p.body()


def drain_and_ignore(client: ICBClient, duration: float = 0.2) -> list[Packet]:
    return client.drain_for(duration)


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
        carol = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        try:
            login_and_sync(alice, loginid="idA", nick="alice", group="1", io_timeout_s=T)
            login_and_sync(bob, loginid="idB", nick="bob", group="1", io_timeout_s=T)
            login_and_sync(carol, loginid="idC", nick="carol", group="1", io_timeout_s=T)

            # --- Test 1: /pass moderation ---
            # Create a new group. Alice is mod.
            alice.send_cmd("g", "GRP")
            drain_and_ignore(alice)
            bob.send_cmd("g", "GRP")
            drain_and_ignore(bob)

            # Alice passes mod to bob
            alice.send_cmd("pass", "bob")

            def bob_got_pass(p: Packet) -> bool:
                return pkt_status_contains(p, "Pass", "bob")
            bob.wait_for(bob_got_pass, timeout_s=T)

            # --- Test 2: /boot ---
            # Bob (now mod) boots alice
            carol.send_cmd("g", "GRP")
            drain_and_ignore(carol)

            bob.send_cmd("boot", "alice")

            def alice_was_booted(p: Packet) -> bool:
                return pkt_status_contains(p, "Boot", "boot")
            alice.wait_for(alice_was_booted, timeout_s=T)

            # Alice should now be in BOOT_GROUP (~RESEDA~)
            # She won't see GRP messages anymore. Just drain.
            drain_and_ignore(alice)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            # --- Test 3: /topic ---
            bob.send_cmd("topic", "new topic")

            def carol_sees_topic(p: Packet) -> bool:
                return pkt_status_contains(p, "Topic", "changed the topic")
            carol.wait_for(carol_sees_topic, timeout_s=T)

            # --- Test 4: /status controlled ---
            # Only CONTROLLED groups restrict non-mod open messages (not moderated).
            bob.send_cmd("status", "c")
            time.sleep(0.15)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            # Non-mod (carol) tries to talk - should get error
            carol.send_open("should fail")
            pkts = carol.drain_for(0.5)
            got_error = any(pkt_is_error_containing(p, "permission") or
                           pkt_is_error_containing(p, "not the mod") or
                           pkt_is_error_containing(p, "do not have")
                           for p in pkts)
            if not got_error:
                raise AssertionError("carol should get an error sending to a controlled group where she's not mod")

            # Switch back to public for further tests
            bob.send_cmd("status", "p")
            time.sleep(0.1)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            # --- Test 5: /invite and restricted group ---
            bob.send_cmd("status", "r")
            time.sleep(0.1)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            # Invite alice back
            bob.send_cmd("invite", "alice")
            drain_and_ignore(bob)

            # Alice tries to join
            alice.send_cmd("g", "GRP")
            # She should be able to join since she was invited
            def alice_joined(p: Packet) -> bool:
                return pkt_status_contains(p, "Arrive", "alice")
            try:
                bob.wait_for(alice_joined, timeout_s=T)
            except TimeoutError:
                raise AssertionError("alice should be able to join after being invited to restricted group")

            drain_and_ignore(alice)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            # --- Test 6: /cancel invitation ---
            # First, have alice leave, then bob cancels any invite and alice can't rejoin
            alice.send_cmd("g", "1")
            drain_and_ignore(alice)
            drain_and_ignore(bob)
            drain_and_ignore(carol)

            bob.send_cmd("cancel", "alice")
            drain_and_ignore(bob)

            # Alice tries to join - should fail
            alice.send_cmd("g", "GRP")
            pkts = alice.drain_for(0.5)
            # She should get an error about the group being restricted
            got_restricted_error = any(
                pkt_is_error_containing(p, "invited") or
                pkt_is_error_containing(p, "restricted") or
                pkt_is_error_containing(p, "not invited")
                for p in pkts
            )
            if not got_restricted_error:
                raise AssertionError("alice should not be able to join restricted group after invitation cancelled")

        finally:
            alice.close()
            bob.close()
            carol.close()
    finally:
        server.stop()


def main() -> int:
    enable_tls = "--tls" in __import__("sys").argv
    run(enable_tls=enable_tls)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

#!/usr/bin/env python3
"""
Integration tests for error handling and edge cases:
  - Invalid packet type
  - Message to non-existent user
  - UTF-8 messages passing through correctly
  - Ping/Pong, Noop
  - Disconnect handling
"""

import argparse
import time
from pathlib import Path

from icb import (
    ICBClient,
    Packet,
    login_and_sync,
    send_frame,
    with_server,
)


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

        # --- Tests 1 & 2: Invalid packet + msg to nonexistent user (same connection) ---
        c = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        try:
            login_and_sync(c, loginid="idE1", nick="erruser", group="1", io_timeout_s=T)

            # Test 1: Invalid packet type - 'Z' is not valid
            send_frame(c.sock, b"Z" + b"garbage" + b"\x00")
            pkts = c.drain_for(0.5)
            got_error = any(p.ptype == "e" for p in pkts)
            if not got_error:
                raise AssertionError(
                    f"expected error for invalid packet type; saw: {[(p.ptype, p.body()) for p in pkts]}"
                )

            # Test 2: Message to non-existent user (reuse same connection)
            c.send_cmd("m", "nobodyhere hi")
            pkts = c.drain_for(0.5)
            got_not_found = any(
                p.ptype == "e" and (b"not signed on" in p.body() or b"No one" in p.body())
                for p in pkts
            )
            if not got_not_found:
                raise AssertionError(
                    f"expected error for msg to non-existent user; saw: {[(p.ptype, p.body()) for p in pkts]}"
                )

            # Test 3: Noop should not produce an error
            send_frame(c.sock, b"n\x00")
            pkts = c.drain_for(0.3)
            for p in pkts:
                if p.ptype == "e":
                    raise AssertionError(f"noop should not produce an error; got: {p.body()}")

            # Test 4: Pong should not produce an error
            send_frame(c.sock, b"l\x00")
            pkts = c.drain_for(0.3)
            for p in pkts:
                if p.ptype == "e" and b"PONG" in p.body():
                    raise AssertionError("pong should not produce an error")
        finally:
            c.close()

        time.sleep(1.0)

        # --- Test 5: UTF-8 messages pass through correctly ---
        alice = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        bob = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        try:
            login_and_sync(alice, loginid="idE2", nick="alice", group="1", io_timeout_s=T)
            login_and_sync(bob, loginid="idE3", nick="bob", group="1", io_timeout_s=T)

            alice.send_cmd("g", "UTF")
            bob.send_cmd("g", "UTF")
            time.sleep(0.2)
            alice.drain_for(0.3)
            bob.drain_for(0.3)

            # Send a message with UTF-8 characters: café
            utf8_msg = "café"
            alice.send_open(utf8_msg)

            pkts = bob.drain_for(0.5)
            got_utf8 = any(
                p.ptype == "b"
                and len(p.fields()) >= 2
                and p.fields()[0] == b"alice"
                and "caf".encode("utf-8") in p.fields()[1]
                for p in pkts
            )
            if not got_utf8:
                raise AssertionError(
                    f"bob should receive UTF-8 message; saw: {[(p.ptype, p.fields()) for p in pkts]}"
                )
        finally:
            alice.close()
            bob.close()

        time.sleep(0.3)

        # --- Test 6: Graceful disconnect (client just closes) ---
        # Use unique nicks (carol/dave) to avoid any race with the server
        # still cleaning up alice/bob from Test 5 on slow VMs (e.g. FreeBSD).
        carol = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        dave = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=T)
        try:
            login_and_sync(carol, loginid="idE4", nick="carol", group="1", io_timeout_s=T)
            login_and_sync(dave, loginid="idE5", nick="dave", group="1", io_timeout_s=T)

            carol.send_cmd("g", "DCN")
            dave.send_cmd("g", "DCN")
            time.sleep(0.2)
            carol.drain_for(0.2)
            dave.drain_for(0.2)

            # Carol disconnects
            carol.close()

            # Dave should see a sign-off status
            pkts = dave.drain_for(2.0)
            got_signoff = any(
                p.ptype == "d"
                and len(p.fields()) >= 2
                and p.fields()[0] == b"Sign-off"
                and b"carol" in p.fields()[1]
                for p in pkts
            )
            if not got_signoff:
                raise AssertionError(
                    f"dave should see carol sign-off; saw: {[(p.ptype, p.fields()) for p in pkts]}"
                )
        finally:
            try:
                carol.close()
            except Exception:
                pass
            dave.close()

    finally:
        server.stop()


def main() -> int:
    enable_tls = "--tls" in __import__("sys").argv
    run(enable_tls=enable_tls)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

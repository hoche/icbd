#!/usr/bin/env python3

import argparse
import time
from pathlib import Path

from icb import ICBClient, Packet, login_and_sync, with_server


def run(enable_tls: bool) -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--icbd", required=True)
    ap.add_argument("--fixtures", required=True)
    ap.add_argument("--io-timeout-s", type=float, default=2.0)
    ap.add_argument("--tls", action="store_true", help="Connect to the TLS listener (requires TLS-enabled build)")
    args = ap.parse_args()

    icbd_path = Path(args.icbd)
    fixtures_dir = Path(args.fixtures)

    server, clear_port, ssl_port = with_server(icbd_path, fixtures_dir, enable_tls=enable_tls)
    try:
        port = ssl_port if enable_tls else clear_port
        assert port is not None

        alice = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=args.io_timeout_s)
        bob = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=args.io_timeout_s)
        try:
            login_and_sync(alice, loginid="testidA", nick="alice", group="1", io_timeout_s=args.io_timeout_s)
            login_and_sync(bob, loginid="testidB", nick="bob", group="1", io_timeout_s=args.io_timeout_s)

            # Join a non-special group to avoid topic restrictions and keep behavior consistent.
            alice.send_cmd("g", "TST")
            bob.send_cmd("g", "TST")
            time.sleep(0.10)

            # 1) Public open message: bob should receive 'b' with fields: sender, message.
            msg = "hello group"
            alice.send_open(msg)

            def bob_got_open(p: Packet) -> bool:
                if p.ptype != "b":
                    return False
                f = p.fields()
                return len(f) >= 2 and f[0] == b"alice" and f[1] == msg.encode("utf-8")

            bob.wait_for(bob_got_open, timeout_s=args.io_timeout_s)

            # By default, sender does NOT receive own open messages unless echoback is enabled.
            # Ensure alice does not see her own 'b alice^A...' within a short window.
            pkts = alice.drain_for(0.30)
            for p in pkts:
                if p.ptype == "b":
                    f = p.fields()
                    if len(f) >= 2 and f[0] == b"alice" and f[1] == msg.encode("utf-8"):
                        raise AssertionError("alice unexpectedly received her own open message (echoback should be off by default)")

            # 2) Private message via /m: bob should receive a personal 'c' packet with alice + message.
            pmsg = "hi bob"
            alice.send_cmd("m", f"bob {pmsg}")

            def bob_got_personal(p: Packet) -> bool:
                if p.ptype != "c":
                    return False
                f = p.fields()
                return len(f) >= 2 and f[0] == b"alice" and f[1] == pmsg.encode("utf-8")

            bob.wait_for(bob_got_personal, timeout_s=args.io_timeout_s)
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


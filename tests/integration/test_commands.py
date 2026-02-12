#!/usr/bin/env python3

import argparse
import time
from pathlib import Path

from icb import ICBClient, Packet, login_and_sync, with_server


def pkt_is_cmdout_co_contains(substr: str):
    needle = substr.encode("utf-8")

    def _pred(p: Packet) -> bool:
        if p.ptype != "i":
            return False
        f = p.fields()
        if not f:
            return False
        if f[0] != b"co":
            return False
        return needle in p.payload_no_nul()

    return _pred


def any_wl_nick(pkts: list[Packet], nick: str) -> bool:
    n = nick.encode("ascii")
    for p in pkts:
        if p.ptype != "i":
            continue
        f = p.fields()
        if len(f) >= 3 and f[0] == b"wl" and f[2] == n:
            return True
    return False


def any_wl_nick_has_ssl(pkts: list[Packet], nick: str) -> bool:
    n = nick.encode("ascii")
    for p in pkts:
        if p.ptype != "i":
            continue
        f = p.fields()
        # iwl^AMod^ANick^AIdle^AResp^ALoginTime^AUserID^AHostID^ARegisterInfo
        if len(f) >= 9 and f[0] == b"wl" and f[2] == n:
            return b"ssl" in f[8]
    return False


def run(enable_tls: bool) -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--icbd", required=True)
    ap.add_argument("--fixtures", required=True)
    ap.add_argument("--io-timeout-s", type=float, default=2.0)
    args = ap.parse_args()

    icbd_path = Path(args.icbd)
    fixtures_dir = Path(args.fixtures)

    server, clear_port, ssl_port = with_server(icbd_path, fixtures_dir, enable_tls=enable_tls)
    try:
        port = ssl_port if enable_tls else clear_port
        assert port is not None

        a = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=args.io_timeout_s)
        b = ICBClient.connect("127.0.0.1", port, use_tls=enable_tls, timeout_s=args.io_timeout_s)
        try:
            login_and_sync(a, loginid="testidA", nick="alice", group="1", io_timeout_s=args.io_timeout_s)
            login_and_sync(b, loginid="testidB", nick="bob", group="1", io_timeout_s=args.io_timeout_s)

            # Make a fresh group where we can change topic/status.
            a.send_cmd("g", "TST")
            b.send_cmd("g", "TST")
            time.sleep(0.10)

            # 1) /w . should include a group header for TST and user list with both nicks.
            a.send_cmd("w", ".")
            pkts = a.drain_for(0.50)

            # Group header line (cmdout/co) includes "Group: ..."
            if not any(pkt_is_cmdout_co_contains("Group:")(p) for p in pkts):
                raise AssertionError("expected at least one command output line containing 'Group:' after /w .")

            # Ensure the group name appears in output (visible groups show the real name).
            if not any(pkt_is_cmdout_co_contains("Group: TST")(p) for p in pkts):
                raise AssertionError("expected /w output to contain 'Group: TST'")

            # For groups with >=2 users, s_who emits wl entries.
            if not any_wl_nick(pkts, "alice"):
                raise AssertionError("expected a wl entry for alice in /w . output")
            if not any_wl_nick(pkts, "bob"):
                raise AssertionError("expected a wl entry for bob in /w . output")

            # In TLS mode, wl status should include 'ssl' marker; in clear mode it shouldn't.
            if enable_tls:
                if not any_wl_nick_has_ssl(pkts, "alice"):
                    raise AssertionError("expected alice wl status to include 'ssl' in TLS mode")
                if not any_wl_nick_has_ssl(pkts, "bob"):
                    raise AssertionError("expected bob wl status to include 'ssl' in TLS mode")
            else:
                if any_wl_nick_has_ssl(pkts, "alice") or any_wl_nick_has_ssl(pkts, "bob"):
                    raise AssertionError("did not expect wl status to include 'ssl' in clear mode")

            # 2) topic change should broadcast a Topic status into the group (bob should see it).
            a.send_cmd("topic", "hello")

            def is_topic_status(p: Packet) -> bool:
                if p.ptype != "d":
                    return False
                f = p.fields()
                return len(f) >= 2 and f[0] == b"Topic" and b"changed the topic" in f[1]

            b.wait_for(is_topic_status, timeout_s=args.io_timeout_s)

            # 3) status change should affect group header flags. Switch to moderated ('m') and re-run /w .
            a.send_cmd("status", "m")
            time.sleep(0.10)
            a.send_cmd("w", ".")
            pkts2 = a.drain_for(0.60)
            if not any(pkt_is_cmdout_co_contains("Group: TST (m")(p) for p in pkts2):
                raise AssertionError("expected group header to show moderated control '(m' after 'status m'")
        finally:
            a.close()
            b.close()
    finally:
        server.stop()


def main() -> int:
    # CTest passes --tls as present/absent via args; keep it simple:
    enable_tls = "--tls" in __import__("sys").argv
    run(enable_tls=enable_tls)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


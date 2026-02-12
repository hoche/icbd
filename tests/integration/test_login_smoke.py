#!/usr/bin/env python3

import argparse
from pathlib import Path

def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--icbd", required=True, help="Path to icbd server binary")
    ap.add_argument("--fixtures", required=True, help="Path to prod/ fixture directory")
    ap.add_argument("--startup-timeout-s", type=float, default=2.5)
    ap.add_argument("--io-timeout-s", type=float, default=1.5)
    args = ap.parse_args()

    icbd_path = Path(args.icbd)
    fixtures_dir = Path(args.fixtures)
    if not icbd_path.exists():
        raise SystemExit(f"--icbd does not exist: {icbd_path}")
    if not fixtures_dir.exists():
        raise SystemExit(f"--fixtures does not exist: {fixtures_dir}")

    from icb import ICBClient, login_and_sync, with_server

    server, clear_port, _ssl_port = with_server(icbd_path, fixtures_dir, enable_tls=False, startup_timeout_s=args.startup_timeout_s)
    try:
        c = ICBClient.connect("127.0.0.1", clear_port, use_tls=False, timeout_s=args.io_timeout_s)
        try:
            login_and_sync(c, loginid="testid", nick="testnick", group="1", io_timeout_s=args.io_timeout_s)
        except Exception:
            server.dump_diagnostics("login_smoke")
            raise
        finally:
            c.close()
    finally:
        server.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())


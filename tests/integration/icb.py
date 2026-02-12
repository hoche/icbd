#!/usr/bin/env python3

from __future__ import annotations

import dataclasses
import os
import shutil
import socket
import ssl
import subprocess
import tempfile
import time
from pathlib import Path
from typing import Callable, Optional


ICB_SEP = b"\x01"


def find_free_port() -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind(("127.0.0.1", 0))
        return s.getsockname()[1]


def has_ipv6() -> bool:
    """Return True if the system supports IPv6 loopback (::1)."""
    if not socket.has_ipv6:
        return False
    try:
        with socket.socket(socket.AF_INET6, socket.SOCK_STREAM) as s:
            s.bind(("::1", 0))
        return True
    except OSError:
        return False


def _recv_exact(sock: socket.socket, n: int, timeout_s: float) -> bytes:
    sock.settimeout(timeout_s)
    out = bytearray()
    while len(out) < n:
        chunk = sock.recv(n - len(out))
        if not chunk:
            raise RuntimeError("socket closed while reading")
        out += chunk
    return bytes(out)


def recv_frame(sock: socket.socket, timeout_s: float) -> bytes:
    length_b = _recv_exact(sock, 1, timeout_s)
    length = length_b[0]
    payload = _recv_exact(sock, length, timeout_s)
    return payload


def send_frame(sock: socket.socket, payload: bytes) -> None:
    if len(payload) > 255:
        raise ValueError("payload too large for 1-byte length framing")
    sock.sendall(bytes([len(payload)]) + payload)


def _strip_trailing_nul(payload: bytes) -> bytes:
    return payload[:-1] if payload.endswith(b"\x00") else payload


@dataclasses.dataclass(frozen=True)
class Packet:
    ptype: str
    payload: bytes  # includes type byte and (maybe) trailing NUL

    def payload_no_nul(self) -> bytes:
        return _strip_trailing_nul(self.payload)

    def body(self) -> bytes:
        """Payload without the leading type byte, and without trailing NUL."""
        p = self.payload_no_nul()
        return p[1:] if len(p) >= 1 else b""

    def fields(self) -> list[bytes]:
        """Split body on ^A for packets that use field separation."""
        b = self.body()
        if b == b"":
            return []
        return b.split(ICB_SEP)


class ICBClient:
    def __init__(self, sock: socket.socket):
        self.sock = sock

    @staticmethod
    def connect(host: str, port: int, use_tls: bool, timeout_s: float) -> "ICBClient":
        s = socket.create_connection((host, port), timeout=timeout_s)
        if use_tls:
            ctx = ssl.create_default_context()
            ctx.check_hostname = False
            ctx.verify_mode = ssl.CERT_NONE
            s = ctx.wrap_socket(s, server_hostname=host)
        return ICBClient(s)

    def close(self) -> None:
        try:
            self.sock.close()
        except OSError:
            pass

    def recv_packet(self, timeout_s: float) -> Packet:
        payload = recv_frame(self.sock, timeout_s=timeout_s)
        if not payload:
            raise RuntimeError("empty frame payload")
        ptype = chr(payload[0])
        return Packet(ptype=ptype, payload=payload)

    def send_login(self, loginid: str, nick: str, group: str, password: str = "") -> None:
        # 'a' + fields + trailing NUL. (Server expects C-string parsing.)
        fields = ICB_SEP.join(
            [loginid.encode("ascii"), nick.encode("ascii"), group.encode("ascii"), b"login", password.encode("ascii")]
        ) + ICB_SEP
        payload = b"a" + fields + b"\x00"
        send_frame(self.sock, payload)

    def send_cmd(self, cmd: str, args: str = "") -> None:
        # 'h' + "cmd^Aargs\0"
        body = cmd.encode("ascii") + ICB_SEP + args.encode("utf-8") + b"\x00"
        send_frame(self.sock, b"h" + body)

    def send_open(self, msg: str) -> None:
        send_frame(self.sock, b"b" + msg.encode("utf-8") + b"\x00")

    def wait_for(
        self,
        pred: Callable[[Packet], bool],
        timeout_s: float,
        per_read_timeout_s: float = 0.25,
    ) -> list[Packet]:
        deadline = time.time() + timeout_s
        seen: list[Packet] = []
        while time.time() < deadline:
            pkt = self.recv_packet(timeout_s=min(per_read_timeout_s, max(0.01, deadline - time.time())))
            seen.append(pkt)
            if pred(pkt):
                return seen
        raise TimeoutError(f"timed out waiting for predicate; saw {len(seen)} packets")

    def drain_for(self, duration_s: float) -> list[Packet]:
        deadline = time.time() + duration_s
        seen: list[Packet] = []
        while time.time() < deadline:
            try:
                pkt = self.recv_packet(timeout_s=min(0.10, max(0.01, deadline - time.time())))
                seen.append(pkt)
            except (TimeoutError, socket.timeout):
                pass
        return seen


def copy_fixtures(src_dir: Path, dst_dir: Path) -> None:
    dst_dir.mkdir(parents=True, exist_ok=True)
    for entry in src_dir.iterdir():
        if entry.is_file():
            shutil.copy2(entry, dst_dir / entry.name)


def ensure_test_pem(run_dir: Path) -> None:
    pem_path = run_dir / "icbd.pem"
    if pem_path.exists():
        return

    openssl = shutil.which("openssl")
    if not openssl:
        raise RuntimeError("openssl not found in PATH (required for TLS integration tests)")

    key_path = run_dir / "key.pem"
    crt_path = run_dir / "cert.pem"

    subprocess.run(
        [
            openssl,
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-keyout",
            str(key_path),
            "-out",
            str(crt_path),
            "-days",
            "1",
            "-subj",
            "/CN=localhost",
        ],
        cwd=str(run_dir),
        check=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    pem_path.write_bytes(key_path.read_bytes() + b"\n" + crt_path.read_bytes())


def start_server(
    icbd_path: Path,
    run_dir: Path,
    clear_port: int,
    ssl_port: Optional[int],
    log_level: int = 0,
) -> subprocess.Popen:
    cmd = [str(icbd_path), "-f", "-p", str(clear_port), "-l", str(log_level)]
    if ssl_port is not None:
        # Note: icbd uses getopt() optional-arg parsing for -s (s::), which in
        # GNU getopt requires the argument be attached (e.g. "-s7327"), not
        # separated ("-s 7327").
        cmd += [f"-s{ssl_port}"]
    return subprocess.Popen(
        cmd,
        cwd=str(run_dir),
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
    )


def wait_for_connect(host: str, port: int, timeout_s: float) -> socket.socket:
    deadline = time.time() + timeout_s
    last_err: Optional[BaseException] = None
    while time.time() < deadline:
        try:
            return socket.create_connection((host, port), timeout=0.25)
        except OSError as e:
            last_err = e
            time.sleep(0.05)
    raise RuntimeError(f"could not connect to icbd on {host}:{port}: {last_err}")


class ServerRun:
    def __init__(self, proc: subprocess.Popen, run_dir: Path):
        self.proc = proc
        self.run_dir = run_dir

    def is_alive(self) -> bool:
        return self.proc.poll() is None

    def dump_diagnostics(self, label: str = "") -> None:
        """Print diagnostic info about the server — useful on test failure."""
        prefix = f"[DIAG {label}] " if label else "[DIAG] "
        alive = self.is_alive()
        print(f"{prefix}server pid={self.proc.pid} alive={alive}", flush=True)
        if not alive:
            print(f"{prefix}server exit code: {self.proc.returncode}", flush=True)

        # Dump the server log file
        log_path = self.run_dir / "icbd.log"
        if log_path.exists():
            try:
                log_text = log_path.read_text(errors="replace")
                if log_text.strip():
                    print(f"{prefix}=== icbd.log ===", flush=True)
                    for line in log_text.splitlines()[-50:]:
                        print(f"{prefix}  {line}", flush=True)
                    print(f"{prefix}=== end icbd.log ===", flush=True)
                else:
                    print(f"{prefix}icbd.log is empty", flush=True)
            except OSError as e:
                print(f"{prefix}could not read icbd.log: {e}", flush=True)
        else:
            print(f"{prefix}icbd.log not found at {log_path}", flush=True)

        # Drain stdout/stderr (non-blocking)
        for stream_name, stream in [("stdout", self.proc.stdout), ("stderr", self.proc.stderr)]:
            if stream is None:
                continue
            try:
                import select as _sel
                if hasattr(_sel, "select"):
                    r, _, _ = _sel.select([stream], [], [], 0)
                    if r:
                        data = stream.read(4096) if hasattr(stream, "read") else ""
                        if data:
                            print(f"{prefix}{stream_name}: {data!r}", flush=True)
            except Exception:
                pass

    def stop(self) -> None:
        if self.proc.poll() is not None:
            return
        self.proc.terminate()
        try:
            self.proc.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            self.proc.kill()
            self.proc.wait(timeout=2.0)


def with_server(
    icbd_path: Path,
    fixtures_dir: Path,
    *,
    enable_tls: bool,
    startup_timeout_s: float = 2.5,
) -> tuple[ServerRun, int, Optional[int]]:
    clear_port = find_free_port()
    ssl_port = find_free_port() if enable_tls else None

    td = tempfile.TemporaryDirectory(prefix="icbd-test-")
    run_dir = Path(td.name) / "run"
    copy_fixtures(fixtures_dir, run_dir)
    if enable_tls:
        ensure_test_pem(run_dir)

    proc = start_server(icbd_path, run_dir, clear_port=clear_port, ssl_port=ssl_port, log_level=3)
    server = ServerRun(proc=proc, run_dir=run_dir)
    # keep tempdir alive by attaching it
    server._tempdir = td  # type: ignore[attr-defined]

    # Wait for at least the clear port to accept.
    s = wait_for_connect("127.0.0.1", clear_port, timeout_s=startup_timeout_s)
    s.close()

    # If TLS is enabled, also wait for the TLS listener to accept.
    if enable_tls and ssl_port is not None:
        s2 = wait_for_connect("127.0.0.1", ssl_port, timeout_s=startup_timeout_s)
        s2.close()

    # Brief pause to let the server process the probe connection's disconnect
    # before we connect the real test client. This avoids a race condition on
    # some platforms (notably macOS) where the server hasn't finished cleaning
    # up the probe FD when the test client connects.
    time.sleep(0.10)

    return server, clear_port, ssl_port


def login_and_sync(client: ICBClient, loginid: str, nick: str, group: str, io_timeout_s: float) -> list[Packet]:
    """
    Consume the initial protocol banner ('j'), send login, and wait for login-ok ('a').
    Returns all packets seen during this handshake.
    """
    seen: list[Packet] = []
    # protocol banner
    pkt = client.recv_packet(timeout_s=io_timeout_s)
    seen.append(pkt)
    if pkt.ptype != "j":
        raise AssertionError(f"expected protocol banner 'j', got {pkt.ptype!r} payload={pkt.payload!r}")

    client.send_login(loginid=loginid, nick=nick, group=group, password="")

    def is_login_ok(p: Packet) -> bool:
        return p.ptype == "a"

    seen.extend(client.wait_for(is_login_ok, timeout_s=io_timeout_s))
    return seen


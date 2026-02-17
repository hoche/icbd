"""
icbdb.py - Python module for reading/writing ICB's internal .db format.

File format (little-endian, ".db" suffix appended to the base name):
  Header  = magic[4] + entry_count[4]          (8 bytes)
  Entry_i = key_len[4] + val_len[4] + key + val

The magic bytes are b'IDB\\x01'.
"""

import os
import struct
import tempfile

MAGIC = b'IDB\x01'
HDR_SIZE = 8
ENTRY_HDR_SIZE = 8


class IcbDb:
    """Read/write access to an ICB .db file.

    Usage:
        with IcbDb("./icbdb") as db:
            db["server.nick"] = "server"
            print(db["server.nick"])
            for key, val in db.items():
                print(key, val)
    """

    def __init__(self, basepath):
        """Open (or create) the database at *basepath*.db."""
        self.path = basepath + ".db"
        self._data = {}    # key (str) -> value (str)
        self._dirty = False
        self._load()

    # ---- context-manager support -------------------------------------------
    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
        return False

    # ---- dict-like interface -----------------------------------------------
    def __getitem__(self, key):
        return self._data[key]

    def __setitem__(self, key, value):
        self._data[key] = value
        self._dirty = True

    def __delitem__(self, key):
        del self._data[key]
        self._dirty = True

    def __contains__(self, key):
        return key in self._data

    def __len__(self):
        return len(self._data)

    def __iter__(self):
        return iter(self._data)

    def get(self, key, default=None):
        return self._data.get(key, default)

    def keys(self):
        return self._data.keys()

    def values(self):
        return self._data.values()

    def items(self):
        return self._data.items()

    def pop(self, key, *args):
        result = self._data.pop(key, *args)
        self._dirty = True
        return result

    # ---- persistence -------------------------------------------------------
    def _load(self):
        """Load the .db file into memory."""
        if not os.path.exists(self.path):
            return

        with open(self.path, "rb") as f:
            hdr = f.read(HDR_SIZE)
            if len(hdr) < HDR_SIZE:
                raise ValueError(f"Truncated header in {self.path}")
            if hdr[:4] != MAGIC:
                raise ValueError(f"Bad magic in {self.path}")

            count = struct.unpack_from("<I", hdr, 4)[0]

            for _ in range(count):
                rec_hdr = f.read(ENTRY_HDR_SIZE)
                if len(rec_hdr) < ENTRY_HDR_SIZE:
                    raise ValueError(f"Truncated entry header in {self.path}")
                klen, vlen = struct.unpack("<II", rec_hdr)
                key = f.read(klen).decode("utf-8", errors="replace")
                val = f.read(vlen).decode("utf-8", errors="replace")
                self._data[key] = val

    def flush(self):
        """Write the in-memory state to disk atomically (write-tmp + rename)."""
        if not self._dirty:
            return

        dirpath = os.path.dirname(self.path) or "."
        fd, tmp = tempfile.mkstemp(dir=dirpath, suffix=".tmp")
        try:
            with os.fdopen(fd, "wb") as f:
                # Header
                f.write(MAGIC)
                f.write(struct.pack("<I", len(self._data)))
                # Entries
                for key, val in self._data.items():
                    kb = key.encode("utf-8")
                    vb = val.encode("utf-8")
                    f.write(struct.pack("<II", len(kb), len(vb)))
                    f.write(kb)
                    f.write(vb)
                f.flush()
                os.fsync(f.fileno())
            os.replace(tmp, self.path)
        except Exception:
            os.unlink(tmp)
            raise
        self._dirty = False

    def close(self):
        """Flush any pending changes and close."""
        self.flush()

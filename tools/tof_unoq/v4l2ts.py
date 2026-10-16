"""Parse per-buffer lines printed by ``v4l2-ctl --stream-mmap ... --verbose``.

v4l-utils utils/v4l2-ctl/v4l2-ctl-streaming.cpp, print_concise_buffer(), writes
one line per dequeued buffer to stderr, e.g.

    cap dqbuf: 0 seq:      3 bytesused: 2531328 ts: 1234.567890 delta: 33.333 ms fps: 30.00 \
field: None (ts-monotonic, ts-src-eof)

The timestamp is printed only when the driver does not copy it from user space,
and the flags in parentheses say which clock it is on. Only ``ts-monotonic``
timestamps share a clock with Python's ``time.monotonic_ns()``; anything else
is reported as such and never silently treated as monotonic.
"""

from __future__ import annotations

import re
from dataclasses import dataclass

_LINE = re.compile(
    r"^(?P<dir>cap|out) dqbuf:\s*(?P<index>\d+)\s+seq:\s*(?P<seq>\d+)"
    r"\s+bytesused:\s*(?P<bytes>[\d/]+)"
)
_TS = re.compile(r"\bts:\s*(?P<ts>\d+\.\d+)")
_FLAGS = re.compile(r"\((?P<flags>[^()]*)\)\s*$")


@dataclass
class BufferStamp:
    index: int
    seq: int
    bytesused: str
    ts_s: float | None
    flags: list[str]

    @property
    def clock(self) -> str:
        if "ts-monotonic" in self.flags:
            return "CLOCK_MONOTONIC"
        if "ts-copy" in self.flags:
            return "copied_from_userspace"
        if self.ts_s is None:
            return "none"
        return "unknown"

    @property
    def ts_mono_ns(self) -> int | None:
        if self.ts_s is None or self.clock != "CLOCK_MONOTONIC":
            return None
        return round(self.ts_s * 1e9)

    @property
    def error(self) -> bool:
        return "error" in self.flags


def parse(text: str) -> list[BufferStamp]:
    out = []
    for raw in text.splitlines():
        line = raw.strip()
        m = _LINE.match(line)
        if not m or m.group("dir") != "cap":
            continue
        ts = _TS.search(line)
        fl = _FLAGS.search(line)
        flags = [f.strip() for f in fl.group("flags").split(",")] if fl else []
        out.append(
            BufferStamp(
                index=int(m.group("index")),
                seq=int(m.group("seq")),
                bytesused=m.group("bytes"),
                ts_s=float(ts.group("ts")) if ts else None,
                flags=flags,
            )
        )
    return out

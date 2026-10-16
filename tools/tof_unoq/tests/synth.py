"""SYNTHETIC line generators for tests. Nothing here is sensor output or bench evidence."""

from __future__ import annotations

import socket
import sys
import threading
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from tofproto import make_line  # noqa: E402


def hello(us: int = 1_000) -> str:
    return make_line(
        f"TOF,HELLO,{us},proto=1,sketch=tof_unoq_vl53l8cx SYNTHETIC,uld=VL53L8CX_1.3.0"
    )


def evt(us: int, level: str, code: str, **kv) -> str:
    extra = "".join(f",{k}={v}" for k, v in kv.items())
    return make_line(f"TOF,EVT,{us},{level},{code}{extra}")


def cfg(us: int = 3_000_000, zones: int = 64, hz: int = 5) -> str:
    return make_line(
        f"TOF,CFG,{us},zones={zones},hz={hz},mode=continuous,integration_ms=20,sharpener_pct=5,"
        f"target_order=strongest,targets_per_zone=1"
    )


def frame(
    seq: int,
    zones: int = 64,
    distance=lambda z, s: 500 + (z % 3),
    status=lambda z, s: 5,
    period_us: int = 200_000,
    t0_us: int = 4_000_000,
    stream: int | None = None,
) -> str:
    ready = t0_us + seq * period_us
    sc = (seq + 1) % 256 if stream is None else stream
    parts = [f"TOF,FRM,{seq},{sc},{ready},{ready + 9_000},{zones},31"]
    for z in range(zones):
        st = status(z, seq)
        parts.append(f"{distance(z, seq)}:{st}:{0 if st == 255 else 1}:{6 + z % 4}")
    return make_line(",".join(parts))


def healthy_session(n: int = 10, zones: int = 64) -> list[str]:
    return [
        hello(),
        evt(10_000, "I", "BUS_BEGIN", bus="Wire1_I2C4_SDA_PD13_SCL_PD12", hz=400000),
        evt(20_000, "I", "SCAN", count=1, addrs="0x29"),
        evt(21_000, "I", "ACK", addr7=41),
        evt(22_000, "I", "ID_OK", device_id=240, revision_id=12),
        evt(2_900_000, "I", "INIT_OK", ms=2850),
        cfg(zones=zones),
        evt(3_100_000, "I", "RANGING_START"),
        *[frame(i, zones) for i in range(n)],
    ]


class FakeMonitorServer:
    """Localhost TCP server standing in for arduino-router's monitor port (SYNTHETIC)."""

    def __init__(self, lines: list[str], interval_s: float = 0.002, keep_open_s: float = 0.5):
        self.lines = lines
        self.interval_s = interval_s
        self.keep_open_s = keep_open_s
        self.received = b""
        self.srv = socket.socket()
        self.srv.bind(("127.0.0.1", 0))
        self.srv.listen(1)
        self.port = self.srv.getsockname()[1]
        self.thread = threading.Thread(target=self._serve, daemon=True)
        self.thread.start()

    def _serve(self) -> None:
        conn, _ = self.srv.accept()
        conn.settimeout(0.05)
        for ln in self.lines:
            conn.sendall(ln.encode("ascii") + b"\n")
            self._drain(conn)
            time.sleep(self.interval_s)
        end = time.monotonic() + self.keep_open_s
        while time.monotonic() < end:
            self._drain(conn)
        conn.close()
        self.srv.close()

    def _drain(self, conn) -> None:
        try:
            data = conn.recv(4096)
            if data:
                self.received += data
        except (TimeoutError, OSError):
            pass

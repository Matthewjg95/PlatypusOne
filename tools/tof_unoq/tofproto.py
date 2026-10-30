"""Parser for the tof_unoq_vl53l8cx line protocol (firmware/tof_unoq_vl53l8cx/tof_proto.h).

Every record is ``$TOF,<TYPE>,<fields...>*CCCC`` where CCCC is the upper-case hex
CRC-16/CCITT-FALSE of the bytes between ``$`` and ``*``. Lines that fail the CRC
are rejected, never repaired: the Monitor path can split or drop output.

Target-status meanings are copied from ST UM3109 Rev 7, Table 4; the confidence
classes are ST's own statement under that table ("status 5 is considered as
100% valid. A status of 6 or 9 can be considered with a confidence value of
50%. All other statuses are below the 50% confidence level."). Nothing here
corrects or calibrates a distance.

Protocol 2 adds the trigger and the IMU on the same bus (qwiic_peripherals.h):
``EVT ... TRIGGER`` (edge=press|release, presses, src) and ``IMU`` records with
acceleration in mg and angular rate in mdps on the BMI270's own axes. Protocol 1
streams parse unchanged.
"""

from __future__ import annotations

from dataclasses import dataclass, field

PROTOCOL_VERSION = 2

# ST UM3109 Rev 7, Table 4 "List of available target status".
TARGET_STATUS = {
    0: "Ranging data are not updated",
    1: "Signal rate too low on SPAD array",
    2: "Target phase",
    3: "Sigma estimator too high",
    4: "Target consistency failed",
    5: "Range valid",
    6: "Wrap around not performed (typically the first range)",
    7: "Rate consistency failed",
    8: "Signal rate too low for the current target",
    9: "Range valid with large pulse (may be due to a merged target)",
    10: "Range valid, but no target detected at previous range",
    11: "Measurement consistency failed",
    12: "Target blurred by another one, due to sharpener",
    13: "Target detected but inconsistent data. Frequently happens for secondary targets.",
    255: "No target detected (only if number of targets detected is enabled)",
}

# UM3109 Rev 7, note under Table 4 / Table 5 (driver errors).
ULD_ERRORS = {
    0: "No error",
    127: "Incorrect setting (unknown resolution, ranging frequency too high, ...)",
    255: "Major error, usually a timeout due to an I2C/SPI error",
}


def status_class(status: int) -> str:
    """ST's confidence wording for a target status (UM3109 Rev 7, Table 4 note)."""
    if status == 5:
        return "valid"
    if status in (6, 9):
        return "valid_50pct"
    if status == 255:
        return "no_target"
    return "below_50pct"


class ProtocolError(ValueError):
    """A line that is not a well-formed, CRC-correct record."""


def crc16_ccitt_false(data: bytes) -> int:
    crc = 0xFFFF
    for byte in data:
        crc ^= byte << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def make_line(body: str) -> str:
    """Build a record from its body (text between '$' and '*'). Used by tests and simulators."""
    return f"${body}*{crc16_ccitt_false(body.encode('ascii')):04X}"


@dataclass
class Zone:
    index: int
    distance_mm: int
    target_status: int
    nb_target_detected: int
    range_sigma_mm: int

    @property
    def status_class(self) -> str:
        return status_class(self.target_status)

    def row_col(self, zones: int) -> tuple[int, int]:
        """Row/column of the ULD zone index (raw order; the lens flips the scene, DS14161 7.1.3)."""
        side = 8 if zones == 64 else 4
        return self.index // side, self.index % side


@dataclass
class Record:
    type: str
    fields: list[str]
    raw: str
    kv: dict[str, str] = field(default_factory=dict)
    mcu_us: int | None = None
    # EVT
    level: str | None = None
    code: str | None = None
    # FRM
    seq: int | None = None
    streamcount: int | None = None
    us_ready: int | None = None
    us_read_done: int | None = None
    zones_count: int | None = None
    temp_c: int | None = None
    zones: list[Zone] = field(default_factory=list)
    # IMU (BMI270 sensor axes; no mounting transform, no calibration)
    sensortime: int | None = None
    acc_mg: tuple[int, int, int] | None = None
    gyr_mdps: tuple[int, int, int] | None = None

    @property
    def temp_cc(self) -> int | None:
        """IMU die temperature in centi-degC, when the record carries one."""
        return int(self.kv["t_cc"]) if "t_cc" in self.kv else None


def _kv(items: list[str]) -> dict[str, str]:
    out = {}
    for it in items:
        if "=" not in it:
            raise ProtocolError(f"expected key=value, got {it!r}")
        k, v = it.split("=", 1)
        out[k] = v
    return out


def _int(s: str, what: str) -> int:
    try:
        return int(s)
    except ValueError as exc:
        raise ProtocolError(f"{what}: not an integer: {s!r}") from exc


def parse_line(line: str) -> Record:
    """Parse one record. Raises ProtocolError for anything malformed or CRC-failing."""
    raw = line.rstrip("\r\n")
    start = raw.find("$TOF,")
    if start < 0:
        raise ProtocolError("no $TOF, start marker")
    if start > 0:
        raise ProtocolError(f"{start} unexpected bytes before the start marker")
    star = raw.rfind("*")
    if star < 0 or len(raw) - star != 5:
        raise ProtocolError("missing or truncated *CCCC checksum")
    body = raw[1:star]
    try:
        want = int(raw[star + 1 :], 16)
    except ValueError as exc:
        raise ProtocolError("checksum is not hex") from exc
    got = crc16_ccitt_false(body.encode("ascii", errors="replace"))
    if got != want:
        raise ProtocolError(f"CRC mismatch: line says {want:04X}, computed {got:04X}")
    parts = body.split(",")
    if len(parts) < 3 or parts[0] != "TOF":
        raise ProtocolError("too few fields")
    rtype, rest = parts[1], parts[2:]
    rec = Record(type=rtype, fields=rest, raw=raw)
    if rtype in ("HELLO", "CFG", "HB"):
        rec.mcu_us = _int(rest[0], "mcu_us")
        rec.kv = _kv(rest[1:])
    elif rtype == "EVT":
        if len(rest) < 3:
            raise ProtocolError("EVT needs mcu_us, level, code")
        rec.mcu_us = _int(rest[0], "mcu_us")
        rec.level, rec.code = rest[1], rest[2]
        if rec.level not in ("I", "W", "E"):
            raise ProtocolError(f"EVT level {rec.level!r}")
        rec.kv = _kv(rest[3:])
    elif rtype == "FRM":
        if len(rest) < 6:
            raise ProtocolError("FRM header too short")
        rec.seq = _int(rest[0], "seq")
        rec.streamcount = _int(rest[1], "streamcount")
        rec.us_ready = _int(rest[2], "us_ready")
        rec.us_read_done = _int(rest[3], "us_read_done")
        rec.mcu_us = rec.us_ready
        rec.zones_count = _int(rest[4], "zones")
        rec.temp_c = _int(rest[5], "temp_c")
        zone_fields = rest[6:]
        if rec.zones_count not in (16, 64):
            raise ProtocolError(f"zones must be 16 or 64, got {rec.zones_count}")
        if len(zone_fields) != rec.zones_count:
            raise ProtocolError(f"FRM says {rec.zones_count} zones, carries {len(zone_fields)}")
        for i, z in enumerate(zone_fields):
            p = z.split(":")
            if len(p) != 4:
                raise ProtocolError(f"zone {i}: expected d:s:n:g, got {z!r}")
            rec.zones.append(
                Zone(
                    i,
                    _int(p[0], "distance"),
                    _int(p[1], "status"),
                    _int(p[2], "nb"),
                    _int(p[3], "sigma"),
                )
            )
    elif rtype == "IMU":
        if len(rest) < 9:
            raise ProtocolError("IMU needs mcu_us, seq, sensortime, 3 acc, 3 gyr")
        rec.mcu_us = _int(rest[0], "mcu_us")
        rec.seq = _int(rest[1], "seq")
        rec.sensortime = _int(rest[2], "sensortime")
        acc = [_int(v, "acc") for v in rest[3:6]]
        gyr = [_int(v, "gyr") for v in rest[6:9]]
        rec.acc_mg = (acc[0], acc[1], acc[2])
        rec.gyr_mdps = (gyr[0], gyr[1], gyr[2])
        rec.kv = _kv(rest[9:])
    else:
        raise ProtocolError(f"unknown record type {rtype!r}")
    return rec

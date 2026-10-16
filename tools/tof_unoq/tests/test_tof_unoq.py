"""Tests for the UNO Q + VL53L8CX tooling. All inputs are SYNTHETIC.

Run: python3 -m unittest discover -s tools/tof_unoq/tests -v
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import shutil
import subprocess
import sys
import tempfile
import textwrap
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
TOOLS = HERE.parent
REPO = TOOLS.parents[1]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(HERE))

import analyze  # noqa: E402
import coacq  # noqa: E402
import synth  # noqa: E402
import tof_log  # noqa: E402
import tofproto  # noqa: E402
import v4l2ts  # noqa: E402


def uld_src() -> Path | None:
    env = os.environ.get("TOF_ULD_SRC")
    cands = [Path(env)] if env else []
    cands += [
        Path.home() / "Arduino/libraries/STM32duino_VL53L8CX/src",
        Path("/home/user/stm32duino/vl53l8cx/src"),
    ]
    for c in cands:
        if (c / "vl53l8cx_api.c").exists():
            return c
    return None


class ProtocolTests(unittest.TestCase):
    def test_crc_check_value(self):
        self.assertEqual(tofproto.crc16_ccitt_false(b"123456789"), 0x29B1)

    def test_frame_round_trip(self):
        rec = tofproto.parse_line(synth.frame(3))
        self.assertEqual((rec.type, rec.seq, rec.zones_count, len(rec.zones)), ("FRM", 3, 64, 64))
        self.assertEqual(rec.zones[0].distance_mm, 500)
        self.assertEqual(rec.zones[9].row_col(64), (1, 1))

    def test_rejects_corruption_truncation_and_count_mismatch(self):
        good = synth.frame(1)
        bad_digit = good.replace(",501:", ",511:", 1)
        for line in (bad_digit, good[: len(good) // 2], good[:-1], "garbage", "xx" + good):
            with self.subTest(line=line[:40]), self.assertRaises(tofproto.ProtocolError):
                tofproto.parse_line(line)
        short = tofproto.make_line("TOF,FRM,1,2,3,4,64,30,500:5:1:6")
        with self.assertRaises(tofproto.ProtocolError):
            tofproto.parse_line(short)

    def test_status_classes_follow_um3109(self):
        self.assertEqual(tofproto.status_class(5), "valid")
        self.assertEqual(tofproto.status_class(6), "valid_50pct")
        self.assertEqual(tofproto.status_class(9), "valid_50pct")
        self.assertEqual(tofproto.status_class(255), "no_target")
        self.assertEqual(tofproto.status_class(10), "below_50pct")
        self.assertIn(13, tofproto.TARGET_STATUS)


class FirmwareHostTests(unittest.TestCase):
    """Compiles the firmware's Arduino-independent headers on the host."""

    @classmethod
    def setUpClass(cls):
        cls.tmp = Path(tempfile.mkdtemp())
        cls.cxx = shutil.which("g++")
        cls.cc = shutil.which("gcc")
        cls.binary = None
        cls.with_uld = False
        if not cls.cxx:
            return
        src = HERE / "host" / "test_host.cpp"
        cmd = [cls.cxx, "-std=c++17", "-Wall", "-Wextra", "-Werror", "-O1", str(src)]
        uld = uld_src()
        if uld and cls.cc:
            objs = []
            for c in ("vl53l8cx_api.c", "vl53l8cx_platform.c"):
                o = cls.tmp / (c + ".o")
                subprocess.run(
                    [cls.cc, "-O1", "-w", f"-I{uld}", "-c", str(uld / c), "-o", str(o)], check=True
                )
                objs.append(str(o))
            cmd = cmd[:-1] + ["-DTOF_WITH_ULD", f"-I{uld}", str(src), *objs]
            cls.with_uld = True
        cls.binary = cls.tmp / "test_host"
        subprocess.run([*cmd, "-o", str(cls.binary)], check=True)

    @classmethod
    def tearDownClass(cls):
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def test_platform_and_protocol_checks(self):
        if not self.binary:
            self.skipTest("no g++")
        r = subprocess.run([str(self.binary)], capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        self.assertIn("all host checks passed", r.stdout)
        if self.with_uld:
            self.assertIn("ULD is_alive integration: ran", r.stdout)

    def test_cpp_lines_parse_in_python(self):
        if not self.binary:
            self.skipTest("no g++")
        out = subprocess.run(
            [str(self.binary), "emit"], capture_output=True, text=True, check=True
        ).stdout
        recs = [tofproto.parse_line(ln) for ln in out.splitlines()]
        self.assertEqual([r.type for r in recs], ["HELLO", "EVT", "FRM", "FRM", "FRM"])
        frm = recs[2]
        self.assertEqual(frm.temp_c, -3)
        self.assertEqual(frm.zones[0].target_status, 255)
        self.assertEqual(frm.zones[1].distance_mm, -5)
        self.assertEqual(frm.zones[63].status_class, "valid_50pct")


def _args(out: Path, port: int, **kw) -> argparse.Namespace:
    a = tof_log.build_parser().parse_args(["--out", str(out), "--port", str(port)])
    for k, v in kw.items():
        setattr(a, k, v)
    return a


class LoggerTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def test_healthy_session_with_corruption_and_gap(self):
        lines = synth.healthy_session(8)
        lines.insert(10, lines[10][:-3] + "XYZ")  # CRC-broken copy of a frame
        del lines[12]  # drop one frame -> sequence gap
        srv = synth.FakeMonitorServer(lines)
        out = self.tmp / "s1"
        rc = tof_log.run(
            _args(out, srv.port, frames=7, init_timeout=5, ref_distance_mm=500.0, label="500mm")
        )
        self.assertEqual(rc, 0)
        meta = json.loads((out / "session.json").read_text())
        self.assertEqual(meta["frames"], 7)
        self.assertEqual(meta["crc_or_format_rejects"], 1)
        self.assertEqual(len(meta["sequence_gaps"]), 1)
        self.assertEqual(meta["cfg_read_back_from_sensor"]["zones"], "64")
        self.assertIn(b"STATUS", srv.received)
        with open(out / "zones.csv") as f:
            self.assertEqual(sum(1 for _ in csv.DictReader(f)), 7 * 64)
        raw = (out / "raw.log").read_text()
        self.assertIn(" BAD ", raw)

    def test_no_ack_is_diagnosed_and_exits_2(self):
        lines = [
            synth.hello(),
            synth.evt(10, "I", "SCAN", count=0, addrs="none"),
            synth.evt(11, "E", "NO_ACK", addr7=41),
        ]
        srv = synth.FakeMonitorServer(lines)
        out = self.tmp / "s2"
        rc = tof_log.run(_args(out, srv.port, init_timeout=5))
        self.assertEqual(rc, 2)
        meta = json.loads((out / "session.json").read_text())
        self.assertIn("SPI/I2C-to-GND jumper", meta["diagnosis"])
        self.assertEqual(meta["frames"], 0)

    def test_refuses_existing_output(self):
        out = self.tmp / "s3"
        out.mkdir()
        (out / "x").write_text("keep")
        self.assertEqual(tof_log.run(_args(out, 1)), 4)
        self.assertEqual((out / "x").read_text(), "keep")

    def test_connect_failure_exits_3(self):
        s = __import__("socket").socket()
        s.bind(("127.0.0.1", 0))
        port = s.getsockname()[1]
        s.close()
        self.assertEqual(tof_log.run(_args(self.tmp / "s4", port)), 3)

    def test_reconfiguration_commands_are_sent_once(self):
        srv = synth.FakeMonitorServer(synth.healthy_session(5))
        rc = tof_log.run(_args(self.tmp / "s5", srv.port, frames=5, res=4, hz=10, init_timeout=5))
        self.assertEqual(rc, 0)
        sent = srv.received.decode()
        self.assertEqual(sent.count("RES 4"), 1)
        self.assertEqual(sent.count("HZ 10"), 1)
        self.assertLess(sent.index("STOP"), sent.index("START"))


class AnalyzeTests(unittest.TestCase):
    def test_central_statistics_and_reference(self):
        tmp = Path(tempfile.mkdtemp())
        try:
            s = tof_log.TofSession(tmp / "a", "500mm", {"ref_distance_mm": 498.0})
            for i, ln in enumerate(synth.healthy_session(30)):
                s.handle(ln, 1_000_000_000 + i * 200_000_000, i)
            s.close("test")
            a = analyze.analyze(tmp / "a")
            self.assertEqual(a["frames"], 30)
            c = a["central"]
            self.assertEqual(c["zones"], [27, 28, 35, 36])
            self.assertEqual(c["frames_with_all_central_valid"], 30)
            # zones 27,28,35,36 -> 500 + z % 3 = 500, 501, 502, 500
            self.assertAlmostEqual(c["distance_valid_mm"]["median"], 500.5)
            self.assertAlmostEqual(c["median_minus_reference_mm"], 2.5)
            self.assertIn("no correction is derived", c["note"])
            md = analyze.to_markdown(a)
            self.assertNotIn("PASS", md.upper().replace("PASSWORD", ""))
            self.assertIn("drift_estimate_ppm", a["clock_relation"])
        finally:
            shutil.rmtree(tmp, ignore_errors=True)

    def test_invalid_zones_excluded_from_distance_stats(self):
        tmp = Path(tempfile.mkdtemp())
        try:
            s = tof_log.TofSession(tmp / "b")
            lines = [synth.cfg()] + [
                synth.frame(
                    i,
                    status=lambda z, s: 255 if z == 27 else 5,
                    distance=lambda z, s: 9999 if z == 27 else 700,
                )
                for i in range(5)
            ]
            for i, ln in enumerate(lines):
                s.handle(ln, i * 200_000_000, i)
            s.close("test")
            a = analyze.analyze(tmp / "b")
            self.assertEqual(a["zones"][27]["valid_frames"], 0)
            self.assertEqual(a["zones"][27]["distance_valid_mm"]["n"], 0)
            self.assertEqual(a["central"]["frames_with_all_central_valid"], 0)
            self.assertEqual(a["central"]["distance_valid_mm"]["median"], 700)
        finally:
            shutil.rmtree(tmp, ignore_errors=True)


class V4l2Tests(unittest.TestCase):
    # SYNTHETIC lines in the format of v4l-utils print_concise_buffer().
    _FL = "field: None (ts-monotonic, ts-src-eof)"
    TEXT = "\n".join(
        [
            f"cap dqbuf: 0 seq:      0 bytesused: 2526592 ts: 812.345678 {_FL}",
            f"cap dqbuf: 1 seq:      1 bytesused: 2526592 ts: 812.379012 delta: 33.334 ms "
            f"fps: 30.00 {_FL}",
            "cap dqbuf: 2 seq:      3 bytesused: 2526592 ts: 812.445678 delta: 66.666 ms "
            "fps: 30.00 dropped: 1 field: None (error, ts-monotonic, ts-src-eof)",
            "cap dqbuf: 3 seq:      4 bytesused: 2526592 ts: 9.000000 field: None (ts-copy)",
            "something else",
        ]
    )

    def test_parse(self):
        st = v4l2ts.parse(self.TEXT)
        self.assertEqual([s.seq for s in st], [0, 1, 3, 4])
        self.assertEqual(st[0].ts_mono_ns, 812_345_678_000)
        self.assertTrue(st[2].error)
        self.assertEqual(st[3].clock, "copied_from_userspace")
        self.assertIsNone(st[3].ts_mono_ns)


class CoacqTests(unittest.TestCase):
    def setUp(self):
        self.tmp = Path(tempfile.mkdtemp())

    def tearDown(self):
        shutil.rmtree(self.tmp, ignore_errors=True)

    def _args(self, port, **kw):
        a = coacq.build_parser().parse_args(["--out", str(self.tmp / "run"), "--port", str(port)])
        a.tof_wait, a.tail, a.interval = 5, 0.1, 0.02
        for k, v in kw.items():
            setattr(a, k, v)
        return a

    def test_per_frame_mode_pairs_every_camera_frame(self):
        srv = synth.FakeMonitorServer(synth.healthy_session(200), interval_s=0.005, keep_open_s=2)
        cam = self.tmp / "fakecam.py"
        cam.write_text(
            "import sys,json; open(sys.argv[1],'wb').write(b'SYNTHETIC'); "
            "print(json.dumps({'exposure_lines': 1700, 'synthetic': True}))\n"
        )
        rc = coacq.run(
            self._args(srv.port, camera_frames=3, camera_cmd=f"{sys.executable} {cam} {{out}}")
        )
        self.assertEqual(rc, 0)
        root = self.tmp / "run"
        man = json.loads((root / "coacq.json").read_text())
        self.assertEqual(man["camera"]["frames_recorded"], 3)
        self.assertEqual(man["camera"]["failed_commands"], 0)
        self.assertIn("No hardware trigger", man["synchronisation"])
        with open(root / "pairs.csv") as f:
            pairs = list(csv.DictReader(f))
        self.assertEqual(len(pairs), 3)
        self.assertTrue(all(p["tof_seq"] != "" for p in pairs))
        self.assertTrue(all(p["camera_time_basis"] == "host_bracket_midpoint" for p in pairs))
        with open(root / "camera" / "camera.csv") as f:
            self.assertIn("synthetic", next(csv.DictReader(f))["camera_meta_json"])

    def test_stream_mode_uses_v4l2_timestamps(self):
        srv = synth.FakeMonitorServer(synth.healthy_session(100), interval_s=0.005, keep_open_s=2)
        fake = self.tmp / "fake_v4l2ctl.py"
        fake.write_text(
            textwrap.dedent("""\
            import sys, time
            out = sys.argv[1]; n = int(sys.argv[2])
            open(out, 'wb').write(b'SYNTHETIC')
            for i in range(n):
                t = time.monotonic()
                fl = "field: None (ts-monotonic, ts-src-eof)"
                sys.stderr.write(f"cap dqbuf: {i} seq: {i:6d} bytesused: 9 ts: {t:.6f} {fl}\\n")
                time.sleep(0.03)
            """)
        )
        rc = coacq.run(
            self._args(
                srv.port, camera_frames=4, camera_stream=f"{sys.executable} {fake} {{out}} {{n}}"
            )
        )
        self.assertEqual(rc, 0)
        with open(self.tmp / "run" / "pairs.csv") as f:
            pairs = list(csv.DictReader(f))
        self.assertEqual(len(pairs), 4)
        self.assertTrue(all(p["camera_time_basis"] == "v4l2_buffer_timestamp" for p in pairs))
        self.assertTrue(all(abs(float(p["dt_ms"])) < 200 for p in pairs))

    def test_camera_not_started_when_tof_fails(self):
        srv = synth.FakeMonitorServer(
            [synth.hello(), synth.evt(5, "E", "NO_ACK", addr7=41)], keep_open_s=1
        )
        marker = self.tmp / "camera_ran"
        cmd = f"{sys.executable} -c \"open('{marker}','w')\""
        rc = coacq.run(self._args(srv.port, camera_frames=1, camera_cmd=cmd, tof_wait=2))
        self.assertEqual(rc, 2)
        self.assertFalse(marker.exists())


if __name__ == "__main__":
    unittest.main()

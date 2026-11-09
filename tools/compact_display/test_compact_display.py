"""Host checks for the compact-display panel definitions and overlay generator.

These prove the files are internally consistent and match the vendor's own
overlay. They say nothing about whether a panel lights up on an UNO Q; that is
bench_accept.sh.

    python3 -m unittest tools/compact_display/test_compact_display.py
"""

from __future__ import annotations

import json
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import gen_overlay  # noqa: E402

PANELS = {
    "35H": HERE / "panels/waveshare-3in5-dsi-h.panel",
    "35E": HERE / "panels/waveshare-3in5-dsi-e.panel",
}
REFERENCE = json.loads((HERE / "reference/waveshare_35dsi_overrides.json").read_text())

TIMING_KEYS = {
    "hactive": "HACTIVE",
    "hfp": "HFRONT",
    "hsync": "HSYNC",
    "hbp": "HBACK",
    "vactive": "VACTIVE",
    "vfp": "VFRONT",
    "vsync": "VSYNC",
    "vbp": "VBACK",
}
ALL_KEYS = [
    "CLOCK_KHZ",
    "DSI_FORMAT",
    "DSI_MODE_FLAGS",
    "TOUCH_ADDR",
    "GOODIX_ALT_ADDR",
    "PANEL_CTRL_COMPATIBLE",
    *TIMING_KEYS.values(),
]


def source(path: Path, keys: list[str]) -> dict[str, str]:
    script = f". '{path}'; " + "; ".join(f'printf "%s=%s\\n" {k} "${k}"' for k in keys)
    out = subprocess.run(["sh", "-c", script], capture_output=True, text=True, check=True).stdout
    return dict(line.partition("=")[::2] for line in out.splitlines())


def panel(name: str) -> dict[str, str]:
    path = PANELS[name]
    return {**gen_overlay.load_panel(path), **source(path, ALL_KEYS)}


def timing(p: dict[str, str]) -> tuple[int, int, int]:
    htotal = sum(int(p[k]) for k in ("HACTIVE", "HFRONT", "HSYNC", "HBACK"))
    vtotal = sum(int(p[k]) for k in ("VACTIVE", "VFRONT", "VSYNC", "VBACK"))
    return int(p["CLOCK_KHZ"]) * 1000, htotal, vtotal


class PanelMatchesVendorOverlay(unittest.TestCase):
    def test_timings_equal_waveshare_overlay(self):
        for name in PANELS:
            p, ref = panel(name), REFERENCE[name]
            with self.subTest(panel=name):
                self.assertEqual(int(p["CLOCK_KHZ"]) * 1000, ref["clock_hz"])
                for ref_key, panel_key in TIMING_KEYS.items():
                    self.assertEqual(int(p[panel_key]), ref[ref_key], panel_key)
                self.assertEqual(p["DSI_FORMAT"], "MIPI_DSI_FMT_" + ref["format"])

    def test_lanes_mode_and_no_controller(self):
        for name in PANELS:
            p = panel(name)
            with self.subTest(panel=name):
                self.assertEqual(int(p["DSI_LANES"]), REFERENCE["default_data_lanes"])
                self.assertEqual(p["DSI_MODE_FLAGS"], "MIPI_DSI_" + REFERENCE["mode"])
                # Waveshare's overlay has no regulator/backlight: neither do we.
                self.assertIsNone(REFERENCE["power_supply"])
                self.assertEqual(p["PANEL_CTRL_ADDR"], "")
                self.assertEqual(p["PANEL_CTRL_COMPATIBLE"], "")
                # Empty so the upstream build does not patch edt-ft5x06.
                self.assertEqual(p["TOUCH_ADDR"], "")

    def test_touch_matches_vendor_and_mode(self):
        for name in PANELS:
            p = panel(name)
            with self.subTest(panel=name):
                self.assertEqual(p["GOODIX_COMPATIBLE"], REFERENCE["touch"]["compatible"])
                self.assertEqual(
                    {p["GOODIX_ADDR"], p["GOODIX_ALT_ADDR"]}, set(REFERENCE["touch"]["addresses"])
                )
                self.assertEqual(int(p["GOODIX_SIZE_X"]), int(p["HACTIVE"]))
                self.assertEqual(int(p["GOODIX_SIZE_Y"]), int(p["VACTIVE"]))

    def test_refresh_is_60hz(self):
        for name in PANELS:
            clock, htotal, vtotal = timing(panel(name))
            with self.subTest(panel=name):
                self.assertAlmostEqual(clock / (htotal * vtotal), 60.0, delta=0.5)

    def test_single_lane_bit_rate_as_documented(self):
        # The selection doc and the H panel header quote 806 Mbit/s against the
        # proven 5" at 667 Mbit/s (27.8 MHz x 24 bpp / 1 lane). Keep them honest.
        clock, _, _ = timing(panel("35H"))
        self.assertAlmostEqual(clock * 24 / 1 / 1e6, 806.4, places=1)
        clock, _, _ = timing(panel("35E"))
        self.assertAlmostEqual(clock * 24 / 1 / 1e6, 576.0, places=1)


class OverlayGenerator(unittest.TestCase):
    def test_h_overlay_shape(self):
        dts = gen_overlay.render(gen_overlay.load_panel(PANELS["35H"]))
        self.assertIn('compatible = "platypus,waveshare-3in5-dsi-h";', dts)
        self.assertIn('compatible = "goodix,gt911";', dts)
        self.assertIn("reg = <0x14>;", dts)
        self.assertIn("touchscreen-size-x = <480>;", dts)
        self.assertIn("data-lanes = <0>;", dts)
        self.assertIn("target = <&cci_i2c0>;", dts)
        self.assertIn("target = <&mdss_dsi0>;", dts)
        self.assertIn("vdda-supply = <&pm4125_l5>;", dts)
        # Nothing on the 15-pin FFC can serve these.
        for absent in ("interrupts", "reset-gpios", "backlight", "power-supply"):
            self.assertNotIn(absent + " =", dts)

    def test_rejects_controller_panels(self):
        d = gen_overlay.load_panel(PANELS["35H"])
        with self.assertRaises(ValueError):
            gen_overlay.render({**d, "PANEL_CTRL_ADDR": "0x45"})

    def test_rejects_bad_lanes_and_address(self):
        d = gen_overlay.load_panel(PANELS["35H"])
        with self.assertRaises(ValueError):
            gen_overlay.render({**d, "DSI_LANES": "5"})
        with self.assertRaises(ValueError):
            gen_overlay.render({**d, "GOODIX_ADDR": "20"})

    def test_address_override_appended_by_installer(self):
        # install_panel.sh appends GOODIX_ADDR="0x5d" when the panel answers there.
        with tempfile.TemporaryDirectory() as tmp:
            p = Path(tmp) / "eff.panel"
            p.write_text(PANELS["35H"].read_text() + '\nGOODIX_ADDR="0x5d"\n')
            dts = gen_overlay.render(gen_overlay.load_panel(p))
        self.assertIn("touchscreen@5d {", dts)
        self.assertIn("reg = <0x5d>;", dts)

    @unittest.skipUnless(shutil.which("dtc"), "dtc not installed")
    def test_overlays_compile(self):
        with tempfile.TemporaryDirectory() as tmp:
            for name, path in PANELS.items():
                dts, dtbo = Path(tmp) / f"{name}.dts", Path(tmp) / f"{name}.dtbo"
                self.assertEqual(gen_overlay.main(["gen_overlay", str(path), str(dts)]), 0)
                subprocess.run(
                    ["dtc", "-@", "-q", "-I", "dts", "-O", "dtb", "-o", str(dtbo), str(dts)],
                    check=True,
                )
                self.assertGreater(dtbo.stat().st_size, 0)


if __name__ == "__main__":
    unittest.main()

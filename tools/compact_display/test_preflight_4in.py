"""Synthetic source-layout tests; these do NOT compile a target kernel module."""
import importlib.util
import json
import os
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
spec = importlib.util.spec_from_file_location('preflight_4in', HERE / 'preflight_4in.py')
preflight = importlib.util.module_from_spec(spec)
spec.loader.exec_module(preflight)

SAMPLE = '''
static const struct panel_init_cmd ws_panel_4_a_init[] = {
    _INIT_DCS_CMD(0x11),
    _INIT_DELAY_CMD(200),
    {},
};
static const struct drm_display_mode ws_panel_4_a_mode = {
    .clock = 27200,
    .hdisplay = 480,
    .vdisplay = 800,
};
static const struct ws_panel_desc ws_panel_4_inch_a_desc = {
    .init = ws_panel_4_a_init,
    .mode = &ws_panel_4_a_mode,
    .mode_flags = MIPI_DSI_MODE_VIDEO_HSE | MIPI_DSI_MODE_VIDEO |
                  MIPI_DSI_MODE_LPM | MIPI_DSI_CLOCK_NON_CONTINUOUS,
    .lanes = 2,
    .format = MIPI_DSI_FMT_RGB888,
};
static const struct of_device_id ws_panel_of_match[] = {
    { .compatible = "waveshare,4.0-dsi-touch-a", &ws_panel_4_inch_a_desc },
    {},
};
'''


class Preflight(unittest.TestCase):
    def test_recognizes_expected_source(self):
        result = preflight.inspect(SAMPLE)
        self.assertEqual((result['width'], result['height'], result['lanes']), (480, 800, 2))
        self.assertEqual(result['status'], 'SOURCE_STRUCTURE_ONLY')
        self.assertNotIn('PASS', result['not_verified'])

    def test_rejects_wrong_mode(self):
        with self.assertRaises(preflight.SourceError):
            preflight.inspect(SAMPLE.replace('.clock = 27200', '.clock = 36000'))

    def test_rejects_missing_init(self):
        with self.assertRaises(preflight.SourceError):
            preflight.inspect(SAMPLE.replace('_INIT_DCS_CMD(0x11)', '/* missing */'))

    def test_rejects_wrong_compatible(self):
        with self.assertRaises(preflight.SourceError):
            preflight.inspect(SAMPLE.replace('waveshare,4.0-dsi-touch-a', 'waveshare,4.0-dsi-touch-c'))

    def test_no_compile_without_explicit_args(self):
        with tempfile.TemporaryDirectory() as td:
            src = Path(td) / 'driver.c'
            src.write_text(SAMPLE)
            self.assertEqual(preflight.main(['--driver', str(src)]), 0)
            self.assertEqual(preflight.main(['--driver', str(src), '--compile']), 2)
            self.assertEqual(sorted(p.name for p in Path(td).iterdir()), ['driver.c'])

    def test_compile_path_isolated_with_fake_toolchain(self):
        # Exercises orchestration only. A fake make is NOT kernel compilation.
        with tempfile.TemporaryDirectory() as td:
            root = Path(td)
            driver = root / 'driver.c'
            driver.write_text(SAMPLE)
            patcher = root / 'patch.py'
            patcher.write_text(
                'import pathlib,sys\n'
                'p=pathlib.Path(sys.argv[1])\n'
                's=p.read_text().replace(\"waveshare,4.0-dsi-touch-a\", '
                '\"unoq,4-0-dsi-touch-a\")\n'
                'p.write_text(s)\n'
            )
            fakebin = root / 'bin'
            fakebin.mkdir()
            fake_make = fakebin / 'make'
            fake_make.write_text('#!/bin/sh\necho MOCK-MAKE-NOT-A-REAL-BUILD\n')
            fake_make.chmod(0o755)
            headers = root / 'fake-headers'
            headers.mkdir()
            out = root / 'build'
            original = os.environ.get('PATH', '')
            try:
                os.environ['PATH'] = str(fakebin) + os.pathsep + original
                facts = preflight.inspect(driver.read_text())
                with patch.object(preflight.os, 'geteuid', return_value=1000):
                    preflight.compile_only(driver, patcher, out, headers, facts)
            finally:
                os.environ['PATH'] = original
            self.assertEqual(driver.read_text(), SAMPLE)
            self.assertIn('MOCK-MAKE-NOT-A-REAL-BUILD', (out / 'build.log').read_text())
            self.assertIn(preflight.PRIVATE_COMPAT, (out / 'panel-waveshare-dsi-v2.c').read_text())
            self.assertFalse(json.loads((out / 'result.json').read_text())['installed'])


if __name__ == '__main__':
    unittest.main()

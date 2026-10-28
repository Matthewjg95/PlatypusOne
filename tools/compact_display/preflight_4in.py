#!/usr/bin/env python3
"""Read-only 4-DSI-TOUCH-A source audit; optional *compile-only* UNO Q kernel probe.

No install, modprobe, device-tree edit, display change, or hardware access.
The source is supplied by the operator and is NOT fetched implicitly.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import platform
import re
import shutil
import subprocess
import sys
from pathlib import Path

COMPAT = 'waveshare,4.0-dsi-touch-a'
PRIVATE_COMPAT = 'unoq,4-0-dsi-touch-a'


class SourceError(ValueError):
    pass


def _require(pattern: str, text: str, what: str, flags: int = re.S) -> re.Match[str]:
    match = re.search(pattern, text, flags)
    if match is None:
        raise SourceError(f'{what}: missing or source layout changed; do not build')
    return match


def inspect(source: str) -> dict[str, object]:
    """Follow compatible -> descriptor -> init/mode, fail closed on drift."""
    descriptor = _require(
        r'\{\s*\.compatible\s*=\s*"' + re.escape(COMPAT) + r'"\s*,\s*&(\w+)\s*\}',
        source, '4-inch A compatible',
    ).group(1)
    desc = _require(
        r'static const struct ws_panel_desc\s+' + re.escape(descriptor) +
        r'\s*=\s*\{(.*?)\n\};', source, 'panel descriptor',
    ).group(1)
    init = _require(r'\.init\s*=\s*(\w+)', desc, 'init symbol').group(1)
    mode = _require(r'\.mode\s*=\s*&?(\w+)', desc, 'mode symbol').group(1)
    lanes = int(_require(r'\.lanes\s*=\s*(\d+)', desc, 'DSI lanes').group(1))
    fmt = _require(r'\.format\s*=\s*(MIPI_DSI_FMT_\w+)', desc, 'DSI format').group(1)
    flags = _require(r'\.mode_flags\s*=\s*(.*?),\s*\.lanes', desc, 'DSI flags').group(1)
    mode_body = _require(
        r'static const struct drm_display_mode\s+' + re.escape(mode) +
        r'\s*=\s*\{(.*?)\n\};', source, 'mode body',
    ).group(1)
    init_body = _require(
        r'static const struct panel_init_cmd\s+' + re.escape(init) +
        r'\[\]\s*=\s*\{(.*?)\n\};', source, 'init body',
    ).group(1)
    clock = int(_require(r'\.clock\s*=\s*(\d+)', mode_body, 'pixel clock').group(1))
    width = int(_require(r'\.hdisplay\s*=\s*(\d+)', mode_body, 'horizontal pixels').group(1))
    height = int(_require(r'\.vdisplay\s*=\s*(\d+)', mode_body, 'vertical pixels').group(1))
    required_flags = ('MIPI_DSI_MODE_VIDEO_HSE', 'MIPI_DSI_MODE_VIDEO',
                      'MIPI_DSI_MODE_LPM', 'MIPI_DSI_CLOCK_NON_CONTINUOUS')
    if (width, height, clock, lanes, fmt) != (480, 800, 27200, 2, 'MIPI_DSI_FMT_RGB888'):
        raise SourceError('unexpected mode/lanes/format: investigate source before compiling')
    if any(flag not in flags for flag in required_flags):
        raise SourceError('DSI mode flags differ from reviewed source')
    if '_INIT_DCS_CMD' not in init_body or '_INIT_DELAY_CMD' not in init_body:
        raise SourceError('vendor initialization sequence not found')
    return {'compatible': COMPAT, 'descriptor': descriptor, 'init': init,
            'mode': mode, 'width': width, 'height': height, 'clock_khz': clock,
            'lanes': lanes, 'format': fmt, 'flags': list(required_flags),
            'init_dcs_commands': init_body.count('_INIT_DCS_CMD'),
            'source_sha256': hashlib.sha256(source.encode('utf-8')).hexdigest(),
            'status': 'SOURCE_STRUCTURE_ONLY',
            'not_verified': ['UNO Q kernel compilation', 'device-tree overlay',
                             '5 V wiring', 'touch orientation', 'physical display',
                             'CSI coexistence']}


def compile_only(driver: Path, patcher: Path, out: Path, kernel_build: Path,
                 facts: dict[str, object]) -> None:
    if hasattr(os, 'geteuid') and os.geteuid() == 0:
        raise SourceError('refusing to compile as root; no sudo is needed')
    if (out.resolve().is_relative_to(Path('/lib/modules'))
            or out.resolve().is_relative_to(Path('/boot')):
        raise SourceError('output must not be inside a system module/boot directory')
    if not patcher.is_file():
        raise SourceError(f'upstream patcher not found: {patcher}')
    if not kernel_build.is_dir():
        raise SourceError(f'kernel build headers missing: {kernel_build}')
    if not shutil.which('make'):
        raise SourceError('make is not installed')
    if out.exists() and any(out.iterdir()):
        raise SourceError(f'output directory is not empty: {out}')
    out.mkdir(parents=True, exist_ok=True)
    src = out / 'panel-waveshare-dsi-v2.c'
    shutil.copyfile(driver, src)
    patch_env = dict(os.environ,
                     PANEL_DT_COMPATIBLE=COMPAT,
                     PANEL_DT_COMPATIBLE_OUT=PRIVATE_COMPAT)
    patch = subprocess.run([sys.executable, str(patcher), str(src)],
                           text=True, capture_output=True, env=patch_env)
    (out / 'patch.log').write_text(patch.stdout + patch.stderr)
    if patch.returncode:
        raise SourceError(f'upstream patcher failed (see {out / "patch.log"})')
    patched = src.read_text()
    if patched.count(f'.compatible = "{PRIVATE_COMPAT}"') != 1:
        raise SourceError('private compatible not unique in patched source')
    if re.search(r'\.compatible\s*=\s*"waveshare,', patched):
        raise SourceError('untrimmed Waveshare compatible remains; aborting')
    (out / 'Makefile').write_text('obj-m += panel-waveshare-dsi-v2.o\n')
    result = subprocess.run(['make', '-C', str(kernel_build), f'M={out.resolve()}', 'modules'],
                            text=True, capture_output=True)
    (out / 'build.log').write_text(result.stdout + result.stderr)
    record = {**facts, 'kernel': platform.uname().release,
              'kernel_build': str(kernel_build.resolve()),
              'patcher_sha256': hashlib.sha256(patcher.read_bytes()).hexdigest(),
              'compile_status': 'PASS' if result.returncode == 0 else 'FAIL',
              'installed': False}
    (out / 'result.json').write_text(json.dumps(record, indent=2) + '\n')
    if result.returncode:
        raise SourceError(f'kernel compile failed (see {out / "build.log"}')


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--driver', type=Path, required=True,
                        help='local Raspberry Pi panel-waveshare-dsi-v2.c source')
    parser.add_argument('--compile', action='store_true', help='compile only; never install')
    parser.add_argument('--patcher', type=Path, help='upstream tools/patch-waveshare-panel.py')
    parser.add_argument('--out', type=Path, help='new or empty build directory')
    parser.add_argument('--kernel-build', type=Path,
                        default=Path('/lib/modules') / platform.uname().release / 'build')
    args = parser.parse_args(argv)
    try:
        if not args.driver.is_file():
            raise SourceError(f'driver source missing: {args.driver}')
        facts = inspect(args.driver.read_text())
        if args.compile:
            if args.patcher is None or args.out is None:
                raise SourceError('--compile requires --patcher and --out')
            compile_only(args.driver, args.patcher, args.out, args.kernel_build, facts)
            facts['compile_status'] = 'PASS'
        print(json.dumps(facts, indent=2))
        return 0
    except (OSError, SourceError) as exc:
        print(f'4-inch preflight BLOCKED: {exc}', file=sys.stderr)
        return 2


if __name__ == '__main__':
    raise SystemExit(main())

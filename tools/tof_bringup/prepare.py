#!/usr/bin/env python3
"""Fetch pinned ST ULD sources; preserve notices, exclude competing Wire wrapper."""
import pathlib
import shutil
import subprocess
import tempfile

REV = 'a93a9d6796f2a74835a4088f225daec343153c62'
ROOT = pathlib.Path(__file__).resolve().parents[2]
DEST = ROOT / 'firmware/tof_bringup/src/uld'

def main():
    if DEST.exists():
        raise SystemExit(f'{DEST} exists; remove it explicitly before regenerating')
    with tempfile.TemporaryDirectory() as tmp:
        subprocess.run(['git', 'clone', 'https://github.com/stm32duino/VL53L8CX.git', tmp], check=True)
        subprocess.run(['git', '-C', tmp, 'checkout', '--detach', REV], check=True)
        DEST.mkdir(parents=True)
        for src in (pathlib.Path(tmp) / 'src').glob('vl53l8cx_*'):
            if src.suffix in ('.c', '.h'):
                shutil.copy2(src, DEST / src.name)
        shutil.copy2(pathlib.Path(tmp) / 'LICENSE', DEST / 'LICENSE')
        (DEST / 'REVISION').write_text(REV + '\n')
    print(f'Prepared {DEST}')

if __name__ == '__main__':
    main()

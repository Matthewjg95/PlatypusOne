#!/usr/bin/env python3
"""Entry point: python3 tools/camera_characterize/camera_characterize.py <command> ...

See camchar/cli.py and docs/hardware/CAMERA_CHARACTERIZATION.md.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from camchar.cli import main  # noqa: E402

if __name__ == "__main__":
    sys.exit(main())

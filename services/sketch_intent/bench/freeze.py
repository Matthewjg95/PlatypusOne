"""Record (or check) the frozen resolver: source hash + default parameters.

The held-out set may only be scored against a frozen resolver. ``python3
bench/freeze.py`` writes bench/FREEZE.json; benchmark.py refuses to label a
held-out run "held-out" unless the current code still matches it.
"""

from __future__ import annotations

import hashlib
import json
import sys
from dataclasses import asdict
from datetime import UTC, datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FREEZE = ROOT / "bench" / "FREEZE.json"
sys.path.insert(0, str(ROOT))

from sketch_intent import RESOLVER_VERSION  # noqa: E402
from sketch_intent.contract import Params  # noqa: E402

# Presentation-only modules: changing them cannot change a proposal.
NOT_FROZEN = {"overlay.py", "cli.py", "__main__.py"}


def source_hash() -> str:
    h = hashlib.sha256()
    for f in sorted((ROOT / "sketch_intent").glob("*.py")):
        if f.name in NOT_FROZEN:
            continue
        h.update(f.name.encode() + b"\0" + f.read_bytes() + b"\0")
    return h.hexdigest()


def current() -> dict:
    return {"resolver_version": RESOLVER_VERSION, "source_sha256": source_hash(), "params": asdict(Params())}


def check() -> tuple[bool, dict | None]:
    if not FREEZE.exists():
        return False, None
    frozen = json.loads(FREEZE.read_text())
    cur = current()
    same = all(frozen[k] == cur[k] for k in ("resolver_version", "source_sha256", "params"))
    return same, frozen


if __name__ == "__main__":
    rec = current()
    rec["frozen_at_utc"] = datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
    rec["note"] = (
        "Parameters and resolver code frozen after development on the DEV fixtures only. "
        "Held-out fixtures, the ShadowScan renders and the scan-0053 washer were first run "
        "after this point; any later change to frozen code invalidates the held-out label."
    )
    FREEZE.write_text(json.dumps(rec, indent=2) + "\n")
    print(json.dumps(rec, indent=2))

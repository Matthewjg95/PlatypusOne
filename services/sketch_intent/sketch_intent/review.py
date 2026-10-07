"""Human review of a proposal: accept / reject items, never edit the original.

Reviewable items are primitives (g*) and constraints (c*). Rules:
  - "unsupported" items cannot be accepted (there is no analytic geometry);
  - accepting a "rejected_by_evidence" constraint needs an explicit override
    note: the human is asserting intent the evidence contradicts, and that
    is recorded as such;
  - every decision records who, when (UTC) and an optional note.

The output is a NEW file; the reviewed proposal keeps the original content
plus ``review`` and a ``parent_sha256`` of the file it was derived from.
"""

from __future__ import annotations

import hashlib
import json
from datetime import UTC, datetime
from pathlib import Path
from typing import Any


class ReviewError(ValueError):
    pass


def _items(proposal: dict[str, Any]) -> dict[str, dict[str, Any]]:
    items = {p["id"]: p for p in proposal["primitives"]}
    items.update({c["id"]: c for c in proposal["constraints"]})
    return items


def apply_decisions(
    proposal: dict[str, Any],
    accept: list[str],
    reject: list[str],
    *,
    by: str,
    accept_proposed: bool = False,
    override_note: str | None = None,
    note: str | None = None,
    now: str | None = None,
) -> dict[str, Any]:
    if not by.strip():
        raise ReviewError("a reviewer name is required (--by)")
    items = _items(proposal)
    ts = now or datetime.now(UTC).strftime("%Y-%m-%dT%H:%M:%SZ")
    decided = {d["item"]: d for d in proposal["review"]["decisions"]}
    overlap = set(accept) & set(reject)
    if overlap:
        raise ReviewError(f"both accepted and rejected: {sorted(overlap)}")
    todo: list[tuple[str, str]] = [(i, "accepted") for i in accept] + [
        (i, "rejected") for i in reject
    ]
    if accept_proposed:
        todo += [
            (i, "accepted")
            for i, it in items.items()
            if it["decision"] == "proposed"
            and i not in decided
            and i not in reject
            and i not in accept
        ]
    for item, decision in todo:
        if item not in items:
            raise ReviewError(f"unknown item {item!r}")
        it = items[item]
        rec: dict[str, Any] = {
            "item": item,
            "decision": decision,
            "by": by,
            "at": ts,
            "resolver_decision": it["decision"],
        }
        if note:
            rec["note"] = note
        if decision == "accepted":
            if it["decision"] == "unsupported":
                raise ReviewError(
                    f"{item} is unsupported (no analytic geometry) and cannot be accepted"
                )
            if it["decision"] == "rejected_by_evidence":
                if not override_note:
                    raise ReviewError(
                        f"{item} was rejected by the evidence; accepting it needs --override-note "
                        "explaining the design intent"
                    )
                rec["override"] = override_note
        decided[item] = rec
    out = json.loads(json.dumps(proposal))  # deep copy
    out["review"]["decisions"] = sorted(decided.values(), key=lambda d: d["item"])
    pending = [
        i
        for i, it in items.items()
        if it["decision"] in ("proposed", "question") and i not in decided
    ]
    out["review"]["state"] = "complete" if not pending else "partial"
    out["review"]["undecided"] = sorted(pending)
    return out


def review_file(src: Path, dst: Path, **kw) -> dict[str, Any]:
    if dst.exists():
        raise ReviewError(f"{dst} exists; reviews are written to a new file")
    raw = src.read_bytes()
    proposal = json.loads(raw)
    out = apply_decisions(proposal, **kw)
    out["review"]["parent_sha256"] = hashlib.sha256(raw).hexdigest()
    dst.write_text(json.dumps(out, indent=2) + "\n")
    return out


def accepted_sets(proposal: dict[str, Any]) -> tuple[set[str], set[str]]:
    acc = {d["item"] for d in proposal["review"]["decisions"] if d["decision"] == "accepted"}
    rej = {d["item"] for d in proposal["review"]["decisions"] if d["decision"] == "rejected"}
    return acc, rej

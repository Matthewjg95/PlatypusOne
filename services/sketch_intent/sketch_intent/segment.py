"""Unconstrained primitive hypotheses for one contour loop.

1. Gaps: consecutive points farther apart than the gap rule are MISSING
   EVIDENCE. No fit ever spans a gap.
2. Outliers: a point far (> outlier_factor x tol) from the line through its
   well-fitting neighbours is excluded from fits and reported. It stays in
   the evidence.
3. Whole-loop circle test (closed, gap-free loops).
4. Candidate breakpoints by Douglas-Peucker at epsilon = tol.
5. Optimal segmentation: the fewest primitives (line cost 1, arc cost 1.15)
   such that every primitive fits its points within the evidence band
   (max <= tol, rms <= tol/2). Ties break on total squared residual.
6. Adjacent primitives that fit jointly are merged (also across the loop
   start). Runs of short or ill-fitting pieces become FREEFORM: preserved
   as evidence and never turned into invented geometry.
7. Short corner pieces are checked against their alternatives (sharp
   corner / chamfer line / fillet arc). More than one fitting alternative
   is reported as an ambiguity.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

import numpy as np

from .contract import Loop, Params
from .fitting import CircleFit, LineFit, cross2, fit_circle, fit_line

LINE_COST, ARC_COST = 1.0, 1.15


@dataclass
class Primitive:
    pid: str
    kind: str  # line | arc | circle | freeform
    loop_id: str
    idx: np.ndarray  # global point indices used for the fit, in contour order
    fit: LineFit | CircleFit | None
    alternatives: list[dict[str, Any]] = field(default_factory=list)
    ambiguous: bool = False
    notes: list[str] = field(default_factory=list)


@dataclass
class LoopAnalysis:
    loop: Loop
    gaps: list[dict[str, Any]]
    outliers: list[int]
    primitives: list[Primitive]
    cyclic: bool  # closed and gap-free: last primitive joins the first
    junction_gaps: dict[int, dict[str, Any]]  # junction after primitive i -> gap info


def _fits(f: LineFit | CircleFit | None, tol: float) -> bool:
    return f is not None and f.max_abs <= tol and f.rms <= tol / 2


def _dp(pts: np.ndarray, eps: float) -> list[int]:
    """Douglas-Peucker on an open polyline; returns kept positions (sorted)."""
    keep = {0, len(pts) - 1}
    stack = [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        if b - a < 2:
            continue
        p, q = pts[a], pts[b]
        seg = q - p
        L = np.linalg.norm(seg)
        mid = pts[a + 1 : b]
        d = np.linalg.norm(mid - p, axis=1) if L == 0 else np.abs(cross2(seg, mid - p)) / L
        k = int(np.argmax(d))
        if d[k] > eps:
            m = a + 1 + k
            keep.add(m)
            stack.append((a, m))
            stack.append((m, b))
    return sorted(keep)


def _gaps(P: np.ndarray, closed: bool, outliers: set[int], prm: Params) -> dict[int, int]:
    """Gaps between consecutive INLIERS: {inlier index -> next inlier index}."""
    n = len(P)
    inl = [i for i in range(n) if i not in outliers]
    pairs = list(zip(inl, inl[1:] + ([inl[0]] if closed else []), strict=False))
    steps = np.array([np.linalg.norm(P[j] - P[i]) for i, j in pairs])
    med = float(np.median(steps)) if len(steps) else 0.0
    thr = max(prm.gap_factor * med, prm.gap_min_px)
    gaps = {i: j for (i, j), st in zip(pairs, steps, strict=True) if st > thr}
    if not closed:
        gaps[inl[-1]] = -1  # the open end is always a gap
    return gaps


def _outliers(P: np.ndarray, closed: bool, prm: Params) -> list[int]:
    """Points far off the line through their well-fitting neighbours.

    Runs before gap detection: a spike makes two long steps, which must not be
    mistaken for missing evidence.
    """
    n = len(P)
    tol = prm.tolerance_px
    w = prm.outlier_window
    flagged: set[int] = set()
    for _ in range(3):
        new = set()
        for i in range(n):
            nb = []
            for off in list(range(-w, -1)) + list(range(2, w + 1)):
                j = i + off
                if not closed and not 0 <= j < n:
                    continue
                j %= n
                if j not in flagged:
                    nb.append(j)
            if len(nb) < 4:
                continue
            f = fit_line(P[nb])
            if (
                f.rms <= 0.5 * tol
                and abs(float(f.distances(P[i : i + 1])[0])) > prm.outlier_factor * tol
            ):
                new.add(i)
        if new == flagged:
            break
        flagged = new
    return sorted(flagged)


def _chains(
    n: int, gaps: dict[int, int], outliers: set[int], closed: bool
) -> tuple[list[list[int]], bool]:
    """Split the loop into gap-free chains of inlier indices (contour order)."""
    if closed and not gaps:
        return [[i for i in range(n) if i not in outliers]], True
    start = (max(gaps) + 1) % n if gaps else 0
    chains: list[list[int]] = []
    cur: list[int] = []
    for k in range(n):
        i = (start + k) % n
        if i not in outliers:
            cur.append(i)
        if i in gaps:
            if cur:
                chains.append(cur)
            cur = []
    if cur:
        chains.append(cur)
    return [c for c in chains if len(c) >= 2], False


class _Segmenter:
    def __init__(self, P: np.ndarray, prm: Params, loop_size: float):
        self.P = P
        self.prm = prm
        self.tol = prm.tolerance_px
        self.rmax = prm.max_radius_factor * loop_size
        self.cache: dict[tuple, tuple] = {}

    def line(self, idx: list[int]) -> LineFit:
        return fit_line(self.P[idx])

    def arc(self, idx: list[int]) -> CircleFit | None:
        if len(idx) < self.prm.min_arc_points:
            return None
        f = fit_circle(self.P[idx])
        if f is None or f.radius > self.rmax:
            return None
        if f.span_rad < np.radians(self.prm.min_arc_span_deg):
            return None
        return f

    def best(self, idx: list[int]) -> tuple[str, Any, float] | None:
        """Cheapest primitive that fits idx: (kind, fit, cost)."""
        key = (idx[0], idx[-1], len(idx))
        if key in self.cache:
            return self.cache[key]
        out = None
        lf = self.line(idx)
        if _fits(lf, self.tol):
            out = ("line", lf, LINE_COST)
        else:
            af = self.arc(idx)
            if _fits(af, self.tol):
                out = ("arc", af, ARC_COST)
        self.cache[key] = out
        return out


def _segment_chain(
    chain: list[int], seg: _Segmenter, cyclic: bool
) -> list[tuple[str, list[int], Any]]:
    P = seg.P
    if cyclic:
        cpts = P[chain]
        start = int(np.argmax(np.linalg.norm(cpts - cpts.mean(axis=0), axis=1)))
        chain = chain[start:] + chain[:start]
        seq = chain + [chain[0]]
    else:
        seq = chain
    bp = _dp(P[seq], seg.tol)
    k = len(bp)
    max_span = 60
    INF = (float("inf"), float("inf"))
    best: list[tuple[float, float]] = [INF] * k
    prev: list[int] = [-1] * k
    choice: list[Any] = [None] * k
    best[0] = (0.0, 0.0)
    for c in range(1, k):
        for a in range(max(0, c - max_span), c):
            if best[a] == INF:
                continue
            idx = seq[bp[a] : bp[c] + 1]
            r = seg.best(idx)
            if r is None:
                if c != a + 1:
                    continue
                lf = seg.line(idx)  # adjacent pieces always connect (flagged later)
                r = ("line", lf, LINE_COST + 0.5)
            kind, f, cost = r
            cand = (best[a][0] + cost, best[a][1] + f.rms**2 * len(idx))
            if cand < best[c]:
                best[c], prev[c], choice[c] = cand, a, (kind, idx, f)
    out = []
    c = k - 1
    while c > 0:
        out.append(choice[c])
        c = prev[c]
    out.reverse()
    return out


def _merge_adjacent(prims: list[tuple[str, list[int], Any]], seg: _Segmenter, cyclic: bool):
    changed = True
    while changed and len(prims) > 1:
        changed = False
        n = len(prims)
        pairs = list(range(n - 1)) + ([n - 1] if cyclic and n > 2 else [])
        for i in pairs:
            j = (i + 1) % n
            a, b = prims[i], prims[j]
            idx = a[1] + b[1][1:] if a[1][-1] == b[1][0] else a[1] + b[1]
            r = seg.best(idx)
            if r is not None:
                merged = (r[0], idx, r[1])
                prims = [merged] + prims[1:-1] if j == 0 else prims[:i] + [merged] + prims[j + 1 :]
                changed = True
                break
    return prims


def analyze_loop(loop: Loop, prm: Params, counter: list[int]) -> LoopAnalysis:
    P = loop.points
    n = len(P)
    tol = prm.tolerance_px
    outl = _outliers(P, loop.closed, prm)
    gaps = _gaps(P, loop.closed, set(outl), prm)
    gap_info = [
        {"after_point": int(i), "to_point": int(j), "length_px": float(np.linalg.norm(P[j] - P[i]))}
        if j >= 0
        else {
            "after_point": int(i),
            "to_point": None,
            "length_px": None,
            "note": "open contour end",
        }
        for i, j in sorted(gaps.items())
    ]
    chains, cyclic = _chains(n, gaps, set(outl), loop.closed)
    size = float(np.linalg.norm(P.max(axis=0) - P.min(axis=0)))
    seg = _Segmenter(P, prm, size)

    def new_id() -> str:
        counter[0] += 1
        return f"g{counter[0]}"

    prims: list[Primitive] = []
    junction_gaps: dict[int, dict[str, Any]] = {}

    if cyclic:
        idx = chains[0]
        cf = fit_circle(P[idx])
        if cf is not None and _fits(cf, tol) and cf.span_rad >= np.radians(prm.min_circle_span_deg):
            return LoopAnalysis(
                loop,
                gap_info,
                outl,
                [Primitive(new_id(), "circle", loop.loop_id, np.array(idx), cf)],
                True,
                {},
            )

    for ci, chain in enumerate(chains):
        if len(chain) < 3:
            continue
        pieces = _segment_chain(chain, seg, cyclic)
        pieces = _merge_adjacent(pieces, seg, cyclic)
        chain_prims = [
            Primitive(new_id(), kind, loop.loop_id, np.array(idx), f) for kind, idx, f in pieces
        ]
        _mark_freeform(chain_prims, P, prm)
        prims.extend(_collapse_freeform(chain_prims))
        if not cyclic:
            junction_gaps[len(prims) - 1] = {"chain": ci, "kind": "evidence_gap"}

    prims = _smooth_freeform(prims, P, prm, cyclic, junction_gaps)
    _corner_alternatives(prims, P, prm, cyclic, junction_gaps)
    return LoopAnalysis(loop, gap_info, outl, prims, cyclic, junction_gaps)


SMOOTH_TURN_DEG = 15.0


def _end_tangents(p: Primitive, P: np.ndarray) -> tuple[np.ndarray, np.ndarray, int]:
    """Unit tangents at the start and end along travel, and the arc's turn sign."""
    pts = P[p.idx]
    if p.kind == "line":
        d = p.fit.direction * (1 if (pts[-1] - pts[0]) @ p.fit.direction >= 0 else -1)
        return d, d, 0
    c = p.fit.center
    a = np.unwrap(np.arctan2(pts[:, 1] - c[1], pts[:, 0] - c[0]))
    sgn = 1 if a[-1] >= a[0] else -1

    def tan(q):
        v = q - c
        return sgn * np.array([-v[1], v[0]]) / np.linalg.norm(v)

    return tan(pts[0]), tan(pts[-1]), sgn


def _smooth_freeform(prims, P, prm, cyclic, junction_gaps):
    """Smooth runs that look like a freeform curve, not design geometry.

    A run of >= 4 primitives joined without corners (turn < 15 deg) is a
    freeform suspect when it reverses curvature (convex and concave arcs), or
    has >= 2 arc-to-arc smooth junctions. Rounded rectangles, slots and
    filleted plates alternate arcs with straight lines and pass. The arc/line
    approximation is kept as a recorded alternative, never as proposed geometry.
    """
    n = len(prims)
    if n < 4 or any(p.kind not in ("line", "arc") for p in prims):
        return prims
    tans = [_end_tangents(p, P) for p in prims]
    smooth = []
    for i in range(n if cyclic else n - 1):
        j = (i + 1) % n
        cosang = float(np.clip(tans[i][1] @ tans[j][0], -1, 1))
        smooth.append(i not in junction_gaps and np.degrees(np.arccos(cosang)) < SMOOTH_TURN_DEG)
    # maximal runs of smooth junctions (cyclic aware)
    if cyclic and all(smooth):
        runs = [list(range(n))]
    else:
        runs, cur = [], []
        order = list(range(n))
        if cyclic:
            k = smooth.index(False)
            order = order[k + 1 :] + order[: k + 1]
        for i in order:
            cur.append(i)
            if i >= len(smooth) or not smooth[i]:
                runs.append(cur)
                cur = []
        if cur:
            runs.append(cur)
    flagged: set[int] = set()
    # Rule 2: runs of >= 4 short pieces (< 12 % of the loop length each) whose
    # corners are not all near 90 deg (castellations / teeth stay analytic).
    perim = sum(_length(p, P) for p in prims)
    turn = []
    for i in range(n if cyclic else n - 1):
        j = (i + 1) % n
        turn.append(float(np.degrees(np.arccos(np.clip(tans[i][1] @ tans[j][0], -1, 1)))))
    short = [_length(p, P) < 0.12 * perim for p in prims]
    i = 0
    while i < n:
        j = i
        while j < n and short[j]:
            j += 1
        if j - i >= 4:
            inner = [turn[k] for k in range(i, j - 1) if k < len(turn)]
            square = all(abs(t - 90) < 10 for t in inner) and all(
                prims[k].kind == "line" for k in range(i, j)
            )
            if not square:
                flagged.update(range(i, j))
        i = max(j, i + 1)
    for run in runs:
        if len(run) < 4:
            continue
        signs = {tans[i][2] for i in run if prims[i].kind == "arc"}
        arc_arc = sum(
            1
            for a, b in zip(
                run, run[1:] + ([run[0]] if len(run) == n and cyclic else []), strict=False
            )
            if prims[a].kind == "arc" and prims[b].kind == "arc"
        )
        if {1, -1} <= signs or arc_arc >= 2:
            flagged.update(run)
    if not flagged:
        return prims
    for i in flagged:
        p = prims[i]
        p.alternatives = [
            {
                "interpretation": f"{p.kind}_piece_of_smooth_approximation",
                "max_px": p.fit.max_abs,
                "rms_px": p.fit.rms,
                "fits": True,
            }
        ]
        p.kind = "freeform"
        p.notes.append(
            "run of short or smoothly blended pieces with no line/arc structure: likely a "
            "freeform curve; the line/arc approximation is kept as an alternative only"
        )
    return _collapse_freeform(prims)


def _length(p: Primitive, P: np.ndarray) -> float:
    pts = P[p.idx]
    return float(np.sum(np.linalg.norm(np.diff(pts, axis=0), axis=1)))


def _mark_freeform(prims: list[Primitive], P: np.ndarray, prm: Params) -> None:
    """Ill-fitting pieces, and runs of >= 3 short pieces, are freeform evidence."""
    tol = prm.tolerance_px
    short = [p.kind == "line" and _length(p, P) < 3 * tol for p in prims]
    for p in prims:
        if p.kind in ("line", "arc") and not _fits(p.fit, tol):
            p.kind = "freeform"
            p.notes.append("no line or arc fits within the evidence band")
    i = 0
    while i < len(prims):
        j = i
        while j < len(prims) and short[j]:
            j += 1
        if j - i >= 3:
            for p in prims[i:j]:
                p.kind = "freeform"
                p.notes.append("run of short pieces: shape not explained by a line or arc")
        i = max(j, i + 1)


def _collapse_freeform(prims: list[Primitive]) -> list[Primitive]:
    out: list[Primitive] = []
    for p in prims:
        if out and p.kind == "freeform" and out[-1].kind == "freeform":
            prev = out[-1]
            prev.idx = np.concatenate([prev.idx, p.idx[1:] if prev.idx[-1] == p.idx[0] else p.idx])
            prev.notes = sorted(set(prev.notes + p.notes))
        else:
            out.append(p)
    for p in out:
        if p.kind == "freeform":
            p.fit = None
    return out


def _corner_alternatives(prims, P, prm, cyclic, junction_gaps) -> None:
    """For a short piece between two lines: sharp corner, chamfer, fillet."""
    tol = prm.tolerance_px
    n = len(prims)
    for i, p in enumerate(prims):
        if p.kind not in ("line", "arc") or n < 3:
            continue
        a = prims[i - 1] if (i > 0 or cyclic) else None
        b = prims[(i + 1) % n] if (i < n - 1 or cyclic) else None
        if a is None or b is None or a.kind != "line" or b.kind != "line":
            continue
        if (i - 1) % n in junction_gaps or i in junction_gaps:
            continue
        if _length(p, P) > 0.5 * min(_length(a, P), _length(b, P)):
            continue  # not a corner piece
        pts = P[p.idx]
        alts = []
        da = np.abs(a.fit.distances(pts))
        db = np.abs(b.fit.distances(pts))
        sharp_max = float(np.max(np.minimum(da, db)))
        alts.append(
            {"interpretation": "sharp_corner", "max_px": sharp_max, "fits": sharp_max <= tol}
        )
        lf = fit_line(pts)
        alts.append(
            {
                "interpretation": "chamfer_line",
                "max_px": lf.max_abs,
                "rms_px": lf.rms,
                "fits": _fits(lf, tol),
            }
        )
        cf = fit_circle(pts)
        if cf is not None:
            alts.append(
                {
                    "interpretation": "fillet_arc",
                    "radius_px": cf.radius,
                    "max_px": cf.max_abs,
                    "rms_px": cf.rms,
                    "fits": _fits(cf, tol) and len(pts) >= 3,
                }
            )
        fitting = [x for x in alts if x["fits"]]
        p.alternatives = alts
        if len(fitting) > 1:
            p.ambiguous = True
            p.notes.append(
                "corner region fits "
                + " and ".join(x["interpretation"] for x in fitting)
                + "; the evidence does not decide between them"
            )

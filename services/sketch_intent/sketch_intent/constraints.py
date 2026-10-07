"""Constraint hypotheses: each one is a CONSTRAINED refit compared with the
unconstrained fits of the same evidence.

Decision rule (identical for every constraint type), with tol = tolerance_px:
  proposed              constrained fit still within the evidence band
                        (every point <= tol, RMS <= tol/2) AND not markedly
                        worse than the unconstrained fits (RMS <= rms_inflation
                        x unconstrained, or + 0.25 tol): the correction is
                        within what the evidence cannot distinguish
  question              inside the band but the evidence distinguishes it
                        (RMS inflated), or constrained max <= question_factor x tol: plausible
                        design intent, but it moves geometry beyond the
                        evidence band, so a human must decide
  rejected_by_evidence  anything beyond: the evidence contradicts it

Every hypothesis records what it would change (rotation, endpoint shift,
radius change) in pixels and, when calibrated, millimetres. A decision within
20 % of a threshold is flagged ``near_threshold``.

Tested: parallel, perpendicular (line pairs within angle_window_deg of 0 / 90),
coincident (junction of adjacent primitives), tangent (arc next to a line),
equal_radius, concentric (arcs / circles).
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

import numpy as np

from .contract import Params
from .fitting import (
    CircleFit,
    LineFit,
    circle_fixed_radius,
    cross2,
    fit_line,
    line_from_normal,
    scatter,
)
from .segment import LoopAnalysis, Primitive


@dataclass
class Hypothesis:
    cid: str
    ctype: str
    entities: list[str]
    decision: str
    measured: dict[str, Any]
    correction: dict[str, Any]
    constrained_fit: dict[str, Any]
    reason: str
    flags: list[str] = field(default_factory=list)


def _pooled(*pairs: tuple[float, int]) -> float:
    """RMS over several point sets from (rms, n) pairs."""
    n = sum(k for _, k in pairs)
    return float(np.sqrt(sum(r * r * k for r, k in pairs) / max(n, 1)))


def decide(
    max_px: float,
    rms_px: float,
    prm: Params,
    rms_free: float | None = None,
    rms_constrained: float | None = None,
) -> tuple[str, list[str]]:
    """Objective: orthogonal point-to-geometry distance of the same inlier points.

    rms_free: RMS of the unconstrained fits of those points, when they exist;
    rms_constrained: the matching constrained RMS (defaults to rms_px). A
    constraint that fits inside the band but inflates the RMS beyond
    rms_inflation x the free fit is distinguishable by the evidence, so it is
    a question rather than a proposal.
    """
    tol = prm.tolerance_px
    flags = []
    rc = rms_px if rms_constrained is None else rms_constrained
    inflated = rms_free is not None and rc > max(
        prm.rms_inflation * rms_free, rms_free + 0.05 * tol
    )
    if max_px <= tol and rms_px <= tol / 2 and inflated:
        d = "question"
        flags.append("evidence_distinguishes")
    elif max_px <= tol and rms_px <= tol / 2:
        d = "proposed"
        if max_px > 0.8 * tol or rms_px > 0.4 * tol:
            flags.append("near_threshold")
    elif max_px <= prm.question_factor * tol:
        d = "question"
        if max_px < 1.25 * tol:
            flags.append("near_threshold")
    else:
        d = "rejected_by_evidence"
        if max_px < 1.25 * prm.question_factor * tol:
            flags.append("near_threshold")
    return d, flags


def _rot90(v: np.ndarray) -> np.ndarray:
    return np.array([-v[1], v[0]])


def _angle_between(d1: np.ndarray, d2: np.ndarray) -> float:
    """Acute angle between two undirected lines, degrees in [0, 90]."""
    c = abs(float(np.dot(d1, d2)))
    return float(np.degrees(np.arccos(min(1.0, c))))


def _min_eig(m: np.ndarray) -> np.ndarray:
    w, v = np.linalg.eigh(m)
    return v[:, 0]


class Context:
    """Per-proposal lookup: primitive by id, its points, scale for mm reporting."""

    def __init__(self, loops: list[LoopAnalysis], prm: Params, mm_per_px: float | None):
        self.loops = loops
        self.prm = prm
        self.k = mm_per_px
        self.prims: dict[str, Primitive] = {}
        self.points: dict[str, np.ndarray] = {}
        for la in loops:
            for p in la.primitives:
                self.prims[p.pid] = p
                self.points[p.pid] = la.loop.points[p.idx]

    def mm(self, px: float | None) -> float | None:
        return None if px is None or self.k is None else px * self.k

    def lines(self) -> list[Primitive]:
        return [p for p in self.prims.values() if p.kind == "line"]

    def rounds(self) -> list[Primitive]:
        return [p for p in self.prims.values() if p.kind in ("arc", "circle")]


def _fitdict(ctx: Context, fits: dict[str, LineFit | CircleFit]) -> dict[str, Any]:
    return {
        pid: {
            "max_px": f.max_abs,
            "rms_px": f.rms,
            "max_mm": ctx.mm(f.max_abs),
            "rms_mm": ctx.mm(f.rms),
        }
        for pid, f in fits.items()
    }


def _line_rotation(ctx: Context, p: Primitive, new: LineFit) -> dict[str, Any]:
    old = p.fit
    ang = _angle_between(old.direction, new.direction)
    t0, t1 = old.extent(ctx.points[p.pid])
    shift = 0.5 * (t1 - t0) * np.sin(np.radians(ang))
    return {
        "rotation_deg": ang,
        "endpoint_shift_px": float(shift),
        "endpoint_shift_mm": ctx.mm(float(shift)),
    }


def line_pairs(ctx: Context, counter: list[int]) -> list[Hypothesis]:
    out = []
    prm = ctx.prm
    lines = ctx.lines()
    for i in range(len(lines)):
        for j in range(i + 1, len(lines)):
            a, b = lines[i], lines[j]
            ang = _angle_between(a.fit.direction, b.fit.direction)
            for ctype, target in (("parallel", 0.0), ("perpendicular", 90.0)):
                dev = abs(ang - target)
                if dev > prm.angle_window_deg:
                    continue
                pa, pb = ctx.points[a.pid], ctx.points[b.pid]
                _, sa = scatter(pa)
                _, sb = scatter(pb)
                if ctype == "parallel":
                    n = _min_eig(sa + sb)
                    fa, fb = line_from_normal(pa, n), line_from_normal(pb, n)
                else:
                    n = _min_eig(sa - sb)
                    fa, fb = line_from_normal(pa, n), line_from_normal(pb, _rot90(n))
                mx = max(fa.max_abs, fb.max_abs)
                rms = max(fa.rms, fb.rms)
                d, flags = decide(mx, rms, prm, max(a.fit.rms, b.fit.rms))
                counter[0] += 1
                out.append(
                    Hypothesis(
                        f"c{counter[0]}",
                        ctype,
                        [a.pid, b.pid],
                        d,
                        {"angle_deg": ang, "deviation_from_ideal_deg": dev},
                        {a.pid: _line_rotation(ctx, a, fa), b.pid: _line_rotation(ctx, b, fb)},
                        _fitdict(ctx, {a.pid: fa, b.pid: fb}),
                        f"{ctype}: measured {ang:.2f} deg ({dev:.2f} deg from ideal); "
                        f"constrained max residual {mx:.2f} px vs tolerance "
                        f"{prm.tolerance_px:.2f} px",
                        flags,
                    )
                )
    return out


def _intersect_lines(f1: LineFit, f2: LineFit) -> np.ndarray | None:
    A = np.column_stack([f1.direction, -f2.direction])
    if abs(np.linalg.det(A)) < 1e-9:
        return None
    t = np.linalg.solve(A, f2.centroid - f1.centroid)
    return f1.centroid + t[0] * f1.direction


def _line_circle(f: LineFit, c: CircleFit, near: np.ndarray) -> np.ndarray | None:
    d = f.centroid - c.center
    b = float(np.dot(f.direction, d))
    cc = float(np.dot(d, d)) - c.radius**2
    disc = b * b - cc
    if disc < 0:
        return None
    s = np.sqrt(disc)
    pts = [f.centroid + (-b + s) * f.direction, f.centroid + (-b - s) * f.direction]
    return min(pts, key=lambda p: np.linalg.norm(p - near))


def _circle_circle(c1: CircleFit, c2: CircleFit, near: np.ndarray) -> np.ndarray | None:
    d = float(np.linalg.norm(c2.center - c1.center))
    if d == 0 or d > c1.radius + c2.radius or d < abs(c1.radius - c2.radius):
        return None
    a = (c1.radius**2 - c2.radius**2 + d * d) / (2 * d)
    h = np.sqrt(max(c1.radius**2 - a * a, 0.0))
    u = (c2.center - c1.center) / d
    m = c1.center + a * u
    pts = [m + h * _rot90(u), m - h * _rot90(u)]
    return min(pts, key=lambda p: np.linalg.norm(p - near))


def line_circle_near(f: LineFit, c: CircleFit, near: np.ndarray, tol: float) -> np.ndarray | None:
    """Where a line meets a circle.

    Near-tangent (|distance(center, line) - r| <= tol): the junction of a
    tangent blend is ill-conditioned as an intersection (the two roots spread
    by sqrt(2 r delta)), so it is the tangent point: the foot of the centre on
    the line, pushed onto the circle so arc ends stay exactly on it.
    Otherwise: the real intersection nearest `near`, or None.
    """
    d = f.centroid - c.center
    dist = abs(float(d @ f.normal))
    if abs(dist - c.radius) <= tol:
        b = float(np.dot(f.direction, d))
        foot = d - b * f.direction
        return c.center + foot / np.linalg.norm(foot) * c.radius
    return _line_circle(f, c, near)


def tangency_gap(f: LineFit, c: CircleFit) -> float:
    return abs(abs(float((f.centroid - c.center) @ f.normal)) - c.radius)


def junction_point(
    a: Primitive, b: Primitive, near: np.ndarray, tol: float = 0.0
) -> np.ndarray | None:
    fa, fb = a.fit, b.fit
    if a.kind == "line" and b.kind == "line":
        return _intersect_lines(fa, fb)
    if a.kind == "line" and b.kind == "arc":
        return line_circle_near(fa, fb, near, tol)
    if a.kind == "arc" and b.kind == "line":
        return line_circle_near(fb, fa, near, tol)
    if a.kind == "arc" and b.kind == "arc":
        return _circle_circle(fa, fb, near)
    return None


def junctions(ctx: Context, counter: list[int]) -> list[Hypothesis]:
    """Coincident hypotheses for each junction of adjacent analytic primitives."""
    out = []
    prm = ctx.prm
    for la in ctx.loops:
        prims = la.primitives
        n = len(prims)
        if n < 2:
            continue
        last = n if la.loop.closed else n - 1
        for i in range(last):
            a, b = prims[i], prims[(i + 1) % n]
            counter[0] += 1
            cid = f"c{counter[0]}"
            gap = la.junction_gaps.get(i)
            P = la.loop.points
            ev = P[a.idx[-1]] if gap is None else 0.5 * (P[a.idx[-1]] + P[b.idx[0]])
            if a.kind not in ("line", "arc") or b.kind not in ("line", "arc"):
                out.append(
                    Hypothesis(
                        cid,
                        "coincident",
                        [a.pid, b.pid],
                        "unsupported",
                        {},
                        {},
                        {},
                        "junction with freeform evidence: no analytic vertex",
                    )
                )
                continue
            if a.kind == "line" and b.kind == "line":
                ang = _angle_between(a.fit.direction, b.fit.direction)
                if ang < prm.min_corner_angle_deg and gap is not None:
                    # the same edge continuing across missing evidence?
                    pts = np.concatenate([ctx.points[a.pid], ctx.points[b.pid]])
                    j = fit_line(pts)
                    ea, eb = P[a.idx[-1]], P[b.idx[0]]
                    g = float(np.linalg.norm(eb - ea))
                    _, flags = decide(j.max_abs, j.rms, prm)
                    out.append(
                        Hypothesis(
                            cid,
                            "collinear",
                            [a.pid, b.pid],
                            "question"
                            if j.max_abs <= prm.question_factor * prm.tolerance_px
                            else "rejected_by_evidence",
                            {"angle_deg": ang, "gap_px": g, "gap_mm": ctx.mm(g)},
                            {"bridged_px": g, "bridged_mm": ctx.mm(g)},
                            {"joint": {"max_px": j.max_abs, "rms_px": j.rms}},
                            f"one straight edge across {g:.1f} px of missing contour? joint "
                            "line max residual "
                            f"{j.max_abs:.2f} px; the bridged part has no evidence",
                            flags + ["across_gap"],
                        )
                    )
                    continue
                if ang < prm.min_corner_angle_deg:
                    out.append(
                        Hypothesis(
                            cid,
                            "coincident",
                            [a.pid, b.pid],
                            "question",
                            {"angle_deg": ang},
                            {},
                            {},
                            f"near-collinear junction ({ang:.2f} deg): vertex is ill-conditioned; "
                            "merge or keep as a step?",
                            ["ill_conditioned"],
                        )
                    )
                    continue
            if {a.kind, b.kind} == {"line", "arc"}:
                ln, arc = (a, b) if a.kind == "line" else (b, a)
                delta = tangency_gap(ln.fit, arc.fit)
                if delta <= prm.question_factor * prm.tolerance_px:
                    d, flags = decide(delta, delta / 2, prm)
                    out.append(
                        Hypothesis(
                            cid,
                            "coincident",
                            [a.pid, b.pid],
                            d,
                            {"tangency_gap_px": delta, "tangency_gap_mm": ctx.mm(delta)},
                            {},
                            {},
                            f"line and arc meet as a tangent blend: separation {delta:.2f} px "
                            "(the blend point along the curve is not measurable)",
                            flags + ["tangent_junction"],
                        )
                    )
                    continue
            q = junction_point(a, b, ev, prm.tolerance_px)
            if q is None:
                out.append(
                    Hypothesis(
                        cid,
                        "coincident",
                        [a.pid, b.pid],
                        "rejected_by_evidence",
                        {},
                        {},
                        {},
                        "fitted primitives do not meet: no vertex without moving them",
                    )
                )
                continue
            if gap is not None:
                ea, eb = P[a.idx[-1]], P[b.idx[0]]
                span = float(max(np.linalg.norm(q - ea), np.linalg.norm(q - eb)))
                out.append(
                    Hypothesis(
                        cid,
                        "coincident",
                        [a.pid, b.pid],
                        "question",
                        {
                            "gap_px": float(np.linalg.norm(eb - ea)),
                            "gap_mm": ctx.mm(float(np.linalg.norm(eb - ea))),
                        },
                        {"vertex_extrapolated_px": span, "vertex_extrapolated_mm": ctx.mm(span)},
                        {},
                        f"vertex inferred across {np.linalg.norm(eb - ea):.1f} px of missing "
                        "contour "
                        f"(extrapolated {span:.1f} px): no evidence supports or contradicts it",
                        ["across_gap"],
                    )
                )
                continue
            dist = float(np.linalg.norm(q - ev))
            d, flags = decide(dist, dist / 2, prm)
            out.append(
                Hypothesis(
                    cid,
                    "coincident",
                    [a.pid, b.pid],
                    d,
                    {"vertex_to_evidence_px": dist, "vertex_to_evidence_mm": ctx.mm(dist)},
                    {"vertex_px": [float(q[0]), float(q[1])]},
                    {},
                    f"junction vertex lies {dist:.2f} px from the traced junction point",
                    flags,
                )
            )
    return out


def fillet_center(
    l1: LineFit, l2: LineFit, r: float, near: np.ndarray
) -> tuple[np.ndarray, float] | None:
    """(center, radius) of the circle tangent to both lines nearest `near`.

    Non-parallel lines: radius r kept, center at the offset-line intersection.
    (Near-)parallel lines (a slot end): tangency to both forces the radius to
    half their spacing and the center onto their mid-line; the position along
    the lines is kept from the arc fit.
    """
    if abs(float(cross2(l1.direction, l2.direction))) < np.sin(np.radians(5)):
        n = l1.normal
        o1 = float(l1.centroid @ n)
        o2 = float(l2.centroid @ n)
        mid = 0.5 * (o1 + o2)
        c = near - (float(near @ n) - mid) * n
        return c, 0.5 * abs(o2 - o1)
    best = None
    for s1 in (-1, 1):
        for s2 in (-1, 1):
            o1 = LineFit(l1.centroid + s1 * r * l1.normal, l1.direction, l1.normal, 0, 0, 0)
            o2 = LineFit(l2.centroid + s2 * r * l2.normal, l2.direction, l2.normal, 0, 0, 0)
            c = _intersect_lines(o1, o2)
            if c is not None and (
                best is None or np.linalg.norm(c - near) < np.linalg.norm(best - near)
            ):
                best = c
    return None if best is None else (best, r)


def _golden(f, lo: float, hi: float, iters: int = 60) -> float:
    """Deterministic 1-D minimiser (golden section) on [lo, hi]."""
    g = (np.sqrt(5) - 1) / 2
    a, b = lo, hi
    c, d = b - g * (b - a), a + g * (b - a)
    fc, fd = f(c), f(d)
    for _ in range(iters):
        if fc < fd:
            b, d, fd = d, c, fc
            c = b - g * (b - a)
            fc = f(c)
        else:
            a, c, fc = c, d, fd
            d = a + g * (b - a)
            fd = f(d)
    return 0.5 * (a + b)


def tangent_fit(lines: list[LineFit], arc: CircleFit, pts: np.ndarray, fix_radius: bool = False):
    """Best circle tangent to the given line(s): (center, radius, kind) or None.

    The constrained hypothesis is the least-squares circle WITHIN the tangent
    family (one free parameter left), not the free fit pushed to tangency:
    a short arc's free radius is poorly determined, and comparing against a
    pushed fit would reject real fillets.
      two lines, not parallel   free: radius (center on the offset-line meet)
      two parallel lines        radius fixed at half the spacing; free: the
                                position along the mid-line
      one line                  free: radius (center keeps its along-line place)
    fix_radius keeps arc.radius (when an equal-radius constraint owns it).
    """

    def sse(c, r):
        return float(np.sum((np.linalg.norm(pts - c, axis=1) - r) ** 2))

    r0 = arc.radius
    if len(lines) == 2:
        l1, l2 = lines
        if abs(float(cross2(l1.direction, l2.direction))) < np.sin(np.radians(5)):
            c0, rr = fillet_center(l1, l2, r0, arc.center)
            u = l1.direction
            t = _golden(lambda t: sse(c0 + t * u, rr), -rr, rr)
            return c0 + t * u, rr, "tangent (semicircle between parallel lines)"
        if fix_radius:
            fc = fillet_center(l1, l2, r0, arc.center)
            return None if fc is None else (fc[0], r0, "tangent (fillet between two lines)")

        def at(r):
            fc = fillet_center(l1, l2, r, arc.center)
            return fc[0] if fc is not None else arc.center

        r = _golden(lambda r: sse(at(r), r), 0.5 * r0, 1.5 * r0)
        fc = fillet_center(l1, l2, r, arc.center)
        return None if fc is None else (fc[0], r, "tangent (fillet between two lines)")
    ln = lines[0]
    dist = float((arc.center - ln.centroid) @ ln.normal)
    side = np.sign(dist) or 1.0

    def at1(r):
        return arc.center - (dist - side * r) * ln.normal

    r = r0 if fix_radius else _golden(lambda r: sse(at1(r), r), 0.5 * r0, 1.5 * r0)
    return at1(r), r, "tangent (arc to line)"


def tangents(ctx: Context, counter: list[int]) -> list[Hypothesis]:
    out = []
    prm = ctx.prm
    for la in ctx.loops:
        prims = la.primitives
        n = len(prims)
        for i, p in enumerate(prims):
            if p.kind != "arc":
                continue
            nb = []
            prev = i - 1 if i > 0 else (n - 1 if la.cyclic else None)
            nxt = i + 1 if i < n - 1 else (0 if la.cyclic else None)
            # junction key = index of the primitive the junction follows
            for j, key in ((prev, prev), (nxt, i)):
                if j is None or j == i or prims[j].kind != "line" or key in la.junction_gaps:
                    continue
                nb.append(prims[j])
            if not nb:
                continue
            pts = ctx.points[p.pid]
            tf = tangent_fit([q.fit for q in nb], p.fit, pts)
            if tf is None:
                continue
            c, rr, kind = tf
            res = np.linalg.norm(pts - c, axis=1) - rr
            mx = float(np.max(np.abs(res)))
            rms = float(np.sqrt(np.mean(res**2)))
            d, flags = decide(mx, rms, prm, p.fit.rms)
            move = float(np.linalg.norm(c - p.fit.center))
            counter[0] += 1
            out.append(
                Hypothesis(
                    f"c{counter[0]}",
                    "tangent",
                    [p.pid] + [q.pid for q in nb],
                    d,
                    {"center_px": [float(p.fit.center[0]), float(p.fit.center[1])]},
                    {
                        "center_move_px": move,
                        "center_move_mm": ctx.mm(move),
                        "constrained_radius_px": float(rr),
                        "radius_change_px": float(rr - p.fit.radius),
                    },
                    {
                        p.pid: {
                            "max_px": mx,
                            "rms_px": rms,
                            "max_mm": ctx.mm(mx),
                            "rms_mm": ctx.mm(rms),
                        }
                    },
                    f"{kind}: arc center moves {move:.2f} px, radius {p.fit.radius:.2f} -> "
                    f"{rr:.2f} px; "
                    f"constrained max residual {mx:.2f} px",
                    flags,
                )
            )
    return out


def round_pairs(ctx: Context, counter: list[int]) -> list[Hypothesis]:
    out = []
    prm = ctx.prm
    rs = ctx.rounds()
    for i in range(len(rs)):
        for j in range(i + 1, len(rs)):
            a, b = rs[i], rs[j]
            fa, fb = a.fit, b.fit
            pa, pb = ctx.points[a.pid], ctx.points[b.pid]
            rel = abs(fa.radius - fb.radius) / max(fa.radius, fb.radius)
            if rel <= prm.equal_radius_rel:
                r = (fa.radius * len(pa) + fb.radius * len(pb)) / (len(pa) + len(pb))
                ca = circle_fixed_radius(pa, r, fa.center)
                cb = circle_fixed_radius(pb, r, fb.center)
                mx, rms = max(ca.max_abs, cb.max_abs), max(ca.rms, cb.rms)
                pooled_c = _pooled((ca.rms, len(pa)), (cb.rms, len(pb)))
                pooled_f = _pooled((fa.rms, len(pa)), (fb.rms, len(pb)))
                d, flags = decide(mx, rms, prm, pooled_f, pooled_c)
                counter[0] += 1
                out.append(
                    Hypothesis(
                        f"c{counter[0]}",
                        "equal_radius",
                        [a.pid, b.pid],
                        d,
                        {
                            "radius_px": [fa.radius, fb.radius],
                            "radius_mm": [ctx.mm(fa.radius), ctx.mm(fb.radius)],
                            "relative_difference": rel,
                            "pooled_rms_free_px": pooled_f,
                            "pooled_rms_constrained_px": pooled_c,
                        },
                        {
                            "common_radius_px": r,
                            "common_radius_mm": ctx.mm(r),
                            "radius_change_px": [r - fa.radius, r - fb.radius],
                        },
                        _fitdict(ctx, {a.pid: ca, b.pid: cb}),
                        f"equal radius: {fa.radius:.2f} vs {fb.radius:.2f} px ({100 * rel:.1f} "
                        "% apart); "
                        f"constrained max residual {mx:.2f} px",
                        flags,
                    )
                )
            dc = float(np.linalg.norm(fa.center - fb.center))
            if dc <= prm.concentric_factor * prm.tolerance_px * 2:
                c = (fa.center * len(pa) + fb.center * len(pb)) / (len(pa) + len(pb))
                res_a = np.linalg.norm(pa - c, axis=1)
                res_b = np.linalg.norm(pb - c, axis=1)
                ra_, rb_ = res_a - res_a.mean(), res_b - res_b.mean()
                mx = float(max(np.abs(ra_).max(), np.abs(rb_).max()))
                rms = float(max(np.sqrt(np.mean(ra_**2)), np.sqrt(np.mean(rb_**2))))
                d, flags = decide(mx, rms, prm, max(fa.rms, fb.rms))
                counter[0] += 1
                out.append(
                    Hypothesis(
                        f"c{counter[0]}",
                        "concentric",
                        [a.pid, b.pid],
                        d,
                        {"center_distance_px": dc, "center_distance_mm": ctx.mm(dc)},
                        {"common_center_px": [float(c[0]), float(c[1])]},
                        {
                            a.pid: {"max_px": float(np.abs(ra_).max())},
                            b.pid: {"max_px": float(np.abs(rb_).max())},
                        },
                        f"concentric: centers {dc:.2f} px apart; constrained max residual "
                        f"{mx:.2f} px",
                        flags,
                    )
                )
    return out


def all_hypotheses(ctx: Context) -> list[Hypothesis]:
    counter = [0]
    return (
        line_pairs(ctx, counter)
        + junctions(ctx, counter)
        + tangents(ctx, counter)
        + round_pairs(ctx, counter)
    )

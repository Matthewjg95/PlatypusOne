"""Geometry for a chosen set of constraints, and its discrepancy from evidence.

The same solver produces both the PROPOSED geometry (constraints decided
"proposed") and, after review, the ACCEPTED geometry (constraints a human
accepted). Constraints are applied jointly:

  lines      parallel / perpendicular form direction classes (union-find with
             parity). One direction per class minimises the summed squared
             orthogonal residual of all its lines: the base normal is the
             smallest-eigenvalue eigenvector of A - B, where A / B sum the
             scatter matrices of the lines parallel / perpendicular to the
             base. Contradictory sets are reported as conflicts, not applied.
  rounds     equal_radius groups share the point-weighted mean radius (centres
             refitted); concentric groups share the weighted mean centre;
             tangent moves an arc centre to tangency with its line(s).
  vertices   each accepted coincident junction becomes the intersection of
             the two resolved primitives; a junction without one stays OPEN
             and the loop cannot be exported as a closed profile.

Discrepancy = orthogonal distance from each evidence point to the resolved
geometry. It is a fit diagnostic, not a measurement accuracy.
"""

from __future__ import annotations

from typing import Any

import numpy as np

from .constraints import (
    Context,
    Hypothesis,
    _intersect_lines,
    _rot90,
    line_circle_near,
    tangent_fit,
)
from .fitting import CircleFit, LineFit, circle_fixed_radius, fit_line, line_from_normal, scatter


class _UF:
    def __init__(self):
        self.parent: dict[str, str] = {}
        self.parity: dict[str, int] = {}

    def find(self, x: str) -> tuple[str, int]:
        if x not in self.parent:
            self.parent[x], self.parity[x] = x, 0
        if self.parent[x] == x:
            return x, 0
        root, p = self.find(self.parent[x])
        self.parent[x] = root
        self.parity[x] ^= p
        return root, self.parity[x]

    def union(self, a: str, b: str, rel: int) -> bool:
        ra, pa = self.find(a)
        rb, pb = self.find(b)
        if ra == rb:
            return (pa ^ pb) == rel
        self.parent[rb] = ra
        self.parity[rb] = pa ^ pb ^ rel
        return True


def _circle_circle(c1: CircleFit, c2: CircleFit, near: np.ndarray) -> np.ndarray | None:
    d = float(np.linalg.norm(c2.center - c1.center))
    if d == 0 or d > c1.radius + c2.radius or d < abs(c1.radius - c2.radius):
        return None
    a = (c1.radius**2 - c2.radius**2 + d * d) / (2 * d)
    h = np.sqrt(max(c1.radius**2 - a * a, 0.0))
    u = (c2.center - c1.center) / d
    m = c1.center + a * u
    return min(
        [m + h * _rot90(u), m - h * _rot90(u)], key=lambda p: float(np.linalg.norm(p - near))
    )


def _sweep_sign(pts: np.ndarray, center: np.ndarray) -> int:
    a = np.unwrap(np.arctan2(pts[:, 1] - center[1], pts[:, 0] - center[0]))
    return 1 if a[-1] >= a[0] else -1


def solve(ctx: Context, applied: list[Hypothesis]) -> dict[str, Any]:
    prm = ctx.prm
    tol = prm.tolerance_px
    fits: dict[str, Any] = {pid: p.fit for pid, p in ctx.prims.items()}
    conflicts: list[dict[str, Any]] = []

    # --- line direction classes -------------------------------------------
    uf = _UF()
    for h in applied:
        if h.ctype in ("parallel", "perpendicular"):
            rel = 0 if h.ctype == "parallel" else 1
            if not uf.union(h.entities[0], h.entities[1], rel):
                conflicts.append(
                    {
                        "constraint": h.cid,
                        "reason": f"{h.ctype} contradicts other accepted direction constraints",
                    }
                )
    classes: dict[str, list[tuple[str, int]]] = {}
    for pid in list(uf.parent):
        root, par = uf.find(pid)
        classes.setdefault(root, []).append((pid, par))
    for members in classes.values():
        if len(members) < 2:
            continue
        A = np.zeros((2, 2))
        B = np.zeros((2, 2))
        for pid, par in members:
            _, s = scatter(ctx.points[pid])
            if par == 0:
                A += s
            else:
                B += s
        w, v = np.linalg.eigh(A - B)
        n0 = v[:, 0]
        for pid, par in members:
            fits[pid] = line_from_normal(ctx.points[pid], n0 if par == 0 else _rot90(n0))

    # --- rounds -------------------------------------------------------------
    eq = _UF()
    co = _UF()
    for h in applied:
        if h.ctype == "equal_radius":
            eq.union(h.entities[0], h.entities[1], 0)
        if h.ctype == "concentric":
            co.union(h.entities[0], h.entities[1], 0)
    in_equal: set[str] = set()
    for groups_uf, kind in ((eq, "equal"), (co, "concentric")):
        groups: dict[str, list[str]] = {}
        for pid in list(groups_uf.parent):
            groups.setdefault(groups_uf.find(pid)[0], []).append(pid)
        for members in groups.values():
            if len(members) < 2:
                continue
            ns = np.array([len(ctx.points[m]) for m in members], float)
            if kind == "equal":
                r = float(np.dot(ns, [fits[m].radius for m in members]) / ns.sum())
                for m in members:
                    fits[m] = circle_fixed_radius(ctx.points[m], r, fits[m].center)
                    in_equal.add(m)
            else:
                c = (
                    np.sum([fits[m].center * n for m, n in zip(members, ns, strict=True)], axis=0)
                    / ns.sum()
                )
                for m in members:
                    pts = ctx.points[m]
                    r = (
                        fits[m].radius
                        if m in in_equal
                        else float(np.mean(np.linalg.norm(pts - c, axis=1)))
                    )
                    res = np.linalg.norm(pts - c, axis=1) - r
                    fits[m] = CircleFit(
                        c,
                        r,
                        float(np.sqrt(np.mean(res**2))),
                        float(np.abs(res).max()),
                        len(pts),
                        fits[m].span_rad,
                    )

    for h in applied:
        if h.ctype != "tangent":
            continue
        arc = h.entities[0]
        lines = [fits[e] for e in h.entities[1:]]
        f = fits[arc]
        tf = tangent_fit(lines, f, ctx.points[arc], fix_radius=arc in in_equal)
        c, r = (tf[0], tf[1]) if tf is not None else (None, f.radius)
        if c is not None:
            pts = ctx.points[arc]
            res = np.linalg.norm(pts - c, axis=1) - r
            fits[arc] = CircleFit(
                c,
                r,
                float(np.sqrt(np.mean(res**2))),
                float(np.abs(res).max()),
                len(pts),
                f.span_rad,
            )

    # --- vertices and entities --------------------------------------------
    coincident = {tuple(h.entities) for h in applied if h.ctype == "coincident"}
    collinear = {tuple(h.entities) for h in applied if h.ctype == "collinear"}
    for a_id, b_id in collinear:  # one line through both pieces
        j = fit_line(np.concatenate([ctx.points[a_id], ctx.points[b_id]]))
        fits[a_id] = line_from_normal(ctx.points[a_id], j.normal)
        fits[b_id] = line_from_normal(ctx.points[b_id], j.normal)
        # same offset: shift both onto the joint line
        for pid in (a_id, b_id):
            f = fits[pid]
            off = float((j.centroid - f.centroid) @ j.normal)
            fits[pid] = LineFit(
                f.centroid + off * j.normal,
                j.direction,
                j.normal,
                float(np.sqrt(np.mean(j.distances(ctx.points[pid]) ** 2))),
                float(np.abs(j.distances(ctx.points[pid])).max()),
                f.n,
            )
    loops_out = []
    for la in ctx.loops:
        prims = la.primitives
        n = len(prims)
        P = la.loop.points
        start_v: dict[int, np.ndarray] = {}
        end_v: dict[int, np.ndarray] = {}
        open_j: list[dict[str, Any]] = []
        last = n if la.loop.closed else n - 1
        for i in range(last if n > 1 else 0):
            j = (i + 1) % n
            a, b = prims[i], prims[j]
            if (a.pid, b.pid) in collinear:
                fa = fits[a.pid]
                gm = 0.5 * (P[a.idx[-1]] + P[b.idx[0]])
                q = fa.centroid + ((gm - fa.centroid) @ fa.direction) * fa.direction
                end_v[i] = q
                start_v[j] = q
                continue
            if (a.pid, b.pid) not in coincident:
                open_j.append(
                    {"after": a.pid, "before": b.pid, "reason": "no accepted coincident constraint"}
                )
                continue
            near = P[a.idx[-1]]
            fa, fb = fits[a.pid], fits[b.pid]
            if a.kind == "line" and b.kind == "line":
                q = _intersect_lines(fa, fb)
            elif a.kind == "line" and b.kind == "arc":
                q = line_circle_near(fa, fb, near, tol)
            elif a.kind == "arc" and b.kind == "line":
                q = line_circle_near(fb, fa, near, tol)
            elif a.kind == "arc" and b.kind == "arc":
                q = _circle_circle(fa, fb, near)
            else:
                q = None
            if q is None:
                open_j.append(
                    {
                        "after": a.pid,
                        "before": b.pid,
                        "reason": "resolved primitives do not intersect",
                    }
                )
                continue
            end_v[i] = q
            start_v[j] = q
        if not la.loop.closed and n:
            open_j.append({"after": prims[-1].pid, "before": None, "reason": "open contour end"})

        entities = []
        sq_all: list[float] = []
        for i, p in enumerate(prims):
            pts = P[p.idx]
            f = fits[p.pid]
            e: dict[str, Any] = {"pid": p.pid, "kind": p.kind}
            if p.kind == "line":
                s = start_v.get(i, f.centroid + ((pts[0] - f.centroid) @ f.direction) * f.direction)
                t = end_v.get(i, f.centroid + ((pts[-1] - f.centroid) @ f.direction) * f.direction)
                e.update(start=s.tolist(), end=t.tolist())
                res = f.distances(pts)
            elif p.kind == "arc":

                def onc(q, f=f):
                    v = q - f.center
                    return f.center + v / np.linalg.norm(v) * f.radius

                s = start_v.get(i, onc(pts[0]))
                t = end_v.get(i, onc(pts[-1]))
                e.update(
                    start=s.tolist(),
                    end=t.tolist(),
                    center=f.center.tolist(),
                    radius=f.radius,
                    sweep_sign_image=_sweep_sign(pts, f.center),
                )
                res = f.distances(pts)
            elif p.kind == "circle":
                e.update(center=f.center.tolist(), radius=f.radius)
                res = f.distances(pts)
            else:
                e.update(points=pts.tolist(), note="freeform evidence, not analytic geometry")
                res = np.zeros(len(pts))
            mx = float(np.abs(res).max()) if len(res) else 0.0
            rms = float(np.sqrt(np.mean(res**2))) if len(res) else 0.0
            sq_all.extend((res**2).tolist())
            e["discrepancy"] = {
                "max_px": mx,
                "rms_px": rms,
                "max_mm": ctx.mm(mx),
                "rms_mm": ctx.mm(rms),
                "exceeds_tolerance": mx > tol,
            }
            entities.append(e)
        exportable = (
            bool(entities)
            and not open_j
            and all(e["kind"] in ("line", "arc", "circle") for e in entities)
            and (len(entities) == 1) == (entities[0]["kind"] == "circle")
        )
        rms_all = float(np.sqrt(np.mean(sq_all))) if sq_all else 0.0
        loops_out.append(
            {
                "loop_id": la.loop.loop_id,
                "role": la.loop.role,
                "entities": entities,
                "open_junctions": open_j,
                "closed_profile": exportable,
                "discrepancy": {
                    "max_px": max((e["discrepancy"]["max_px"] for e in entities), default=0.0),
                    "rms_px": rms_all,
                    "max_mm": ctx.mm(
                        max((e["discrepancy"]["max_px"] for e in entities), default=0.0)
                    ),
                    "rms_mm": ctx.mm(rms_all),
                    "outliers_excluded": len(la.outliers),
                },
            }
        )
    return {"loops": loops_out, "conflicts": conflicts}

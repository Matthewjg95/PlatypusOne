"""SVG overlay: original evidence versus proposed (or accepted) geometry.

Drawn in image pixel coordinates (SVG is y-down like the image):
  grey polyline + dots   evidence contour, exactly as given
  red x                  outliers (excluded from fits, still evidence)
  orange dashes          gaps in the evidence (nothing was fitted across)
  coloured geometry      green: within half the tolerance; amber: within
                         tolerance; red: exceeds tolerance
  magenta dashed         primitives awaiting a question
  purple                 freeform evidence (no analytic geometry)
  thin black whiskers    residuals magnified x(whisker_gain) at sampled
                         points, so a 1 px discrepancy is visible
Side panel: every constraint with its decision and correction.
"""

from __future__ import annotations

import html
import math
from typing import Any

import numpy as np

COL = {
    "ok": "#1a9850",
    "warn": "#d08c00",
    "bad": "#d73027",
    "question": "#c51b7d",
    "freeform": "#7b3294",
}
DEC = {
    "proposed": "#1a9850",
    "question": "#c51b7d",
    "rejected_by_evidence": "#888888",
    "unsupported": "#7b3294",
}


def _f(v: float) -> str:
    return f"{v:.2f}"


def _arc_path(e: dict[str, Any]) -> str:
    s, t, c, r = e["start"], e["end"], e["center"], e["radius"]
    a0 = math.atan2(s[1] - c[1], s[0] - c[0])
    a1 = math.atan2(t[1] - c[1], t[0] - c[0])
    sweep = (a1 - a0) % (2 * math.pi) if e["sweep_sign_image"] > 0 else -((a0 - a1) % (2 * math.pi))
    large = 1 if abs(sweep) > math.pi else 0
    sf = 1 if e["sweep_sign_image"] > 0 else 0
    return f"M {_f(s[0])} {_f(s[1])} A {_f(r)} {_f(r)} 0 {large} {sf} {_f(t[0])} {_f(t[1])}"


def _el(tag: str, text: str | None = None, **attrs: Any) -> str:
    """One SVG element; attribute names use _ for -, floats are rounded."""
    parts = []
    for key, v in attrs.items():
        name = key.rstrip("_").replace("_", "-")
        val = _f(v) if isinstance(v, float | np.floating) else str(v)
        parts.append(f'{name}="{html.escape(val, quote=True)}"')
    a = " ".join(parts)
    return f"<{tag} {a}/>" if text is None else f"<{tag} {a}>{html.escape(text)}</{tag}>"


def _path(pts) -> str:
    return " ".join(("M" if i == 0 else "L") + f" {_f(q[0])} {_f(q[1])}" for i, q in enumerate(pts))


def _foot(e: dict[str, Any], p: np.ndarray) -> np.ndarray:
    if e["kind"] == "line":
        a, b = np.array(e["start"]), np.array(e["end"])
        d = (b - a) / max(np.linalg.norm(b - a), 1e-12)
        return a + ((p - a) @ d) * d
    c = np.array(e["center"])
    v = p - c
    return c + v / max(np.linalg.norm(v), 1e-12) * e["radius"]


def render(
    proposal: dict[str, Any],
    geometry: dict[str, Any] | None = None,
    title: str = "proposed",
    whisker_gain: float = 10.0,
) -> str:
    geometry = geometry or proposal["proposed_geometry"]
    tol = proposal["parameters"]["tolerance_px"]
    k = proposal["input"]["scale_mm_per_px"]
    qitems = {a for q in proposal["questions"] for a in q["about"]}
    prim = {p["id"]: p for p in proposal["primitives"]}
    ev_loops = proposal["evidence"]["loops"]
    allpts = np.concatenate([np.array(lp["points_px"]) for lp in ev_loops])
    lo, hi = allpts.min(axis=0), allpts.max(axis=0)
    span = float(max(hi - lo))
    pad = 0.08 * span + 10
    stroke = max(span / 600, 0.4)
    vx, vy = float(lo[0] - pad), float(lo[1] - pad - 0.06 * span)
    vw = float(hi[0] - lo[0] + 2 * pad)
    vh = float(hi[1] - lo[1] + 2 * pad + 0.06 * span)
    total_w = vw * 1.75
    fs = 0.022 * span
    rows = len(proposal["constraints"]) + len(geometry["loops"]) + 4
    canvas_h = max(vh, fs * (3 + 1.25 * rows))  # the side panel may need more room than the part
    out = [
        '<svg xmlns="http://www.w3.org/2000/svg" '
        f'viewBox="{_f(vx)} {_f(vy)} {_f(total_w)} {_f(canvas_h)}" '
        'width="1400" font-family="sans-serif">',
        _el("rect", x=vx, y=vy, width=total_w, height=canvas_h, fill="#fff"),
    ]
    tx = vx + pad * 0.3
    unit = f" ({tol * k:.3g} mm)" if k else " (UNCALIBRATED: pixels only)"
    hdr = f"{title} - proposal {proposal['proposal_id']} - tolerance {tol:g} px{unit}"
    out.append(_el("text", hdr, x=tx, y=vy + fs * 1.4, font_size=fs))

    # evidence, exactly as given
    for lp in ev_loops:
        P = np.array(lp["points_px"])
        gaps = {g["after_point"] for g in lp["gaps"]}
        d = []
        pen = False
        for i in range(len(P) + (1 if lp["closed"] else 0)):
            q = P[i % len(P)]
            d.append(("L" if pen else "M") + f" {_f(q[0])} {_f(q[1])}")
            pen = (i % len(P)) not in gaps
        out.append(_el("path", d=" ".join(d), fill="none", stroke="#9e9e9e", stroke_width=stroke))
        out += [
            _el("circle", cx=float(q[0]), cy=float(q[1]), r=stroke * 0.9, fill="#bdbdbd")
            for q in P[:: max(1, len(P) // 400)]
        ]
        for g in lp["gaps"]:
            if g.get("length_px") is None:
                continue
            a, b = P[g["after_point"]], P[(g["after_point"] + 1) % len(P)]
            out.append(
                _el(
                    "line",
                    x1=float(a[0]),
                    y1=float(a[1]),
                    x2=float(b[0]),
                    y2=float(b[1]),
                    stroke="#f57c00",
                    stroke_width=stroke * 2,
                    stroke_dasharray=f"{_f(stroke * 4)} {_f(stroke * 3)}",
                )
            )
        s = stroke * 4
        for i in lp["outliers"]:
            x, y = P[i]
            d = f"M {_f(x - s)} {_f(y - s)} L {_f(x + s)} {_f(y + s)} "
            d += f"M {_f(x - s)} {_f(y + s)} L {_f(x + s)} {_f(y - s)}"
            out.append(_el("path", d=d, stroke=COL["bad"], stroke_width=stroke * 1.2))

    # geometry, coloured by discrepancy, with magnified residual whiskers
    loops_ev = {lp["loop_id"]: np.array(lp["points_px"]) for lp in ev_loops}
    w = stroke * 2.2
    for lo_g in geometry["loops"]:
        P = loops_ev[lo_g["loop_id"]]
        for e in lo_g["entities"]:
            dmax = e["discrepancy"]["max_px"]
            col = COL["ok"] if dmax <= tol / 2 else COL["warn"] if dmax <= tol else COL["bad"]
            style = {"fill": "none", "stroke": col, "stroke_width": w}
            if e["pid"] in qitems:
                style.update(
                    stroke=COL["question"], stroke_dasharray=f"{_f(stroke * 6)} {_f(stroke * 3)}"
                )
                col = COL["question"]
            if e["kind"] == "line":
                a, b = e["start"], e["end"]
                out.append(_el("line", x1=a[0], y1=a[1], x2=b[0], y2=b[1], **style))
                mid = (np.array(a) + np.array(b)) / 2
            elif e["kind"] == "arc":
                out.append(_el("path", d=_arc_path(e), **style))
                mid = np.array(e["start"])
            elif e["kind"] == "circle":
                c = e["center"]
                out.append(_el("circle", cx=c[0], cy=c[1], r=e["radius"], **style))
                mid = np.array(c) + np.array([e["radius"], 0])
            else:
                pts = e["points"]
                out.append(
                    _el(
                        "path",
                        d=_path(pts),
                        fill="none",
                        stroke=COL["freeform"],
                        stroke_width=w * 1.4,
                        opacity="0.6",
                    )
                )
                mid = np.array(pts[len(pts) // 2])
            label_y = float(mid[1] - stroke * 3)
            out.append(
                _el("text", e["pid"], x=float(mid[0]), y=label_y, font_size=fs * 0.8, fill=col)
            )
            if e["kind"] in ("line", "arc", "circle"):
                idx = prim[e["pid"]]["point_indices"]
                for i in idx[:: max(1, len(idx) // 25)]:
                    p = P[i]
                    f = _foot(e, p)
                    t = f + (p - f) * whisker_gain
                    out.append(
                        _el(
                            "line",
                            x1=float(f[0]),
                            y1=float(f[1]),
                            x2=float(t[0]),
                            y2=float(t[1]),
                            stroke="#000",
                            stroke_width=stroke * 0.6,
                        )
                    )

    # legend + scale bar
    ly = vy + vh - pad * 0.35
    if k:
        bar_mm = 10 ** math.floor(math.log10(span * k / 4))
        bar = _el(
            "line", x1=tx, y1=ly, x2=tx + bar_mm / k, y2=ly, stroke="#000", stroke_width=stroke * 2
        )
        out.append(bar)
        out.append(_el("text", f"{bar_mm:g} mm", x=tx, y=ly - stroke * 3, font_size=fs * 0.8))
    legend = (
        "grey: evidence | red x: outlier | orange: gap | green/amber/red: geometry vs tolerance | "
        f"magenta: question | purple: freeform | whiskers: residual x{whisker_gain:g}"
    )
    out.append(_el("text", legend, x=tx, y=vy + fs * 2.7, font_size=fs * 0.7, fill="#555"))

    # side panel
    x = vx + vw + pad * 0.2
    y = vy + fs * 3
    out.append(_el("text", "Constraints", x=x, y=y, font_size=fs, font_weight="bold"))
    applied = set(geometry.get("applied_constraints", []))
    small = fs * 0.75
    for c in proposal["constraints"]:
        y += fs * 1.25
        mark = "*" if c["id"] in applied else " "
        txt = f"{mark}{c['id']} {c['type']} {'/'.join(c['entities'])}: {c['decision']}"
        m = c["measured"]
        if c["type"] in ("parallel", "perpendicular"):
            txt += f" dev {m['deviation_from_ideal_deg']:.2f} deg"
        elif c["type"] == "coincident" and "vertex_to_evidence_px" in m:
            txt += f" {m['vertex_to_evidence_px']:.2f} px"
        elif c["type"] == "equal_radius":
            txt += f" {100 * m['relative_difference']:.1f}% apart"
        elif c["type"] == "tangent":
            txt += f" moves {c['correction']['center_move_px']:.2f} px"
        out.append(_el("text", txt, x=x, y=y, font_size=small, fill=DEC.get(c["decision"], "#000")))
    y += fs * 1.6
    out.append(_el("text", "* applied to the drawn geometry", x=x, y=y, font_size=small))
    for lo_g in geometry["loops"]:
        y += fs * 1.25
        dd = lo_g["discrepancy"]
        mm = f" / {dd['max_mm']:.3f} mm" if dd.get("max_mm") is not None else ""
        txt = (
            f"{lo_g['loop_id']} ({lo_g['role']}): max discrepancy {dd['max_px']:.2f} px{mm}; "
            f"closed profile: {lo_g['closed_profile']}"
        )
        out.append(_el("text", txt, x=x, y=y, font_size=small))
    out.append("</svg>")
    return "\n".join(out) + "\n"

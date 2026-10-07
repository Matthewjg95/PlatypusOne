"""Unconstrained primitive fits. All coordinates in evidence units (pixels).

Objective for every primitive: least squares of the ORTHOGONAL point-to-
primitive distance (total least squares). Reported diagnostics are the RMS
and maximum orthogonal residual over the points used.
"""

from __future__ import annotations

from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class LineFit:
    centroid: np.ndarray  # (2,)
    direction: np.ndarray  # unit (2,)
    normal: np.ndarray  # unit (2,), direction rotated +90 deg
    rms: float
    max_abs: float
    n: int

    def distances(self, pts: np.ndarray) -> np.ndarray:
        return (pts - self.centroid) @ self.normal

    def extent(self, pts: np.ndarray) -> tuple[float, float]:
        t = (pts - self.centroid) @ self.direction
        return float(t.min()), float(t.max())


@dataclass(frozen=True)
class CircleFit:
    center: np.ndarray  # (2,)
    radius: float
    rms: float
    max_abs: float
    n: int
    span_rad: float  # angular extent covered by the points (2*pi = full)

    def distances(self, pts: np.ndarray) -> np.ndarray:
        return np.linalg.norm(pts - self.center, axis=1) - self.radius


def scatter(pts: np.ndarray) -> tuple[np.ndarray, np.ndarray]:
    """Centroid and 2x2 scatter matrix (sum of outer products about it)."""
    c = pts.mean(axis=0)
    d = pts - c
    return c, d.T @ d


def line_from_normal(pts: np.ndarray, normal: np.ndarray) -> LineFit:
    """Line through the centroid with a GIVEN normal (used for constrained fits)."""
    c = pts.mean(axis=0)
    n = normal / np.linalg.norm(normal)
    d = np.array([n[1], -n[0]])
    r = (pts - c) @ n
    return LineFit(c, d, n, float(np.sqrt(np.mean(r * r))), float(np.max(np.abs(r))), len(pts))


def fit_line(pts: np.ndarray) -> LineFit:
    """Total-least-squares line: normal = smallest-eigenvalue eigenvector of the scatter."""
    _, s = scatter(pts)
    w, v = np.linalg.eigh(s)
    normal = v[:, 0]
    # Canonical sign so identical inputs give identical outputs.
    if normal[1] < 0 or (normal[1] == 0 and normal[0] < 0):
        normal = -normal
    return line_from_normal(pts, normal)


def _angular_span(pts: np.ndarray, center: np.ndarray) -> float:
    a = np.sort(np.arctan2(pts[:, 1] - center[1], pts[:, 0] - center[0]))
    if len(a) < 2:
        return 0.0
    gaps = np.diff(np.concatenate([a, [a[0] + 2 * np.pi]]))
    return float(2 * np.pi - gaps.max())


def fit_circle(pts: np.ndarray, iterations: int = 20) -> CircleFit | None:
    """Kasa algebraic seed, then Gauss-Newton on the geometric residual.

    Returns None for degenerate (near-collinear) point sets.
    """
    if len(pts) < 3:
        return None
    m = pts.mean(axis=0)
    p = pts - m  # conditioning
    A = np.column_stack([p[:, 0], p[:, 1], np.ones(len(p))])
    b = -(p[:, 0] ** 2 + p[:, 1] ** 2)
    sol, *_ = np.linalg.lstsq(A, b, rcond=None)
    cx, cy = -sol[0] / 2, -sol[1] / 2
    r2 = cx * cx + cy * cy - sol[2]
    if not np.isfinite(r2) or r2 <= 0:
        return None
    c = np.array([cx, cy])
    r = float(np.sqrt(r2))
    for _ in range(iterations):
        d = p - c
        dist = np.linalg.norm(d, axis=1)
        if np.any(dist == 0):
            return None
        res = dist - r
        J = np.column_stack([-d[:, 0] / dist, -d[:, 1] / dist, -np.ones(len(p))])
        step, *_ = np.linalg.lstsq(J, -res, rcond=None)
        c = c + step[:2]
        r += step[2]
        if np.linalg.norm(step) < 1e-12 * max(1.0, r):
            break
    if not np.isfinite(r) or r <= 0:
        return None
    center = c + m
    res = np.linalg.norm(pts - center, axis=1) - r
    return CircleFit(
        center,
        float(r),
        float(np.sqrt(np.mean(res * res))),
        float(np.max(np.abs(res))),
        len(pts),
        _angular_span(pts, center),
    )


def circle_fixed_radius(pts: np.ndarray, radius: float, center0: np.ndarray) -> CircleFit:
    """Center only, radius held (equal-radius constrained fits)."""
    c = center0.astype(float).copy()
    for _ in range(20):
        d = pts - c
        dist = np.linalg.norm(d, axis=1)
        res = dist - radius
        J = np.column_stack([-d[:, 0] / dist, -d[:, 1] / dist])
        step, *_ = np.linalg.lstsq(J, -res, rcond=None)
        c = c + step
        if np.linalg.norm(step) < 1e-12 * max(1.0, radius):
            break
    res = np.linalg.norm(pts - c, axis=1) - radius
    return CircleFit(
        c,
        float(radius),
        float(np.sqrt(np.mean(res * res))),
        float(np.max(np.abs(res))),
        len(pts),
        _angular_span(pts, c),
    )


def cross2(a: np.ndarray, b: np.ndarray) -> np.ndarray | float:
    """z-component of the 2-D cross product (broadcasts over leading axes)."""
    a, b = np.asarray(a), np.asarray(b)
    return a[..., 0] * b[..., 1] - a[..., 1] * b[..., 0]

"""ChArUco target geometry -> printable SVG, and rasters for synthetic fixtures.

One geometry function feeds both, so the synthetic tests detect exactly the
layout that gets printed.
"""

from __future__ import annotations

import cv2
import numpy as np

from .calibration import Board

PAGE_MM = (215.9, 279.4)  # US Letter portrait; also fits A4 (210 x 297) when centred


def black_rects_mm(board: Board) -> list[tuple[float, float, float, float]]:
    """Every black rectangle (x, y, w, h) in board mm, origin top-left, y down."""
    b = board.build()
    cols, rows = board.squares
    s = board.square_mm
    rects: list[tuple[float, float, float, float]] = []
    # Square colour from OpenCV's own rendering, sampled just inside each
    # square's corner (outside any marker): black square -> dark sample.
    px = 40
    img = b.generateImage((cols * px, rows * px), marginSize=0, borderBits=1)
    off = max(1, px // 20)
    for r in range(rows):
        for c in range(cols):
            if img[r * px + off, c * px + off] < 128:
                rects.append((c * s, r * s, s, s))
    dic = b.getDictionary()
    for corners, mid in zip(b.getObjPoints(), b.getIds().ravel(), strict=True):
        c = np.asarray(corners, np.float64)[:, :2]
        bits = cv2.aruco.generateImageMarker(dic, int(mid), 6, borderBits=1)
        ux = (c[1] - c[0]) / 6.0
        uy = (c[3] - c[0]) / 6.0
        for rr in range(6):
            for cc in range(6):
                if bits[rr, cc] < 128:
                    p = c[0] + cc * ux + rr * uy
                    q = c[0] + (cc + 1) * ux + (rr + 1) * uy
                    x0, y0 = min(p[0], q[0]), min(p[1], q[1])
                    rects.append((x0, y0, abs(q[0] - p[0]), abs(q[1] - p[1])))
    return rects


def svg(board: Board, label: str) -> str:
    cols, rows = board.squares
    bw, bh = cols * board.square_mm, rows * board.square_mm
    ox, oy = (PAGE_MM[0] - bw) / 2, (PAGE_MM[1] - bh) / 2 - 8
    out = [
        '<?xml version="1.0" encoding="UTF-8"?>',
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{PAGE_MM[0]}mm" height="{PAGE_MM[1]}mm" '
        f'viewBox="0 0 {PAGE_MM[0]} {PAGE_MM[1]}">',
        f'<rect x="0" y="0" width="{PAGE_MM[0]}" height="{PAGE_MM[1]}" fill="#fff"/>',
        f'<g fill="#000" transform="translate({ox:.4f} {oy:.4f})" shape-rendering="crispEdges">',
    ]
    for x, y, w, h in black_rects_mm(board):
        out.append(f'<rect x="{x:.4f}" y="{y:.4f}" width="{w:.4f}" height="{h:.4f}"/>')
    out.append("</g>")
    ty = oy + bh + 8
    bar_x = ox
    out.append(f'<rect x="{bar_x:.2f}" y="{ty:.2f}" width="100" height="2" fill="#000"/>')
    out.append(
        f'<text x="{bar_x:.2f}" y="{ty + 7:.2f}" font-family="sans-serif" font-size="3.2">'
        f"{label} | bar = 100.0 mm nominal | print at 100% (actual size), flat, matte paper; "
        f"measure 6 squares with calipers and record square_mm + status=verified</text>"
    )
    out.append("</svg>")
    return "\n".join(out) + "\n"


def raster(board: Board, px_per_mm: float, margin_mm: float = 15.0) -> np.ndarray:
    """Board on white paper as uint8 (for synthetic fixtures), rendered from black_rects_mm."""
    cols, rows = board.squares
    W = int(round((cols * board.square_mm + 2 * margin_mm) * px_per_mm))
    H = int(round((rows * board.square_mm + 2 * margin_mm) * px_per_mm))
    # Supersample 4x then area-downsample for anti-aliased edges.
    ss = 4
    img = np.full((H * ss, W * ss), 255, np.uint8)
    k = px_per_mm * ss
    for x, y, w, h in black_rects_mm(board):
        x0 = int(round((x + margin_mm) * k))
        y0 = int(round((y + margin_mm) * k))
        x1 = int(round((x + w + margin_mm) * k))
        y1 = int(round((y + h + margin_mm) * k))
        img[y0:y1, x0:x1] = 0
    return cv2.resize(img, (W, H), interpolation=cv2.INTER_AREA)

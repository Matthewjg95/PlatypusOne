"""Read source frames into analysis arrays. Never writes to a source file.

Every loader returns a `Loaded` with:
  lum    float64 HxW in [0, 1]; for raw Bayer this is LINEAR signal,
         (value - black) / (white - black), demosaiced to grey;
         for processed formats (PNG/PNM/ABGR/YUYV) it is the stored value / max,
         i.e. whatever tone curve the capture stack applied.
  gray8  uint8 HxW = round(255 * lum), for detectors that need 8-bit input
  clipped  bool HxW, True where the stored value is at/near full scale
  linear   True only for raw Bayer

Exposure thresholds therefore mean different radiometry for raw and processed
frames: compare exposure numbers only between frames of the same format.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

import cv2
import numpy as np

from .contract import Frame

# A stored value within this fraction of full scale counts as clipped.
CLIP_FRACTION = 0.995


@dataclass
class Loaded:
    lum: np.ndarray
    gray8: np.ndarray
    clipped: np.ndarray
    linear: bool
    native_full_scale: float  # DN span used for normalisation (white - black, or max)


_BAYER_TO_GRAY = {
    # Sensor-order names (OpenCV's legacy names are offset: RGGB == "BG").
    "RGGB": cv2.COLOR_BayerRGGB2GRAY,
    "BGGR": cv2.COLOR_BayerBGGR2GRAY,
    "GRBG": cv2.COLOR_BayerGRBG2GRAY,
    "GBRG": cv2.COLOR_BayerGBRG2GRAY,
}


def unpack_raw10p(buf: bytes, width: int, height: int, stride: int | None) -> np.ndarray:
    """MIPI RAW10 packed (V4L2 pRAA family): 4 pixels in 5 bytes, 4 MSB bytes then LSBs."""
    line = stride or (width * 5 + 3) // 4
    arr = np.frombuffer(buf, dtype=np.uint8).reshape(height, line)
    groups = width // 4
    blk = arr[:, : groups * 5].reshape(height, groups, 5).astype(np.uint16)
    lsb = blk[:, :, 4]
    out = np.empty((height, groups, 4), dtype=np.uint16)
    for i in range(4):
        out[:, :, i] = (blk[:, :, i] << 2) | ((lsb >> (2 * i)) & 0x3)
    return out.reshape(height, groups * 4)[:, :width]


def _from_bayer(raw: np.ndarray, pattern: str, black: float, white: float) -> Loaded:
    clipped_bayer = raw >= (black + (white - black) * CLIP_FRACTION)
    signal = np.clip(raw.astype(np.float64) - black, 0, white - black)
    scaled = np.round(signal / (white - black) * 65535).astype(np.uint16)
    grey16 = cv2.cvtColor(scaled, _BAYER_TO_GRAY[pattern])
    lum = grey16.astype(np.float64) / 65535.0
    # A grey pixel is clipped if any Bayer sample in its 2x2 neighbourhood is.
    k = np.ones((2, 2), np.uint8)
    clipped = cv2.dilate(clipped_bayer.astype(np.uint8), k).astype(bool)
    return Loaded(lum, _to8(lum), clipped, True, float(white - black))


def _to8(lum: np.ndarray) -> np.ndarray:
    return np.round(np.clip(lum, 0, 1) * 255).astype(np.uint8)


def _processed(values: np.ndarray, full: float) -> Loaded:
    lum = values.astype(np.float64) / full
    return Loaded(lum, _to8(lum), values >= full * CLIP_FRACTION, False, full)


def load(frame: Frame) -> Loaded:
    path: Path | None = frame.abs_path
    if path is None:
        raise ValueError(f"{frame.ref}: no source file")
    d = frame.data
    fmt = d["format"]
    w, h = int(d["width"]), int(d["height"])
    stride = int(d["stride"]) if d.get("stride") else None
    if fmt in ("png", "pgm", "ppm"):
        img = cv2.imread(str(path), cv2.IMREAD_UNCHANGED)
        if img is None:
            raise ValueError(f"{frame.ref}: unreadable {fmt}")
        full = 65535.0 if img.dtype == np.uint16 else 255.0
        if img.ndim == 3:
            if img.shape[2] == 4:
                img = img[:, :, :3]
            img = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        return _processed(img, full)
    buf = path.read_bytes()
    if fmt == "raw10p":
        raw = unpack_raw10p(buf, w, h, stride)
        return _from_bayer(
            raw, d["bayer_pattern"], float(d["black_level"]), float(d["white_level"])
        )
    if fmt == "raw16":
        line = (stride or w * 2) // 2
        raw = np.frombuffer(buf, dtype="<u2").reshape(h, line)[:, :w]
        return _from_bayer(
            raw, d["bayer_pattern"], float(d["black_level"]), float(d["white_level"])
        )
    if fmt == "abgr8888":
        # libcamera / DRM ABGR8888 is little-endian: bytes R, G, B, A in memory.
        line = stride or w * 4
        px = np.frombuffer(buf, dtype=np.uint8).reshape(h, line)[:, : w * 4].reshape(h, w, 4)
        grey = cv2.cvtColor(np.ascontiguousarray(px[:, :, :3]), cv2.COLOR_RGB2GRAY)
        return _processed(grey, 255.0)
    if fmt == "yuyv":
        line = stride or w * 2
        px = np.frombuffer(buf, dtype=np.uint8).reshape(h, line)[:, : w * 2]
        return _processed(np.ascontiguousarray(px[:, 0::2]), 255.0)
    raise ValueError(f"{frame.ref}: unsupported format {fmt}")


def write_pgm(path: Path, gray8: np.ndarray) -> None:
    """Derived-output writer (8-bit binary PGM) for tools that read PNM."""
    h, w = gray8.shape
    with path.open("wb") as f:
        f.write(b"P5\n%d %d\n255\n" % (w, h))
        f.write(np.ascontiguousarray(gray8).tobytes())

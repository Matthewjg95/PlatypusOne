"""Synthetic 20mm reference and 40x20mm plate at exactly 4 pixels/mm."""

from pathlib import Path

import cv2
import numpy as np

from services.vision.python.ai01 import Image


def fixture() -> Image:
    image = np.full((240, 400, 3), 255, dtype=np.uint8)
    image[40:120, 30:110] = 0  # 80x80 pixel cells = 20mm square
    image[100:180, 180:340] = 0  # 160x80 pixel cells = 40x20mm plate
    cv2.circle(image, (260, 140), 12, (255, 255, 255), -1)  # approx 6mm void
    return image


if __name__ == "__main__":
    target = Path(__file__).parent / "fixtures" / "plate.png"
    target.parent.mkdir(exist_ok=True)
    if not cv2.imwrite(str(target), fixture()):
        raise OSError(f"could not write {target}")

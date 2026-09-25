"""Generate the Engineering Scout calibration sheet as a print-ready PDF.

The analyzer (services/vision ScoutAnalyzer) binarizes with Otsu, treats dark
pixels as foreground, and takes the one solid, near-square blob as the 20 mm
reference. Everything in the camera's view must therefore be either white
paper, that single square, or the fastener - so all text, the print-scale
bar and the cut marks live in a strip that is cut off before use.

    python tools/calibration_sheet/make_calibration_sheet.py docs/hardware/calibration_sheet.pdf

Page 1  working sheet: one 20 mm square, the rest blank for the fastener.
Page 2  validation sheet: the same square plus a printed 40 x 8 mm bar with
        known dimensions, so the measurement can be checked without calipers.

Print at 100 % / "actual size" (never "fit to page"), then confirm the
100 mm bar in the strip against a ruler before trusting any measurement.
"""

from __future__ import annotations

import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

DPI = 300
PAGE_W_IN, PAGE_H_IN = 8.5, 11.0  # US Letter; A4 users: still prints at 100 % with margins
MM_PER_IN = 25.4

REFERENCE_MM = 20.0
BAR_MM = (40.0, 8.0)  # validation subject: length x width
SCALE_BAR_MM = 100.0

STRIP_TOP_MM = 215.0  # everything below this line is cut off
MARGIN_MM = 12.0


def px(mm: float) -> int:
    return round(mm / MM_PER_IN * DPI)


def font(size_pt: int) -> ImageFont.ImageFont:
    for name in ("arial.ttf", "DejaVuSans.ttf", "LiberationSans-Regular.ttf"):
        try:
            return ImageFont.truetype(name, round(size_pt / 72 * DPI))
        except OSError:
            continue
    return ImageFont.load_default()


def new_page() -> tuple[Image.Image, ImageDraw.ImageDraw]:
    page = Image.new(
        "L",
        (round(PAGE_W_IN * DPI), round(PAGE_H_IN * DPI)),
        255,
    )
    return page, ImageDraw.Draw(page)


def draw_square(
    draw: ImageDraw.ImageDraw, x_mm: float, y_mm: float, side_mm: float
) -> None:
    draw.rectangle(
        [px(x_mm), px(y_mm), px(x_mm + side_mm), px(y_mm + side_mm)],
        fill=0,
    )


def draw_strip(draw: ImageDraw.ImageDraw, title: str, lines: list[str]) -> None:
    y = STRIP_TOP_MM
    # Cut line: dashed, so it reads as "cut here" and not as a border.
    for x in range(
        px(MARGIN_MM),
        px(PAGE_W_IN * MM_PER_IN - MARGIN_MM),
        px(6),
    ):
        draw.line([x, px(y), x + px(3), px(y)], fill=0, width=px(0.3))
    draw.text(
        (px(MARGIN_MM), px(y + 1.5)),
        "cut along this line before use",
        font=font(8),
        fill=0,
    )

    draw.text((px(MARGIN_MM), px(y + 9)), title, font=font(14), fill=0)
    ty = y + 17
    for line in lines:
        draw.text((px(MARGIN_MM), px(ty)), line, font=font(9), fill=0)
        ty += 4.6

    # Print-scale verification bar with 10 mm ticks.
    bx, by = MARGIN_MM, ty + 4
    draw.rectangle(
        [px(bx), px(by), px(bx + SCALE_BAR_MM), px(by + 1.2)],
        fill=0,
    )
    for i in range(0, int(SCALE_BAR_MM) + 1, 10):
        draw.line(
            [px(bx + i), px(by), px(bx + i), px(by + 4)],
            fill=0,
            width=px(0.3),
        )
        draw.text(
            (px(bx + i - 1.5), px(by + 4.5)),
            str(i),
            font=font(7),
            fill=0,
        )
    draw.text(
        (px(bx + SCALE_BAR_MM + 5), px(by - 1)),
        f"= {SCALE_BAR_MM:.0f} mm on a ruler, or the print is scaled and the sheet is void",
        font=font(8),
        fill=0,
    )


def working_sheet() -> Image.Image:
    page, draw = new_page()
    draw_square(draw, 50.0, 60.0, REFERENCE_MM)
    draw_strip(
        draw,
        "Engineering Scout - calibration sheet (working)",
        [
            f"Reference: the solid square is {REFERENCE_MM:.0f} x {REFERENCE_MM:.0f} mm "
            "(engineering_scout_capture default --reference-mm 20).",
            "Place ONE fastener on the white area, at least 20 mm from the square, "
            "not touching it. Nothing else in the camera's view.",
            "Camera straight down, 15-25 cm above the sheet; light from the side so the "
            "fastener casts no hard shadow.",
            "Matte paper. If the print looks grey or streaky, reprint - the square must be "
            "solid black.",
        ],
    )
    return page


def validation_sheet() -> Image.Image:
    page, draw = new_page()
    draw_square(draw, 50.0, 60.0, REFERENCE_MM)
    bar_x, bar_y = 110.0, 120.0
    draw.rectangle(
        [
            px(bar_x),
            px(bar_y),
            px(bar_x + BAR_MM[0]),
            px(bar_y + BAR_MM[1]),
        ],
        fill=0,
    )
    draw_strip(
        draw,
        "Engineering Scout - calibration sheet (validation)",
        [
            f"Same {REFERENCE_MM:.0f} mm reference square plus a printed test subject: "
            f"a {BAR_MM[0]:.0f} x {BAR_MM[1]:.0f} mm bar.",
            f"Expected result: subject length {BAR_MM[0]:.1f} mm, width {BAR_MM[1]:.1f} mm "
            "(target |error| <= 0.5 mm at 640x480, 15-25 cm).",
            "Use this page first. If the bar measures wrong, the problem is the rig "
            "(scale, focus, lighting), not the fastener.",
        ],
    )
    return page


def main(argv: list[str]) -> int:
    out = Path(argv[1]) if len(argv) > 1 else Path("calibration_sheet.pdf")
    out.parent.mkdir(parents=True, exist_ok=True)
    pages = [validation_sheet(), working_sheet()]
    pages[0].save(
        out,
        "PDF",
        resolution=DPI,
        save_all=True,
        append_images=pages[1:],
    )
    print(f"wrote {out} ({len(pages)} pages, {DPI} dpi)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))

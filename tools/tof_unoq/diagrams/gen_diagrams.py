#!/usr/bin/env python3
"""Generate the UNO Q + Pololu #3419 assembly diagrams (SVG) from one sourced geometry table.

    python3 tools/tof_unoq/diagrams/gen_diagrams.py [--png]

Writes docs/hardware/tof_unoq/diagrams/*.svg (and *.png with --png, via headless
Chromium if available). Edit the tables below, not the SVGs.

Geometry sources (see docs/hardware/tof_unoq/SOURCES.md):
  P1  Pololu #3419 pinout photo 0J12380 (top view) and dimensioned bottom view 0J12379:
      0.5 x 0.9 in board, 1x9 row + 1x4 row, holes 0.7 in apart, dia 0.086 in.
  P2  Pololu schematic 0J2031: connector numbering 1-9 (CORE/IOVDD, AVDD, VIN, GND, SDA,
      SCL, MISO, NCS, INT) and 1-4 (GND, SPI_I2C_N, LPn, SYNC).
  P3  PlatypusOne footprint Pololu_3419_VL53L8CX_Carrier_THT (PR #44), from Pololu's
      dimension drawing: rows 10.16 mm apart, 1x4 row aligned with pins 4-7, holes at
      (8.93, 1.23) and (8.93, 19.13) mm from pin 1.
  A1  Arduino ABX00162 full pinout p.1 and datasheet 9.3: Qwiic 1 GND, 2 +3V3, 3 SDA PD13,
      4 SCL PD12, on the top side next to JSPI; schematic sheet 20 QWIIC1 CI1104M1HR0-NH.
The UNO Q / Media Carrier outlines are SCHEMATIC (not to scale). The Pololu board is to scale.
"""

from __future__ import annotations

import argparse
import html
import shutil
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[3]
OUT = REPO / "docs" / "hardware" / "tof_unoq" / "diagrams"

# ---------------------------------------------------------------- sourced data
IN = 25.4
BOARD_W, BOARD_H = 0.5 * IN, 0.9 * IN  # P1
PITCH = 2.54
EDGE = 1.27  # pin centre from the 1x9-row edge and from the top edge (P3)
ROW_SPACING = 10.16  # P3
HOLE_D = 0.086 * IN  # P1
HOLES_REL = [(8.93, 1.23), (8.93, 19.13)]  # P3, relative to pin 1
MAIN = ["CORE/IOVDD", "AVDD", "VIN", "GND", "SDA/MOSI", "SCL/MCLK", "MISO", "CS", "INT"]  # P1/P2
SMALL = ["GND", "SPI/I2C", "LP", "SYNC"]  # P1/P2; aligned with MAIN pins 4..7 (P1, P3)
SQUARE = {("M", 4), ("S", 1)}  # square pads in P1 photos are the two GND pins
QWIIC = [
    ("1", "GND", "0 V"),
    ("2", "+3V3", "PWR_3P3V"),
    ("3", "SDA", "PD13 I2C4_SDA"),
    ("4", "SCL", "PD12 I2C4_SCL"),
]
WIRES = [  # Qwiic pin, conventional colour (SparkFun Qwiic / Adafruit #4397), Pololu pin
    ("1", "GND", "#111111", "black", ("M", 4)),
    ("2", "+3V3", "#d62728", "red", ("M", 3)),
    ("3", "SDA", "#1f77b4", "blue", ("M", 5)),
    ("4", "SCL", "#e6b800", "yellow", ("M", 6)),
]

C = {
    "pcb": "#1f7a3a",
    "pcb_dark": "#14532a",
    "pad": "#d9d9d9",
    "text": "#111",
    "muted": "#555",
    "warn": "#b00020",
    "ok": "#1a7f37",
    "unver": "#c77700",
    "bg": "#ffffff",
    "line": "#333",
}
FONT = "font-family='DejaVu Sans, Arial, sans-serif'"


def esc(s: str) -> str:
    return html.escape(s, quote=True)


class Svg:
    def __init__(self, w: float, h: float, title: str):
        self.w, self.h = w, h
        self.items = [
            f"<title>{esc(title)}</title>",
            f"<rect width='{w}' height='{h}' fill='{C['bg']}'/>",
        ]

    def add(self, s: str) -> None:
        self.items.append(s)

    def text(self, x, y, s, size=12, color=None, anchor="start", weight="normal", italic=False):
        style = " font-style='italic'" if italic else ""
        self.add(
            f"<text x='{x:.2f}' y='{y:.2f}' {FONT} font-size='{size}' fill='{color or C['text']}' "
            f"text-anchor='{anchor}' font-weight='{weight}'{style}>{esc(s)}</text>"
        )

    def rect(self, x, y, w, h, fill="none", stroke=None, sw=1.0, rx=0, dash=None, opacity=1.0):
        d = f" stroke-dasharray='{dash}'" if dash else ""
        self.add(
            f"<rect x='{x:.2f}' y='{y:.2f}' width='{w:.2f}' height='{h:.2f}' rx='{rx}' "
            f"fill='{fill}' "
            f"stroke='{stroke or 'none'}' stroke-width='{sw}' opacity='{opacity}'{d}/>"
        )

    def line(self, x1, y1, x2, y2, color=None, sw=1.5, dash=None, arrow=False):
        d = f" stroke-dasharray='{dash}'" if dash else ""
        m = " marker-end='url(#arr)'" if arrow else ""
        self.add(
            f"<line x1='{x1:.2f}' y1='{y1:.2f}' x2='{x2:.2f}' y2='{y2:.2f}' "
            f"stroke='{color or C['line']}' "
            f"stroke-width='{sw}' stroke-linecap='round'{d}{m}/>"
        )

    def path(self, d, color, sw=3.0, dash=None):
        dd = f" stroke-dasharray='{dash}'" if dash else ""
        self.add(
            f"<path d='{d}' fill='none' stroke='{color}' stroke-width='{sw}' "
            f"stroke-linecap='round'{dd}/>"
        )

    def circle(self, x, y, r, fill="none", stroke=None, sw=1.0):
        self.add(
            f"<circle cx='{x:.2f}' cy='{y:.2f}' r='{r:.2f}' fill='{fill}' "
            f"stroke='{stroke or 'none'}' "
            f"stroke-width='{sw}'/>"
        )

    def save(self, path: Path) -> None:
        head = (
            f"<svg xmlns='http://www.w3.org/2000/svg' width='{self.w}' height='{self.h}' "
            f"viewBox='0 0 {self.w} {self.h}'>"
            "<defs><marker id='arr' markerWidth='10' markerHeight='10' refX='8' refY='3' "
            "orient='auto'>"
            "<path d='M0,0 L8,3 L0,6 z' fill='#333'/></marker></defs>"
        )
        path.write_text(head + "\n" + "\n".join(self.items) + "\n</svg>\n", encoding="utf-8")


def pin_xy(row: str, n: int, mirror: bool) -> tuple[float, float]:
    """Pin centre in mm on the board, top view; mirror=True gives the bottom view."""
    if row == "M":
        x, y = EDGE, EDGE + (n - 1) * PITCH
    else:
        x, y = EDGE + ROW_SPACING, EDGE + (n + 2) * PITCH  # small pin 1 aligns with main pin 4
    if mirror:
        x = BOARD_W - x
    return x, y


def draw_pololu(
    s: Svg,
    ox: float,
    oy: float,
    k: float,
    mirror: bool,
    label: str,
    jumper: bool = True,
    j1_gap: float = 6,
    j2_labels: bool = True,
) -> dict:
    """Draw the carrier to scale (k px/mm) at (ox, oy). Returns pin positions in px."""
    s.rect(
        ox,
        oy,
        BOARD_W * k,
        BOARD_H * k,
        fill=C["pcb"] if not mirror else C["pcb_dark"],
        stroke="#0b3",
        sw=1.2,
        rx=0.5 * k,
    )
    for hx, hy in HOLES_REL:
        x, y = EDGE + hx, EDGE + hy
        if mirror:
            x = BOARD_W - x
        s.circle(ox + x * k, oy + y * k, HOLE_D / 2 * k, fill="#fff", stroke="#999")
    if not mirror:
        # Sensor body: approximate position only (P1 photo; footprint optical-centre note
        # "approximate").
        sx, sy = EDGE + 5.03, EDGE + 12.0
        s.rect(
            ox + (sx - 3.2) * k, oy + (sy - 1.5) * k, 6.4 * k, 3.0 * k, fill="#111", stroke="#000"
        )
        s.text(
            ox + sx * k,
            oy + (sy + 3.3) * k,
            "VL53L8CX (approx.)",
            size=9,
            color="#fff",
            anchor="middle",
        )
    pos = {}
    for row, names in (("M", MAIN), ("S", SMALL)):
        for i, name in enumerate(names, start=1):
            x, y = pin_xy(row, i, mirror)
            px, py = ox + x * k, oy + y * k
            pos[(row, i)] = (px, py)
            r = 0.85 * k
            if (row, i) in SQUARE:
                s.rect(px - r, py - r, 2 * r, 2 * r, fill=C["pad"], stroke="#777")
            else:
                s.circle(px, py, r, fill=C["pad"], stroke="#777")
            s.circle(px, py, 0.45 * k, fill="#555")
            # label outside the board edge nearest the row
            outside_left = (row == "M") != mirror
            gap = j1_gap if row == "M" else 6
            if row == "S" and not j2_labels:
                continue
            tx = ox - gap if outside_left else ox + BOARD_W * k + gap
            anchor = "end" if outside_left else "start"
            num = f"{'J1' if row == 'M' else 'J2'}-{i}"
            s.text(
                tx,
                py + 4,
                f"{name}  ({num})",
                size=11,
                anchor=anchor,
                color=C["warn"] if name in ("CORE/IOVDD", "AVDD") else C["text"],
                weight="bold"
                if name in ("VIN", "GND", "SDA/MOSI", "SCL/MCLK", "SPI/I2C")
                else "normal",
            )
    if jumper:
        a, b = pos[("S", 1)], pos[("S", 2)]
        s.rect(
            a[0] - 0.9 * k,
            a[1] - 0.9 * k,
            1.8 * k,
            (b[1] - a[1]) + 1.8 * k,
            fill="#000",
            stroke="#000",
            rx=0.3 * k,
            opacity=0.75,
        )
    s.text(ox + BOARD_W * k / 2, oy - 10, label, size=13, anchor="middle", weight="bold")
    return pos


def legend_box(
    s: Svg, x: float, y: float, lines: list[tuple[str, str]], width: float = 380
) -> None:
    h = 18 * len(lines) + 12
    s.rect(x, y, width, h, fill="#fafafa", stroke="#bbb")
    for i, (txt, color) in enumerate(lines):
        s.text(x + 10, y + 22 + 18 * i, txt, size=11, color=color or C["text"])


# ------------------------------------------------------------------ diagrams
def pololu_soldering(path: Path) -> None:
    s = Svg(1180, 900, "Pololu 3419 soldering and jumper")
    s.text(
        20,
        30,
        "Pololu #3419 VL53L8CX carrier - header soldering and I2C-mode jumper",
        size=18,
        weight="bold",
    )
    s.text(
        20,
        52,
        "Board to scale from Pololu drawings (P1-P3). Pin numbers J1-n / J2-n follow Pololu's "
        "schematic "
        "connector numbering. Square pads are the two GND pins, not pin 1.",
        size=12,
        color=C["muted"],
    )
    k = 16.0
    draw_pololu(s, 190, 110, k, mirror=False, label="TOP VIEW - sensor side (solder joints here)")
    draw_pololu(s, 720, 110, k, mirror=True, label="BOTTOM VIEW - header bodies here", jumper=True)
    # side view of header mounting
    sx, sy = 120, 560
    s.text(
        sx, sy - 34, "SIDE VIEW (as in Pololu's breadboard photo 0J12378)", size=13, weight="bold"
    )
    s.rect(sx, sy, 380, 10, fill=C["pcb"], stroke="#0b3")
    s.text(sx + 390, sy + 9, "carrier PCB (sensor faces UP)", size=11)
    s.rect(sx + 150, sy - 12, 70, 12, fill="#111")
    s.text(sx + 185, sy - 16, "sensor", size=10, anchor="middle")
    for i in range(9):
        px = sx + 20 + i * 40
        s.line(px, sy - 8, px, sy + 70, color="#c9a227", sw=4)
        s.rect(px - 9, sy + 10, 18, 22, fill="#222")
        s.circle(px, sy - 4, 5, fill="#ccc", stroke="#999")
    s.text(sx + 390, sy + 26, "black header bodies UNDER the board", size=11)
    s.text(sx + 390, sy + 44, "solder each pin on TOP (sensor side) - keep joints small", size=11)
    s.text(
        sx + 390,
        sy + 62,
        "pin tails point DOWN -> female jumper sockets connect underneath",
        size=11,
    )
    legend_box(
        s,
        120,
        680,
        [
            (
                "SOLDER: full 1x9 + 1x4 straight 0.1 in male headers (13 joints). Headers are "
                "NOT included.",
                None,
            ),
            ("JUMPER: 2-pin 0.1 in shunt across J2-1 GND and J2-2 SPI/I2C -> I2C mode.", None),
            (
                "   (default is SPI: the pin is pulled high on the carrier; Pololu suggests "
                "this shunt)",
                C["muted"],
            ),
            ("NEVER connect power to J1-1 CORE/IOVDD or J1-2 AVDD: regulator OUTPUTS.", C["warn"]),
            (
                "   They are adjacent to J1-3 VIN - the most likely off-by-one wiring error.",
                C["warn"],
            ),
            ("Leave MISO, CS, INT, LP, SYNC unconnected (LP pulled high = I2C enabled).", None),
            ("Keep the protective liner on while soldering; remove it before ranging.", C["muted"]),
            ("Sensor body position is approximate (photo), not a dimension.", C["unver"]),
        ],
        width=760,
    )
    s.save(path)


def unoq_connection(path: Path) -> None:
    s = Svg(1240, 860, "UNO Q Qwiic to Pololu connection")
    s.text(
        20,
        30,
        "Where each wire terminates - UNO Q Qwiic (QWIIC1) -> Pololu #3419",
        size=18,
        weight="bold",
    )
    s.text(
        20,
        52,
        "UNO Q outline is SCHEMATIC (not to scale). Qwiic location and pin order: Arduino "
        "ABX00162 full "
        "pinout p.1 + datasheet section 9.3 (A1). Pololu board to scale.",
        size=12,
        color=C["muted"],
    )
    # UNO Q outline (schematic)
    ux, uy, uw, uh = 40, 90, 520, 400
    s.rect(ux, uy, uw, uh, fill="#2a7f9e", stroke="#14506a", sw=2, rx=14)
    s.text(
        ux + uw / 2,
        uy + 30,
        "Arduino UNO Q (TOP side) - schematic",
        size=14,
        color="#fff",
        anchor="middle",
        weight="bold",
    )
    s.rect(ux + uw / 2 - 40, uy - 14, 80, 28, fill="#bbb", stroke="#666", rx=4)
    s.text(ux + uw / 2, uy + 5, "USB-C", size=11, anchor="middle")
    s.rect(ux + uw - 30, uy + 60, 18, 300, fill="#111")
    s.text(ux + uw - 40, uy + 210, "JDIGITAL", size=11, color="#fff", anchor="end")
    s.rect(ux + 12, uy + 160, 18, 220, fill="#111")
    s.text(ux + 40, uy + 270, "JANALOG / power", size=11, color="#fff")
    s.text(
        ux + uw / 2,
        uy + 200,
        "Media Carrier plugs into JMEDIA/JMISC",
        size=11,
        color="#dff",
        anchor="middle",
    )
    s.text(
        ux + uw / 2,
        uy + 216,
        "on the BOTTOM side (not drawn)",
        size=11,
        color="#dff",
        anchor="middle",
    )
    # JSPI 2x3 and Qwiic at bottom edge (A1)
    s.rect(ux + 300, uy + uh - 70, 50, 34, fill="#111")
    s.text(ux + 325, uy + uh - 76, "JSPI", size=10, color="#fff", anchor="middle")
    qx, qy = ux + 200, uy + uh - 46
    s.rect(qx, qy, 84, 30, fill="#eee", stroke="#555", rx=3)
    s.text(qx + 42, qy - 22, "QWIIC1", size=12, color="#fff", anchor="middle", weight="bold")
    qpins = {}
    for i, (num, _name, _net) in enumerate(QWIIC):
        px = qx + 12 + i * 20
        s.rect(px - 4, qy + 6, 8, 18, fill="#c9a227")
        s.text(px, qy - 4, num, size=11, color="#fff", anchor="middle", weight="bold")
        qpins[num] = (px, qy + 30)
    s.text(qx - 6, qy - 4, "pin", size=10, color="#fff", anchor="end")
    # Qwiic pin table
    tx, ty = 40, 530
    s.text(tx, ty, "QWIIC1 pin map (A1)", size=13, weight="bold")
    for i, (num, name, net) in enumerate(QWIIC):
        s.text(tx, ty + 20 + 18 * i, f"{num}  {name:5s}  {net}", size=12)
    s.text(
        tx,
        ty + 100,
        "2.2 kOhm pull-ups to PWR_3P3V on SDA/SCL (schematic sheet 20, R2613/R2614)",
        size=11,
        color=C["muted"],
    )
    s.text(
        tx,
        ty + 118,
        "Sketch bus: Wire1 = STM32 I2C4 (core variant i2cs[1]). Linux cannot see this bus.",
        size=11,
        color=C["muted"],
    )
    s.text(
        tx,
        ty + 136,
        "Plug is keyed: it fits one way. Seat it only with the board powered OFF.",
        size=11,
        color=C["warn"],
    )
    # Pololu bottom view (pins point down, jumper sockets attach here). J1 labels sit beyond
    # the wire lanes so no wire crosses a label; J2 is described in the legend.
    k = 15.0
    bx, by = 760, 130
    pos = draw_pololu(
        s,
        bx,
        by,
        k,
        mirror=True,
        label="Pololu #3419 BOTTOM view (header tails)",
        j1_gap=110,
        j2_labels=False,
    )
    s.text(bx - 8, pos[("S", 1)][1] + 4, "J2-1 GND", size=10, anchor="end")
    s.text(bx - 8, pos[("S", 2)][1] + 4, "J2-2 SPI/I2C (shunt)", size=10, anchor="end")
    s.text(bx - 8, pos[("S", 3)][1] + 4, "J2-3 LP", size=10, anchor="end")
    s.text(bx - 8, pos[("S", 4)][1] + 4, "J2-4 SYNC", size=10, anchor="end")
    edge = bx + BOARD_W * k
    # depth (below the connector) and lane order: rightmost Qwiic pin shallowest / innermost
    route = {"4": 0, "3": 1, "2": 2, "1": 3}
    for num, _name, color, _cname, target in WIRES:
        x0, y0 = qpins[num]
        x1, y1 = pos[target]
        r = route[num]
        bus_y = y0 + 22 + 16 * r
        lane_x = edge + 18 + 16 * r
        s.path(
            f"M{x0:.1f},{y0:.1f} L{x0:.1f},{bus_y:.1f} L{lane_x:.1f},{bus_y:.1f} "
            f"L{lane_x:.1f},{y1:.1f} L{x1:.1f},{y1:.1f}",
            color,
            sw=3.2,
        )
        s.circle(x1, y1, 5, fill=color, stroke="#000")
    legend_box(
        s,
        600,
        620,
        [
            ("Wire    Qwiic pin        ->  Pololu pin", None),
            ("black   1 GND            ->  J1-4 GND (square pad)", None),
            ("red     2 +3V3 PWR_3P3V  ->  J1-3 VIN   (NOT J1-2 AVDD, NOT J1-1 CORE)", C["warn"]),
            ("blue    3 SDA  PD13      ->  J1-5 SDA/MOSI", None),
            ("yellow  4 SCL  PD12      ->  J1-6 SCL/MCLK", None),
            ("shunt   J2-1 GND <-> J2-2 SPI/I2C (on the carrier, no wire to the UNO Q)", None),
            ("Colours are the Qwiic convention (SparkFun, Adafruit #4397); VERIFY by", C["unver"]),
            ("continuity at Gate B - a third-party cable may differ.", C["unver"]),
        ],
        width=600,
    )
    s.save(path)


def system_wiring(path: Path) -> None:
    s = Svg(1340, 840, "System wiring UNO Q Media Carrier camera display ToF")
    s.text(
        20,
        30,
        "Full system - UNO Q + UNO Media Carrier + IMX219 camera + DSI display + Pololu ToF",
        size=18,
        weight="bold",
    )
    s.text(
        20,
        52,
        "Block diagram, not to scale. Lines are electrical connections; software paths are "
        "listed in the UNO Q block.",
        size=12,
        color=C["muted"],
    )

    def block(x, y, w, h, title, lines, fill):
        s.rect(x, y, w, h, fill=fill, stroke="#333", sw=1.5, rx=8)
        s.text(x + 10, y + 22, title, size=13, weight="bold")
        for i, ln in enumerate(lines):
            s.text(x + 10, y + 42 + 16 * i, ln, size=11)

    block(
        40,
        90,
        250,
        90,
        "Power: USB-C 5 V / 3 A",
        ["UNO Q requests 5 V/3 A PD only (DS)", "or DC_IN 7-24 V (DS)"],
        "#f3f3f3",
    )
    block(
        380,
        80,
        470,
        300,
        "Arduino UNO Q (ABX00162)",
        [
            "QRB2210 MPU - Debian Linux (1.8 V SoC I/O)",
            "   arduino-router: MCU UART <-> /var/run/arduino-router.sock",
            "   monitor proxy TCP 127.0.0.1:7500  <- tof_log.py / coacq.py",
            "   camss: CSI -> /dev/video* (IMX219, PR #48 path)",
            "STM32U585 MCU - Zephyr core (3.3 V I/O)",
            "   sketch tof_unoq_vl53l8cx: Wire1 = I2C4 PD13/PD12",
            "   ST ULD VL53L8CX 1.3.0 (via STM32duino VL53L8CX 2.1.0)",
            "   Bridge/Monitor over UART 115200 baud (library default)",
            "PWR_3P3V: TPS62A02A 2 A buck; also feeds MCU, ANX7625, Wi-Fi",
            "   3.3 V domain - NO documented allowance for external loads",
            "TOP: Qwiic QWIIC1   |   BOTTOM: JMEDIA + JMISC",
        ],
        "#dceef5",
    )
    block(
        380,
        470,
        470,
        150,
        "UNO Media Carrier (ASX00083)",
        [
            "plugs onto JMEDIA + JMISC (bottom of UNO Q), passthrough",
            "CSI0 / CSI1 22-pin 4-lane (1.8 V MPU domain, CCI I2C)",
            "DSI0 22-pin 4-lane display",
            "JMISC also carries MCU I2C4 on PF14/PF15 - SAME peripheral as",
            "Qwiic; the sketch uses the PD12/PD13 pin pair, PF14/15 unused",
        ],
        "#efe6f7",
    )
    block(
        40,
        680,
        300,
        100,
        "IMX219 camera (e.g. B0394)",
        [
            "22-pin FFC to CAMERA1 (PR #48 bench,",
            "2026-10-09)",
            "connector/orientation per CSI_CAMERA_BENCH.md",
        ],
        "#fff3d6",
    )
    block(
        420,
        680,
        390,
        100,
        "Waveshare 5in DSI (dev fixture)",
        [
            "22 -> 15-pin FFC adapter to DSI0",
            "(compact 3.5in candidate not yet run)",
            "touch on the MPU I2C, not the Qwiic bus",
        ],
        "#fff3d6",
    )
    block(
        990,
        120,
        330,
        200,
        "Pololu #3419 VL53L8CX",
        [
            "VIN 3.2-5.5 V (here 3.3 V from Qwiic)",
            "on-board LDOs: AVDD 3.3 V, CORE/IOVDD 1.8 V",
            "NXS0108 shifter: I/O at VIN level",
            "I2C addr 0x29 (7-bit), <= 1 MHz (run 400 kHz)",
            "SPI/I2C shunt to GND -> I2C mode",
            "typ 100 mA, peak 150 mA (Pololu)",
            "MISO/CS/INT/LP/SYNC unconnected",
        ],
        "#e3f5e1",
    )
    s.line(290, 135, 380, 135, sw=3, arrow=True)
    s.text(300, 125, "5 V", size=11)
    # Qwiic cable
    for i, (_num, _name, color, _cname, _t) in enumerate(WIRES):
        y = 200 + 14 * i
        s.line(850, y, 990, y, color=color, sw=3.2)
    s.text(856, 186, "Qwiic 4-wire", size=11)
    s.text(856, 270, "GND/3V3/SDA/SCL", size=11)
    s.text(856, 286, "short: Pololu <8 cm ideal", size=11)
    s.line(615, 380, 615, 470, sw=4)
    s.text(625, 430, "JMEDIA + JMISC (stacked)", size=11)
    s.line(450, 620, 190, 680, sw=3)
    s.text(250, 650, "CSI FFC", size=11)
    s.line(615, 620, 615, 680, sw=3)
    s.text(625, 660, "DSI FFC", size=11)
    legend_box(
        s,
        900,
        440,
        [
            ("Domains", None),
            ("ToF: MCU 3.3 V domain only (Qwiic).", None),
            ("Camera/display: MPU 1.8 V domain via the carrier.", None),
            ("No signal is shared between them; shared = 5 V,", None),
            ("3.3 V rail capacity, ground, mechanics, CPU time.", None),
            ("Coexistence is UNVERIFIED until Gate F.", C["unver"]),
        ],
        width=340,
    )
    s.save(path)


def preinspection(path: Path) -> None:
    s = Svg(1240, 900, "Pre-power inspection points")
    s.text(
        20,
        30,
        "Pre-power inspection and measurement points (Gates B and C)",
        size=18,
        weight="bold",
    )
    s.text(
        20,
        52,
        "B# = continuity/isolation with ALL power removed (Gate B).  V# = DC voltage with the "
        "board "
        "powered, measured by the operator (Gate C).",
        size=12,
        color=C["muted"],
    )
    k = 16.0
    pos = draw_pololu(
        s, 200, 110, k, mirror=False, label="Pololu TOP view (probe the soldered pin tops)"
    )

    left_x = 200 - 175  # beyond the J1 labels
    right_x = 200 + BOARD_W * k + 110  # beyond the J2 labels

    def tag(p, txt, side, color=C["ok"]):
        x, y = p
        tx = left_x if side == "L" else right_x
        s.line(
            tx + (48 if side == "L" else 0),
            y,
            tx + (60 if side == "L" else -12),
            y,
            color=color,
            sw=1.0,
            dash="2,2",
        )
        s.rect(tx, y - 10, 46, 18, fill="#fff", stroke=color, rx=4)
        s.text(tx + 23, y + 3, txt, size=11, color=color, anchor="middle", weight="bold")

    tag(pos[("M", 1)], "V3", "L")
    tag(pos[("M", 2)], "V2", "L")
    tag(pos[("M", 3)], "V1", "L")
    tag(pos[("M", 5)], "V4", "L")
    tag(pos[("M", 6)], "V5", "L")
    tag(pos[("S", 2)], "V6", "R")
    tag(pos[("S", 3)], "V7", "R")
    legend_box(
        s,
        640,
        90,
        [
            ("Gate B - unpowered (board OFF, USB-C and DC_IN removed, cable fitted):", None),
            ("B1  Qwiic pin 1 GND  <->  J1-4 GND          expect continuity", None),
            ("B2  Qwiic pin 2 3V3  <->  J1-3 VIN          expect continuity", None),
            ("B3  Qwiic pin 3 SDA  <->  J1-5 SDA          expect continuity", None),
            ("B4  Qwiic pin 4 SCL  <->  J1-6 SCL          expect continuity", None),
            ("B5  J2-2 SPI/I2C     <->  J2-1 GND          expect ~0 ohm (shunt)", None),
            ("B6  VIN-GND, SDA-GND, SCL-GND, SDA-SCL       NOT a short; record ohms", None),
            ("B7  each adjacent pin pair on J1 and J2      no solder bridge (visual + ohms)", None),
            ("B8  J1-3 VIN <-> J1-2 AVDD and J1-1 CORE     NOT shorted (different nets)", None),
            (
                "    Through-IC readings drift as caps charge: record, don't force a pass.",
                C["muted"],
            ),
            ("", None),
            ("Gate C - powered (operator measures; ToF idle first, then ranging):", None),
            ("V1  VIN - GND          >= 3.20 V (Pololu min) ; UNO Q rail spec 3.1-3.5 V", None),
            (
                "V2  AVDD - GND         >= 3.13 V (DS14161 min) - most critical, re-check "
                "while ranging",
                C["warn"],
            ),
            ("V3  CORE/IOVDD - GND   1.62-1.98 V (DS14161)", None),
            ("V4  SDA idle - GND     ~ VIN (pulled up both sides)", None),
            ("V5  SCL idle - GND     ~ VIN", None),
            ("V6  SPI/I2C - GND      ~ 0 V (shunt present)", None),
            ("V7  LP - GND           ~ VIN (pulled high = I2C enabled)", None),
            ("Do not probe adjacent pins with bare tips while powered: a slip shorts", C["warn"]),
            ("CORE/AVDD/VIN. Use a hooked probe or the female-socket side.", C["warn"]),
        ],
        width=580,
    )
    # UNO Q Qwiic close-up for B1-B4
    qx, qy = 80, 600
    s.text(
        qx,
        qy - 20,
        "UNO Q QWIIC1 (top side, schematic) - probe from the cable's female-socket ends",
        size=12,
        weight="bold",
    )
    s.rect(qx, qy, 120, 40, fill="#eee", stroke="#555", rx=3)
    for i, (num, name, _net) in enumerate(QWIIC):
        px = qx + 20 + i * 27
        s.rect(px - 5, qy + 8, 10, 24, fill="#c9a227")
        s.text(px, qy + 58, f"{num}", size=12, anchor="middle", weight="bold")
        s.text(px, qy + 74, name, size=10, anchor="middle")
    s.text(
        qx + 160,
        qy + 20,
        "Do not push meter probes into the 1 mm Qwiic receptacle;",
        size=11,
        color=C["muted"],
    )
    s.text(
        qx + 160,
        qy + 38,
        "measure at the cable's other end or the Pololu pins.",
        size=11,
        color=C["muted"],
    )
    s.save(path)


def to_png(svg: Path) -> bool:
    chrome = (
        shutil.which("chromium")
        or shutil.which("google-chrome")
        or "/opt/pw-browsers/chromium-1194/chrome-linux/chrome"
    )
    if not Path(chrome).exists():
        return False
    w, h = (int(float(v)) for v in svg.read_text().split("viewBox='0 0 ")[1].split("'")[0].split())
    png = svg.with_suffix(".png")
    subprocess.run(
        [
            chrome,
            "--headless",
            "--no-sandbox",
            "--hide-scrollbars",
            f"--screenshot={png}",
            f"--window-size={w},{h + 120}",
            f"file://{svg}",
        ],
        capture_output=True,
        check=False,
    )
    return png.exists()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--png", action="store_true")
    args = ap.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    made = []
    for name, fn in (
        ("01_system_wiring", system_wiring),
        ("02_pololu_soldering", pololu_soldering),
        ("03_unoq_qwiic_connection", unoq_connection),
        ("04_preinspection_points", preinspection),
    ):
        p = OUT / f"{name}.svg"
        fn(p)
        made.append(p)
        if args.png:
            to_png(p)
    for p in made:
        print(p.relative_to(REPO))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

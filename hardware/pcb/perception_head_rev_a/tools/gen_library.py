# ruff: noqa: E501 -- long human-readable schematic notes and part fields are data
"""Generate lib/PlatypusOne.kicad_sym — project-local symbols for Perception Head Rev A.

Only parts with no suitable KiCad 9 stock symbol live here. Each symbol records
the document its pinout was taken from. Re-run after editing; the output file
is committed so KiCad never depends on this script.
"""

import os
from pathlib import Path

from sexpr import Sym, dumps, find, first, parse

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.join(HERE, "..", "lib", "PlatypusOne.kicad_sym")
STOCK = "/usr/share/kicad/symbols"


def prop(name, value, x=0, y=0, hide=False):
    eff = [Sym("effects"), [Sym("font"), [Sym("size"), Sym("1.27"), Sym("1.27")]]]
    if hide:
        eff.append([Sym("hide"), Sym("yes")])
    return [Sym("property"), name, value, [Sym("at"), Sym(f"{x:g}"), Sym(f"{y:g}"), Sym("0")], eff]


def pin(kind, name, num, x, y, ang, length=2.54):
    f = [Sym("effects"), [Sym("font"), [Sym("size"), Sym("1.27"), Sym("1.27")]]]
    return [
        Sym("pin"),
        Sym(kind),
        Sym("line"),
        [Sym("at"), Sym(f"{x:g}"), Sym(f"{y:g}"), Sym(str(ang))],
        [Sym("length"), Sym(f"{length:g}")],
        [Sym("name"), name, f],
        [Sym("number"), str(num), f],
    ]


def box_symbol(name, props, left, right, top=(), bottom=(), width=15.24):
    """Rectangle symbol. left/right: lists of (kind, name, number) top-to-bottom
    (None = spacer). top/bottom: lists of (kind, name, number) left-to-right."""
    rows = max(len(left), len(right))
    h = (rows + 1) * 2.54
    y0 = h / 2
    hw = width / 2
    body = [
        Sym("rectangle"),
        [Sym("start"), Sym(f"{-hw:g}"), Sym(f"{y0:g}")],
        [Sym("end"), Sym(f"{hw:g}"), Sym(f"{-y0:g}")],
        [Sym("stroke"), [Sym("width"), Sym("0.254")], [Sym("type"), Sym("default")]],
        [Sym("fill"), [Sym("type"), Sym("background")]],
    ]
    pins = []
    for i, p in enumerate(left):
        if p:
            pins.append(pin(p[0], p[1], p[2], -hw - 2.54, y0 - 2.54 * (i + 1), 0))
    for i, p in enumerate(right):
        if p:
            pins.append(pin(p[0], p[1], p[2], hw + 2.54, y0 - 2.54 * (i + 1), 180))
    for i, p in enumerate(top):
        x = -2.54 * (len(top) - 1) / 2 + 2.54 * i
        pins.append(pin(p[0], p[1], p[2], x, y0 + 2.54, 270))
    for i, p in enumerate(bottom):
        x = -2.54 * (len(bottom) - 1) / 2 + 2.54 * i
        pins.append(pin(p[0], p[1], p[2], x, -y0 - 2.54, 90))
    sym = [
        Sym("symbol"),
        name,
        [Sym("pin_names"), [Sym("offset"), Sym("1.016")]],
        [Sym("exclude_from_sim"), Sym("no")],
        [Sym("in_bom"), Sym("yes")],
        [Sym("on_board"), Sym("yes")],
    ]
    sym.append(prop("Reference", props["ref"], -hw, y0 + 1.27))
    sym.append(prop("Value", name, hw, y0 + 1.27))
    sym.append(prop("Footprint", props["fp"], 0, 0, True))
    sym.append(prop("Datasheet", props["ds"], 0, 0, True))
    sym.append(prop("Description", props["desc"], 0, 0, True))
    sym.append([Sym("symbol"), f"{name}_0_1", body])
    sym.append([Sym("symbol"), f"{name}_1_1"] + pins)
    sym.append([Sym("embedded_fonts"), Sym("no")])
    return sym


def bmi270():
    """BMI270 is pin- and package-compatible with the stock BMI160 symbol
    (BMI270 datasheet Table 22). Copy its graphics, fix ASCx to I/O and point at
    the Bosch-exact land pattern."""
    root = parse(Path(f"{STOCK}/Sensor_Motion.kicad_sym").read_text())[0]
    src = next(s for s in find(root, "symbol") if s[1] == "BMI160")
    text = dumps(src).replace('"BMI160', '"BMI270')
    sym = parse(text)[0]
    for p in find(sym, "property"):
        if p[1] == "Value":
            p[2] = "BMI270"
        elif p[1] == "Footprint":
            p[2] = "PlatypusOne:Bosch_LGA-14_3x2.5mm_P0.5mm_BoschLand"
        elif p[1] == "Datasheet":
            p[2] = (
                "https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bmi270-ds000.pdf"
            )
        elif p[1] == "Description":
            p[2] = (
                "Bosch BMI270 6-axis IMU, LGA-14 2.5x3.0 mm (pinout per BST-BMI270-DS000 Table 22)"
            )
    for unit in find(sym, "symbol"):
        for pn in find(unit, "pin"):
            if first(pn, "name")[1] in ("ASCx", "OCSB"):
                pn[1] = Sym("bidirectional")
            if first(pn, "name")[1] == "OSDO":
                pn[1] = Sym("output")
    return sym


def main():
    syms = []
    syms.append(
        box_symbol(
            "TPS2553DBV",
            dict(
                ref="U",
                fp="Package_TO_SOT_SMD:SOT-23-6",
                ds="https://www.ti.com/lit/ds/symlink/tps2553.pdf",
                desc="TI TPS2553 adjustable current-limited power switch, EN active-high, SOT-23-6 (pinout per SLVS841F section 6)",
            ),
            left=[("power_in", "IN", 1), None, ("input", "EN", 3)],
            right=[
                ("power_out", "OUT", 6),
                ("open_collector", "~{FAULT}", 4),
                ("passive", "ILIM", 5),
            ],
            bottom=[("power_in", "GND", 2)],
        )
    )
    syms.append(
        box_symbol(
            "Pololu_3419_VL53L8CX_Carrier",
            dict(
                ref="M",
                fp="PlatypusOne:Pololu_3419_VL53L8CX_Carrier_THT",
                ds="https://www.pololu.com/product/3419",
                desc="Pololu #3419 VL53L8CX carrier: on-board 3.3 V + 1.8 V LDOs and NXS0108 level shifter; host I/O follows VIN (3.2-5.5 V). Pins 1-9 = 1x9 header, 10-13 = 1x4 header (BENCH_VERIFY order).",
            ),
            left=[
                ("power_in", "VIN", 3),
                None,
                ("bidirectional", "SDA", 5),
                ("bidirectional", "SCL", 6),
                ("output", "MISO", 7),
                ("input", "~{CS}", 8),
            ],
            right=[
                ("power_out", "CORE/IOVDD", 1),
                ("power_out", "AVDD", 2),
                None,
                ("output", "INT", 9),
                ("input", "SPI/~{I2C}", 11),
                ("input", "~{LP}", 12),
                ("input", "SYNC", 13),
            ],
            bottom=[("power_in", "GND", 4), ("power_in", "GND", 10)],
            width=20.32,
        )
    )
    syms.append(bmi270())

    lib = [
        Sym("kicad_symbol_lib"),
        [Sym("version"), Sym("20241209")],
        [Sym("generator"), "platypusone_gen_library"],
        [Sym("generator_version"), "9.0"],
    ] + syms
    with open(OUT, "w") as f:
        f.write(dumps(lib) + "\n")
    print("wrote", os.path.relpath(OUT))


if __name__ == "__main__":
    main()

"""Cross-check the KiCad-exported netlist against the generator's declared
connections. Usage: kicad-cli sch export netlist -o net.net <root>.kicad_sch;
python3 tools/check_netlist.py net.net"""

import sys
from pathlib import Path

import gen_schematic as g
from sexpr import find, first, parse


def main(path):
    root = parse(Path(path).read_text())[0]
    actual = {}
    for net in find(first(root, "nets"), "net"):
        name = first(net, "name")[1].split("/")[-1]
        for node in find(net, "node"):
            actual[(first(node, "ref")[1], first(node, "pin")[1])] = name
    errors = 0
    for sh in g.SHEETS:
        for p in sh["parts"]:
            if p.get("power"):
                continue
            for pin, want in p["conns"].items():
                got = actual.get((p["ref"], pin))
                if want is None:
                    if got and not got.startswith("unconnected-"):
                        print(f"{p['ref']}.{pin}: expected NC, got {got}")
                        errors += 1
                    continue
                if got != want:
                    print(f"{p['ref']}.{pin}: expected {want}, got {got}")
                    errors += 1
    nets = {}
    for (ref, pin), n in actual.items():
        nets.setdefault(n, []).append(f"{ref}.{pin}")
    singles = [
        n for n, nodes in nets.items() if len(nodes) == 1 and not n.startswith("unconnected-")
    ]
    print(f"{len(actual)} pins, {len(nets)} nets, mismatches: {errors}")
    if singles:
        print("single-node nets (check intent):", ", ".join(sorted(singles)))
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1]))

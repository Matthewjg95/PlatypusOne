# Perception Head Rev A — KiCad project

KiCad **9.0** (generated and checked with 9.0.9). Open
`perception_head_rev_a.kicad_pro`. Project libraries are referenced through
`sym-lib-table` / `fp-lib-table` (`${KIPRJMOD}/lib/...`); stock parts come from
the standard KiCad 9 libraries.

Status and gates: [../README.md](../README.md). **Pre-layout — no PCB file yet.**

## Sheets

| Sheet | File | Contents |
|---|---|---|
| root | `perception_head_rev_a.kicad_sch` | design intent, label conventions, sheet index |
| HOST_INTERFACE | `host_interface.kicad_sch` | J1 host cable, J2 Qwiic, TVS, bus pull-up options, TCA9534 |
| POWER | `power.kicad_sch` | shunts SH1–SH3, switchable 3.29 V ToF LDO, PWR_FLAGs |
| TOF | `tof.kicad_sch` | Pololu #3419 carrier, I2C strap, bus-B damping options |
| IMU | `imu.kicad_sch` | BMI270 + address option |
| ILLUMINATION | `illumination.kicad_sch` | TPS2553 switch, 2-ch linear CC sinks, light-board connectors, kill jumper |
| ID_CALIBRATION | `id_calibration.kicad_sch` | AT24CS32 + write protect |
| DEBUG_TEST | `debug_test.kicad_sch` | 31 test points, status LED, datum/camera holes, fiducials |

Label conventions: `TBD_*` host pin not frozen · `DNP_OPTION` footprint fitted,
part not populated (also the KiCad DNP attribute) · `BENCH_VERIFY` needs a
measurement. Each sheet carries its provisional decisions as on-sheet notes.

## Project library (`lib/`)

| Item | Source of geometry |
|---|---|
| `PlatypusOne:TPS2553DBV` symbol | TI SLVS841F pin table (not in KiCad 9 stock) |
| `PlatypusOne:BMI270` symbol | stock BMI160 symbol (identical pinout per BMI270 DS Table 22), ASCx/OCSB fixed to I/O |
| `PlatypusOne:Pololu_3419_VL53L8CX_Carrier` symbol + `_THT` footprint | Pololu schematic + dimension drawing (2024-02-14). 1×4 row order BENCH_VERIFY |
| `PlatypusOne:Bosch_LGA-14_3x2.5mm_P0.5mm_BoschLand` footprint | Bosch BMI270 DS §8.3 land pattern (stock footprint lands are 0.2 mm longer) |

Stock symbols/footprints used were cross-checked against the manufacturer
pinouts listed in [../RESEARCH_LEDGER.md](../RESEARCH_LEDGER.md) (TLV758P DBV,
TCA9534 TSSOP-16, AT24CS SOT23-5, TLV9062 SOIC-8, BCP56 SOT-223, JST GH/SH).

## Checks

```sh
kicad-cli sch erc --severity-all -o outputs/erc_report.txt perception_head_rev_a.kicad_sch
kicad-cli sch export netlist --format kicadsexpr -o /tmp/net.net perception_head_rev_a.kicad_sch
(cd tools && python3 check_netlist.py /tmp/net.net)   # netlist == generator intent
```

ERC 2026-10-05: **0 errors, 0 warnings** ([outputs/erc_report.txt](outputs/erc_report.txt)).
No ERC exclusions are in use. Netlist check: 242 pins / 50 nets, 0 mismatches.
CI runs both on every change under `hardware/pcb/` (`.github/workflows/kicad-erc.yml`).

`outputs/` also holds the schematic PDF and grouped BOM CSV for reviewers
without KiCad; regenerate them whenever the schematic changes.

## How the schematic was authored (read before editing)

The first pass was generated from `tools/gen_schematic.py` (circuit as data →
`.kicad_sch`) because it was produced without the KiCad GUI. Every pin is
connected through a short stub and a net label, which is ERC-checkable but not
pretty. **Once anyone edits the schematic in the KiCad GUI, the `.kicad_sch`
files become the source of truth: stop using the generator and delete the
netlist-check step from CI.** Until then, change `tools/gen_schematic.py` /
`tools/gen_library.py` and regenerate.

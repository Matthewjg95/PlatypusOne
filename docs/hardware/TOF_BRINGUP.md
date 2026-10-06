# VL53L8CX bring-up — issue #16 execution packet

Status: **proposed bench procedure; no hardware results**. Prepared 2026-09-26.
Matthew owns assembly, measurements, flashing and first power. Start here, then
fill [the evidence record](TOF_EVIDENCE_TEMPLATE.md). No BOM selection is made.

## Scope and audit

The current experiment is **Tab5 → rear M5-Bus → M024 → Pololu #3419**.
Confirm the actual carrier markings before proceeding; this does not apply to
a bare IC or another breakout. An unspecified external regulator is not an
approved substitute. Its part number, circuit, input, output and enable state
must be established before use.

- PlatypusOne main at `e931fd7`: hardware BOM prefers L8CX; power-reference
  ToF load remains TBD; architecture assigns production sensor I/O to the MCU
  and exposes sensors through ISensorHub. No L8CX driver exists there.
- [Project Platypus PR #1](https://github.com/Matthewjg95/project-platypus/pull/1)
  owns the existing Tab5 wiring plan, test stages and raw evidence. Its
  `docs/TOF_RANGING.md` and `docs/TOF_TEST_PLAN.md` are proposed, not bench proof.
- [PlatypusOne PR #15](https://github.com/Matthewjg95/PlatypusOne/pull/15)
  proposes the downstream gate; it is unmerged. PR #20 is unrelated housekeeping.
- This packet consolidates the first-ranging subset for issue #16. The table
  below is the execution authority **for this packet**, consistent with PR #1.
  If upstream wiring changes, stop and reconcile rather than combine revisions.
- Standalone firmware replaces the Tab5 application while testing. Save its
  current binary/configuration first. It is not the display-client firmware or
  a UNO Q production sensor implementation. Unit V stays on Grove G53/G54.

## Authoritative wiring table

Use official connector numbering, not apparent left/right in a photograph.
The mating M024 view can be mirrored. Map actual pads with continuity while
all power and batteries are disconnected. Wire colors are operator-recorded.

| Carrier terminal | M024 / Tab5 destination | State / verification |
|---|---|---|
| VIN | M5-Bus **12 / 3V3** | Only supply input for this configuration |
| GND | M5-Bus **1, 3 or 5 / GND** | Pick one; record it and verify return |
| SDA/MOSI | M5-Bus **17 / GPIO31 / internal SDA** | Shared internal bus |
| SCL/MCLK | M5-Bus **18 / GPIO32 / internal SCL** | Shared internal bus |
| SPI/I2C | Carrier GND | Strap low before power; default is SPI |
| LP | Open | Carrier pull-up enables I2C |
| INT, SYNC, CS, MISO | Open | Not used by polling scaffold |
| AVDD, CORE/IOVDD | No external connection | Regulator outputs; measurement only |
| M5-Bus 28 / 5V | **No connection to carrier** | Would raise host logic voltage |
| M5-Bus 25/27/29 HVIN, 30 BAT | **No connection** | Not this power path |

Keep signal leads short (target <8 cm), secure against movement, and add no
pull-ups by default. No cover window for first ranging. Remove any shipping
liner from the optical aperture. Do not probe adjacent live connector pins;
use secured, labeled test pads with the power off before attaching clips.

## Voltage assumptions and unresolved facts

| Item | Basis / required action |
|---|---|
| Carrier VIN | Vendor range 3.2–5.5 V; this hookup uses nominal 3.3 V because I/O follows VIN |
| AVDD | Nominal 3.3 V; vendor warns of dropout below ~3.4 V VIN; must stay ≥3.13 V |
| CORE/IOVDD | Nominal 1.8 V; use 1.71–1.89 V as conservative bench screen; confirm exact revision specification if outside it |
| SDA/SCL high | Must match the 3.3 V bus, never 5 V. DMM during traffic shows an average, not logic-high or ringing |
| Tab5 rail capacity | External allowance is not established; unloaded voltage alone does not prove load capacity |
| Current | Vendor typical 100 mA, peaks 150 mA; measure actual incremental load and capture source/current-limit settings |
| Regulator alternative | P/N, dropout, transient response, grounding and backfeed prevention unknown; blocked pending separate circuit review |
| Bus loading/revision | Actual pull-ups, waveform, other addresses, display-driver revision and firmware coexistence unverified |

For this trial use a **3.20–3.40 V loaded VIN screening band**, subject to the
board's limits and instrument uncertainty. AVDD minimum still applies; passing
VIN does not override it. If meter uncertainty overlaps a boundary, do not call
it a pass. Scope VIN/AVDD if available; a DMM cannot establish transient margin.
Never parallel an external regulator output with pin 12. A separate carrier
supply also requires power-sequencing/backfeed review before connection to SDA/SCL.

## Execution sequence and stop gates

### 0 — prepare offline

1. Record board/carrier revision, firmware and equipment; photograph both sides
   and connector orientation. Save current Tab5 firmware/restore method.
2. Build the [standalone sketch](../../firmware/tof_bringup/README.md) before
   assembly. Build failure is a software gate, not a reason to change wiring.
3. Disconnect USB, battery and all other power; verify zero volts before ohms.
4. On disconnected M024/sensor wiring, verify each table connection end to end;
   record resistance and meter-lead baseline, not just a beep.
5. Inspect solder joints and verify no unintended low resistance between VIN–GND,
   SDA–SCL, each signal–GND, each signal–5V, VIN–5V/HVIN/BAT. Capacitors and
   semiconductor paths can give changing readings; investigate sustained near-zero
   resistance rather than imposing one universal resistance threshold.
6. Verify SPI/I2C–GND continuity, regulator outputs unconnected and no loose strands.

**Gate E0:** all nets identified, shorts resolved, correct carrier, photos saved.

### 1 — disconnected baseline and rails

1. Install only M024 while off, carrier disconnected. Boot the sketch; send `b`
   in a 115200-baud serial monitor. Save baseline addresses and test touch/IMU
   using the known-good firmware if necessary before restoring this sketch.
2. Measure 3V3, SDA and SCL to the verified ground. Capture idle levels with a
   scope if available; internal traffic can prevent a meaningful DMM idle reading.
   An existing 0x29 ACK is an address-conflict stop gate.
3. Record Tab5 baseline current, supply and display setting. Power off fully.
4. Attach verified carrier. Apply power without sending `r`. Measure VIN,
   AVDD and CORE/IOVDD at the carrier; record current and any heating/reset.
   Start with a monitored source; do not guess a total-Tab5 current limit from
   the sensor-only 150 mA figure. If no rail-capacity evidence exists, this is a
   supervised characterization trial, not approval for permanent installation.

**Gate E1:** rail screens pass; no heating, resets or bus-level conflict. Stop
immediately for abnormal behavior. Measurements and wiring remain Matthew's gate.

### 2 — first range

1. Open serial capture before sending `r`; place a broad matte target at 500 mm,
   normal to sensor axis. Record reference from sensor optical plane, tape/ruler
   uncertainty, target dimensions, lighting and mounting.
2. `r` scans, then uploads sensor firmware and starts 4×4 at 10 Hz, 400 kHz.
   Initialization can take seconds; do not infer failure from a short pause.
3. Save at least 100 frames. Recheck loaded VIN/AVDD/current during ranging.
4. Continue 30 minutes; exercise touch and move the board to check IMU behavior
   using coexistence instrumentation before declaring the upstream stage passed.
   This minimal sketch services `M5.update()` but does **not** measure IMU gaps
   or touch latency. Record manual responsiveness and leave quantitative gaps
   unresolved. No background task may independently access this bus.
5. Stop by removing power after capture. A HALT/TIMEOUT requires investigation
   and a new run ID; the sketch does not silently recover or erase failure.

**Gate E2:** 100 consecutive frames then 30-minute run with no reset, I2C error,
lockup or unexplained timeout; measured frame rate and touch/IMU limitations
recorded. Do not call requested 10 Hz the achieved rate.

### 3 — repeatability, invalid data and FOV

- At 100, 500 and 1000 mm: broad matte target, fixed mount, 100 frames per
  distance after 2 seconds settling; three independent power cycles, same setup.
  If 100 mm cannot fill the FOV cleanly, document target/fixture geometry.
- Use central 4×4 zones 5, 6, 9, 10 for axial reference comparison; retain every
  other zone. Off-axis ranges to a plane need not equal axial distance.
- Per zone: strict-valid fraction, median, bias versus reference, P5–P95 spread,
  sample count. Across cycles: range of per-cycle medians. No valid samples means
  **unresolved**, never zero distance. Status 5 plus target count >0 is the
  conservative quality gate; keep all other statuses for review, including 6/9.
- Proposed engineering screening gates, not datasheet accuracy certification:
  ≥95% strict-valid central samples on matte targets; central absolute bias
  ≤max(20 mm, 5% of reference); P95–P5 ≤20 mm; cycle-median spread ≤20 mm.
  Record failures and review suitability; never adjust gates after seeing data
  without retaining the original outcome and rationale.
- Dark target, glossy oblique target, bright-window condition, and open/out-of-range
  scene: 100 frames each. Retain invalid statuses and anomalous valid-looking
  returns; a reflective failure need not produce an invalid status.
- FOV sanity: move a small foreground card left/right/up/down against a far wall.
  Record which native zone indices change, orientation photo and video. Do not
  assume displayed grid orientation equals physical direction. Test a mixed-depth
  edge; this one-target build cannot characterize multiple returns within a zone.
- Only after E2, a separate configuration may use 8×8 at 10 Hz: change resolution
  constant AND zone loop bound to 64, record a new firmware hash and repeat E2
  and quality tests. Keep all 64 zones; do not overwrite 4×4 data.

## Expected logs and failure signatures

| Output / symptom | Meaning and next check |
|---|---|
| READY then `ack,0x29` only with carrier | Discovery; not proof of ranging |
| INIT_BEGIN → event init/resolution/frequency/start status 0 | ULD configuration completed |
| `zone,...` rows, indices 0–15 per frame | Native-order raw first-target data; mm and ULD-converted quality values |
| HALT,no_0x29 | Power off; check SPI/I2C strap, LP, ground, swapped nets and baseline address inventory |
| HALT,unexpected_internal_bus | Wrong board/core detection; do not repin blindly |
| Nonzero event status / io_errors | Transaction/ULD failure; check rails, bus loading, short wires, dependency revision |
| TIMEOUT after >2 s with no completed frame | Preserve log; inspect sensor state, bus and power before a new run |
| Repeated boot banner | Reset/power instability or host auto-reset; correlate rail and serial-open events |
| Good ACK, bad distances/statuses | Check liner, target coverage, light, reflections and optical obstruction |
| Touch/display fails | Shared-bus or board-library problem; restore baseline and compare |
| No serial output | Verify port, USB CDC build option, baud and boot; do not treat this as sensor failure |

Rows: `zone,frame,ms,read_ms,index,targets,status,mm,sigma_mm,signal_kcps_spad,ambient_kcps_spad,valid,io_errors`.
`ms` is host uptime after read, not UTC or exposure timestamp. `read_ms` excludes
serial output. Frame rate = (unique frames − 1)/(last ms − first ms) × 1000.
A boot resets counters: preserve complete serial output and separate run IDs.

## Evidence and PlatypusOne promotion

Use Project Platypus `docs/evidence/tof/<date>-<run>/` for raw records, per PR #1;
link immutable commit URLs here after review. Until that branch is merged, retain
records in its experiment branch without overwriting previous captures. No raw
measurements are supplied by this package.

E0/E1/E2 passing permits further characterization, not product graduation.
PlatypusOne graduation requires: electrical/bus proof on the intended **UNO Q**
host and final supply; complete 8×8 quality/failure evidence; measured power
entered in POWER_BATTERY_REFERENCE; stable camera/ToF calibration; a demonstrated
workflow benefit versus camera-only; raw status/provenance preservation through
ISensorHub and the observation contract; and an explicit owner-reviewed decision
(reject / continue / accept bounded use) recorded in an ADR. Tab5 success alone
cannot close those gaps. No VL53L9CX or enclosure work is part of this packet.

## Sources checked 2026-09-26

- [Pololu #3419 electrical/pin documentation](https://www.pololu.com/product/3419)
- [M5Stack Tab5 pin map and library requirements](https://docs.m5stack.com/en/core/Tab5)
- [M024](https://docs.m5stack.com/en/module/bus)
- [ST ULD Arduino source](https://github.com/stm32duino/VL53L8CX/tree/a93a9d6796f2a74835a4088f225daec343153c62)
- [ST UM3109](https://www.st.com/resource/en/user_manual/um3109-a-guide-for-using-the-vl53l8cx-lowpower-highperformance-timeofflight-multizone-ranging-sensor-stmicroelectronics.pdf)

Vendor specifications are not project measurements. Firmware validation status
and remaining build gates are in its README.

## Calculate summaries without altering evidence

For each complete, single-boot capture (no terminal timestamp prefixes):

```sh
python tools/tof_bringup/summarize.py serial.log --reference-mm 500 > summary.json
```

Save the result under the run's `derived/` directory. The helper rejects incomplete
frames and repeated frame IDs, retains status histograms, and returns null for
zones with no strict-valid samples. It does not issue an automatic pass or merge
power cycles. Compare the three per-cycle medians in the evidence record. Keep
startup/failure lines, original logs and SHA256s. A logger-added prefix must be
removed only into a separate derived input, preserving the original.

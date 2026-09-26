# ToF bench evidence — copy per run, never fill by inference

- Run ID / UTC date / operator:
- Procedure revision / firmware commit / binary SHA256:
- ULD commit / M5Unified / M5GFX / board core / FQBN / compile log:
- Tab5 revision and display IC / M024 revision / carrier markings:
- Source / current limit / regulator P/N if applicable / wiring lengths:
- Meter/scope model, range, uncertainty / reference-distance method & uncertainty:
- Target material, dimensions, optical-plane reference, ambient light, warmup:
- Photos (front/back, numbered connector, wiring, target alignment):
- Raw serial log / video / scope traces / configuration path:

## Electrical record (never measure resistance on powered circuitry)

| Check | Expected / criterion | Actual | Pass / fail / unresolved |
|---|---|---|---|
| Each connected net | Matches wiring table; attach measured net list | | |
| Unintended shorts | None; attach ohms readings + lead baseline | | |
| Baseline I2C inventory | No pre-existing 0x29 | | |
| VIN, baseline / idle / ranging min | 3.20–3.40 V trial screen | | |
| AVDD, idle / ranging min | ≥3.13 V; dropout recorded | | |
| CORE/IOVDD | 1.71–1.89 V trial screen | | |
| SDA / SCL high / waveform | 3.3 V domain; no 5 V | | |
| Total baseline / total ranging / incremental current / peak | Measured, not assumed | | |
| Heating / reset / display / touch / IMU | No new malfunction | | |

## Range and repeatability summary

| Run / power cycle | Reference mm | Zone | Frames / strict-valid % | Median mm | Bias mm | P5–P95 mm | Gate result |
|---|---:|---:|---|---:|---:|---|---|
| | | | | | | | |

- Three-cycle median spread by zone:
- 30-minute elapsed / frames / achieved Hz / timeouts / I2C errors / resets:
- Invalid status histogram (include zero-target samples):
- Dark / reflective / bright / out-of-range outcomes:
- FOV card positions → changed native indices (photo-linked):
- Touch/IMU manual check / quantitative latency or gap evidence still missing:
- Derivation command/version and raw-file SHA256s:

## Decision (owner to complete)

- E0 wiring: pending / pass / fail — evidence:
- E1 rails: pending / pass / fail — evidence:
- E2 stability: pending / pass / fail — evidence:
- Quality and repeatability: pending / pass / fail — evidence:
- Recommendation: reject / continue / submit bounded use for review
- Supported operating conditions / failures / unresolved claims:
- Next single experiment:
- Reviewer / date / linked PlatypusOne decision (if any):

Observed: original rows, statuses, rails, photos. Derived: valid fractions,
medians, bias, spread and rate with source links. Inferred: suitability claim.
Unresolved: absent tests, calibration, timestamps and host integration.
Never use sensor-reported mm resolution as an accuracy claim.

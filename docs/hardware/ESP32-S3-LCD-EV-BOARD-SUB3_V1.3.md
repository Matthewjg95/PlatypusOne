# ESP32-S3-LCD-EV-Board-SUB3 v1.3 — PlatypusOne Display Evaluation

Status date: **2026-08-24**

## Summary

The Espressif **ESP32-S3-LCD-EV-Board-SUB3 v1.3** is a strong prototype display candidate for PlatypusOne. It provides a substantially more product-like UI target than the current 3.2-inch 320×240 SPI display in the planning BOM.

Recommended role:

- **Prototype / UI reference display:** YES
- **Production display assembly:** NOT YET LOCKED
- **Reference UI resolution:** 800×480
- **Production strategy:** preserve the logical 800×480 RGB + capacitive-touch target, then select/integrate the final production panel directly once enclosure, supply-chain, and electrical constraints are known.

## Known display characteristics

| Attribute | Value |
|---|---|
| Display family | ESP32-S3-LCD-EV-Board-SUB3 |
| Board revision | v1.3 |
| Diagonal | 4.3 in |
| Resolution | 800×480 |
| Pixel interface | Parallel RGB |
| Pixel format target | RGB565 |
| LCD controller | ST7262E43 |
| Touch | Capacitive |
| Touch controller | GT1151 |
| Reference host | ESP32-S3-LCD-EV-Board-2 |

## Why prototype with it

1. **800×480 is a useful product UI resolution.** It supports real navigation, status areas, graphs, device controls, setup screens, and large touch targets without designing the software around an artificially constrained prototype screen.
2. **Parallel RGB is a better appliance-style graphics path than a small SPI TFT.** It is a more appropriate reference for smooth UI updates and LVGL-style rendering.
3. **Capacitive touch is closer to the intended user experience** than the resistive-touch display currently listed in the planning BOM.
4. **Espressif provides board support**, reducing early display/touch bring-up risk.
5. **The UI work can survive a later panel change** if PlatypusOS treats 800×480 RGB565 + capacitive touch as the logical reference target rather than coupling software to this exact carrier PCB.

## Architecture recommendation

### Prototype phase

Use the SUB3 v1.3 to establish:

- 800×480 layout and typography
- touch interaction model
- navigation and launcher behavior
- LVGL / renderer performance targets
- framebuffer and anti-tearing behavior
- UI memory/performance budget

### Production hardware phase

Do **not** automatically design the production enclosure or PCB around the complete Espressif SUB3 carrier assembly.

Instead, evaluate a production 4.3-inch 800×480 RGB capacitive-touch panel that can be integrated directly with PlatypusOne hardware. This gives control over:

- enclosure depth
- mounting geometry
- connector placement
- BOM cost
- long-term sourcing
- touch/display cable routing

The software abstraction should therefore target display capabilities, not this exact module identity.

## Mechanical notes — supplied standoffs

Espressif's SUB3 v1.3 PCB drawing identifies four mounting positions as **M1–M4**. These are component/reference designators and are **not confirmation of metric screw size**.

An authoritative Espressif source specifying the thread of the supplied brass standoffs has not yet been located.

Current status:

- **Thread size: UNVERIFIED**
- Likely candidates based on this class of hardware: M2.5 or M3
- M2.5 should be checked first, but must not be treated as confirmed

### Verification procedure

Measure the male thread major diameter with calipers or gently test a known metric screw by hand:

| Approx. measured OD | Likely thread |
|---:|---|
| 1.9 mm | M2 |
| 2.4 mm | M2.5 |
| 2.9 mm | M3 |

Do not force a screw that starts tight. Once physically verified, replace `UNVERIFIED` above with the confirmed thread and add the standoff body length / male-thread length if useful for enclosure CAD.

## Relationship to current PlatypusOne BOM

The current planning BOM lists a **3.2-inch ILI9341 320×240 SPI TFT with XPT2046 resistive touch** as the initial display.

This document records the SUB3 as a higher-fidelity prototype candidate. Do not silently delete the older option until the hardware architecture is updated deliberately; the two displays exercise materially different interfaces and UI constraints.

Suggested decision gate:

> If the 4.3-inch mechanical envelope is acceptable for PlatypusOne, adopt **800×480 capacitive touch** as the reference UI target and prototype on the SUB3 v1.3.

**Gate outcome (2026-08-24):** partially taken in
[ADR-0001](../adr/0001-dynamic-linked-prototype-display.md). 800×480 is adopted
as the **UI design reference**, and this panel is one of two linked-prototype
candidates — but no production panel is selected, and the software targets
whatever geometry the display reports at runtime rather than this resolution
specifically. The owner refers to the second candidate as the
"ESP32-S3-Korvo-1" 4.3-inch LCD; confirming whether that is this SUB3 carrier
or a different Espressif board is an open item in the ADR.

## References

- Espressif ESP32-S3-LCD-EV-Board documentation / user guide
- Espressif ESP32-S3-LCD-EV-Board-SUB3 v1.3 PCB drawing
- Espressif BSP for ESP32-S3-LCD-EV-Board

## Open items

- [ ] Physically verify supplied standoff thread size
- [ ] Record standoff body length and male-thread length
- [ ] Confirm exact visible area and active-area dimensions for enclosure work
- [ ] Bench-test touch latency and display update performance
- [ ] Decide whether 4.3-inch / 800×480 becomes the PlatypusOne reference UI target
- [ ] Identify at least two production-grade 4.3-inch RGB capacitive-touch panel sources before locking enclosure geometry

# PlatypusOne camera + ToF test plate (v0)

One flat, removable plate for side-by-side Arducam B0394 IMX219 and Pololu #3419 VL53L8CX modules. A bench fixture, not an enclosure lid. The two outer M3 slots are 68 mm apart horizontally and can be used to attach it to a stand or later shell. This design does not set the enclosure aperture.

## Print and assemble
- Print the 80 × 43 × 3.2 mm STL flat, with no supports. Start with PLA/PLA+, a 0.4 mm nozzle, 0.20 mm layers and 3 or 4 walls. Do not rescale the STL.
- From the optics side, the orientation triangle points up, the B0394 is on the left and the Pololu board on the right. Both optical faces point away from the plate. The camera ribbon exits down through the open notch. With the Pololu board component-side up and its two screw holes on the right, the screw slot pitch is 17.9 mm.
- Use M2.5 camera screws and M2 ToF screws with insulating standoffs/washers and nuts. Start with 4–6 mm spacers; choose height and screw length after inspecting the fitted connector, solder tails and ribbon. Use a small spacer OD where camera components are close. Seat on equal-height spacers on each board, snug by hand, and avoid compressing or bending the PCBs. If the camera holes do not line up, stop and measure; the short horizontal slots are only for a small fit adjustment.
- Keep the lens and ToF apertures uncovered. Keep the ribbon clear of the latch and give it a gentle bend. Tape down cable slack outside the sensor field and avoid loading the board connectors.
- First fit: verify four camera holes and both ToF holes, cable access, and no backside contacts. Aim at a matte wall or square target at two distances and check both views. Record actual optical center offsets and ToF zone rotation before calibration. The model's 33 mm nominal horizontal offset is a starting point, not measured alignment. If using the B0393 camera variant instead, measure its mounting and cable orientation before assuming compatibility.

## Source and verification
- [Arducam B0394 dimensions](https://www.arducam.com/8mp-imx219-camera-module-for-raspberry-pi-b0394.html) and [mechanical drawing, page 2](https://m.media-amazon.com/images/I/81zqnLoBi2L.pdf): nominal 21 × 12.5 mm hole pattern, M2.5.
- [Pololu #3419 drawing](https://www.pololu.com/file/0J2032/vl53l8cx-carrier-dimensions.pdf): 12.7 × 22.9 mm outline, two 2.18 mm holes 2.5 mm in from the short ends, 17.9 mm on center. Its 11.4 mm dimension locates the sensor, not the second hole.
- OpenSCAD export checked: single connected watertight mesh with the intended dimensions. No print, board fit, or optical alignment verified yet.
- Regenerate: `openscad --export-format binstl -o camera_tof_test_plate.stl camera_tof_test_plate.scad`.

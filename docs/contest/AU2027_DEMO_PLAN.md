# PlatypusOne: AU 2027 demo plan

Working shot list for [issue #60](https://github.com/Matthewjg95/PlatypusOne/issues/60). Use the [existing Hackster project](https://www.hackster.io/matthew-garza/platypusone-88fa10); this file does not represent publication or submission. Target Dec 17, 2026; [official cutoff](https://www.hackster.io/contests/autodesk-university-2027-product/rules) Dec 20 at 11:59 PM Pacific.

## One job to demonstrate

“When all I have is the part in front of me, I want a quick measurement I can check later. PlatypusOne saves the original image alongside the result so I can see how it got there.”

1. Dry-run with the working 5-inch Waveshare screen. Pick a matte flat part and independently measure a feature with calipers. Put the scale reference and part in frame.
2. In a continuous take on the PCBWay-enclosed prototype, show point → capture → result → save. Use the touchscreen if the trigger is not validated. Compare the visible result with the caliper value and open the saved image and record with the same observation ID.
3. Before closing the shell, take readable wiring and mounting photos. Record the tested software commit, firmware, actual camera and installed sensors, power source, PCBWay fabrication method, and Fusion revision. Take separate photos of the prototype in action for the entry.
4. If an actual DXF export works reliably on the filmed build, it is optional. In Fusion, use Insert DXF with units set to millimeters; verify a known span and that the sketch profile closes before filming it.

## Page update

- Start BOM with UNO Q, UNO Media Carrier, and Waveshare 5-inch DSI panel (SKU 18396). Fill in the camera, used sensors, power and PCBWay shell from the filmed unit. List the internal battery/charger or custom carrier as roadmap items if not installed.
- Add the matching schematic or readable wiring photos, build instructions and the tested source reference. Export and reopen the native `.f3d` matching the shell sent to PCBWay. If only `.f3z` is offered, resolve linked components or machines in a separate submission copy, not the working assembly.
- The [October washer capture](../hardware/evidence/scout-2026-10-02-washer/RUN_RECORD.md) is prior development evidence. It saved an image/record but misidentified the washer. Keep caliper truth, the live result and the later offline fit separate. It is not validation of the filmed revision.

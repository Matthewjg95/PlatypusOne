# ADM2587EBRWZ Component Bring-Up Dossier

Status: **RECEIVED — 2 units; research/de-risk**
Disposition: **RESEARCH / DE-RISK — dedicated isolated industrial RS-485 module**
Contest request BOM: **excluded**
Contest Core carrier: **excluded unless a recorded scope change promotes it**
Preferred integration: **post-contest daughterboard/accessory, not an automatic main-carrier footprint**

## 1. Exact device

| Field | Value |
|---|---|
| Manufacturer | Analog Devices |
| Orderable part | **ADM2587EBRWZ** |
| Package | RW-20, 20-lead wide-body SOIC, tube variant |
| Temperature range | −40°C to +85°C |
| Quantity received | 2 |
| Function | Signal- and power-isolated RS-485/RS-422 transceiver |
| Maximum data rate | **500 kbps** |
| Supply | Single 3.0–5.5 V rail; designed for 3.3 V or 5 V operation |
| Isolation rating | 2500 V rms for 1 minute; observe the exact safety approvals and working-voltage limits in the current datasheet |
| Moisture sensitivity | **MSL 3 (168 hours)** per distributor product data; verify the bag label before assembly |

The integrated iCoupler signal isolation and isoPower isolated dc-to-dc converter remove the need for a separate isolation supply for the transceiver. The bus may be configured for half- or full-duplex operation.

## 2. PlatypusOne role

This part can give a future PlatypusOne module a robust, galvanically isolated interface to industrial equipment, remote sensor nodes, motor-control hardware, or long cabled links. It is particularly useful when the host must not share ground with field equipment.

It does **not** improve the bounded contest demonstration enough to justify its isolation geometry, EMI work, connector space, and validation schedule. The right first home for it is a dedicated RS-485 daughterboard whose isolation barrier, protection, termination, and cable interface can be evaluated independently.

Two samples enable a useful first system test: one isolated endpoint at each end of a twisted-pair link.

## 3. Electrical facts that drive the design

- ADM2587E maximum data rate is 500 kbps.
- Logic-side operation is supported from a single 3.3 V or 5 V supply.
- Typical supply current depends heavily on supply and bus load; the current datasheet shows values up to roughly 125 mA in specified loaded conditions. Budget with margin from the applicable worst case, not a no-load bench observation.
- DE is active high; RE is active low.
- Receiver inputs are fail-safe for open and shorted lines.
- The device supports half- or full-duplex buses and up to 256 nodes under the stated loading model.
- The wide-body package provides 7.5 mm minimum external creepage and clearance at the package, but the PCB and connector layout must preserve the system's required barrier.
- The part can dissipate about 650 mW fully loaded and relies on PCB copper and ground pins for heat removal.
- The isolated supply is primarily for the internal bus-side circuitry. Any external VISOOUT load reduces thermal and EMI margin; do not treat it as a general isolated auxiliary rail.

## 4. Required support and layout study

Use the Rev. H datasheet reference circuit and the relevant evaluation-board layout as the starting point. At minimum, account for:

- all specified local 0.01 µF, 0.1 µF, and 10 µF decoupling/reservoir capacitors;
- the external VISOOUT-to-VISOIN connection;
- the recommended ferrite beads and their exact side-of-capacitor placement;
- split GND1 and GND2 reference regions;
- isolation keepouts on every layer as required by the chosen safety architecture;
- termination sized and placed for the actual bus topology;
- short stubs and a twisted-pair field connection;
- test points for TxD, RxD, DE, RE, VCC, VISOOUT, A/B and Y/Z as applicable;
- optional configurable half/full-duplex links and 120 Ω termination;
- surge/ESD protection selected for the real cable environment rather than assuming the IC's internal ESD rating is full system protection.

The integrated isoPower converter switches at high frequency. ADI calls out emissions around the 180 MHz primary switching frequency and 360 MHz secondary rectification frequency and provides specific two- and four-layer suppression layouts. Footprint correctness alone is not sufficient: component placement, ferrites, reference-plane geometry, stitching capacitance, and keepouts are functional parts of this circuit.

## 5. Handling and home-storage instructions

The ±15 kV rating applies to specified RS-485 pins under a test model; it does **not** make the loose IC generally ESD-proof. Other pins are rated much lower, and ADI still classifies the device as ESD sensitive.

Until a home ESD station exists:

1. **Leave both parts in the original closed ESD/moisture-barrier packaging.**
2. Store the package in a rigid container away from carpet, foam, plastic bags, pets, fabric, and rapid humidity/temperature changes.
3. Do not open it merely to photograph the bare IC; photograph the external label first.
4. Do not transfer it into an ordinary zipper bag or loose parts drawer.
5. Do not count “touching something metal” as an ESD-control process.

Minimum practical home setup before opening:

- dissipative ESD mat;
- wrist strap connected to the mat's common point through the proper safety resistance;
- known protective-earth connection using a purpose-built ESD grounding lead;
- ESD-safe grounded soldering iron;
- ESD-safe tweezers and component trays;
- avoid very dry conditions and synthetic clothing;
- keep the board, tools, operator, and component at the same controlled potential.

**Moisture control:** MSL 3 permits a limited floor-life clock after a properly dry-packed bag is opened. Record the opening date/time and bag-label conditions. Reseal unused parts with fresh desiccant in a suitable moisture-barrier bag. If the exposure history exceeds the label/J-STD-033 allowance or is unknown before reflow, use an approved bake/repack procedure appropriate to the package and packing materials; do not improvise with a kitchen oven.

## 6. Likely kill or failure mechanisms

- ESD discharge into logic, supply, enable, or ground pins while loose.
- Applying logic above VCC or operating outside the 3.0–5.5 V recommended supply range.
- Shorting the isolation barrier through copper pours, mounting hardware, shields, test equipment, or connector grounds.
- Treating the 2.5 kV proof-test rating as permission to prototype directly on hazardous mains.
- Missing or poorly placed bypass capacitors/ferrites, producing unstable operation or excessive emissions.
- Bus contention, incorrect termination, or long stubs.
- Excessive external load on VISOOUT.
- Reflow after excessive moisture exposure.
- Incorrect A/B or Y/Z naming assumptions across vendors and connectors.
- Connecting an earth-referenced oscilloscope ground across the isolation barrier without understanding the resulting path.

## 7. First bring-up plan

Initial tests remain extra-low-voltage and current-limited. The isolation feature does not require hazardous voltage to validate communications.

1. Keep the ICs sealed until the daughterboard, parts, and ESD workspace are ready.
2. Build or assemble two reference-layout-based nodes.
3. Inspect orientation, barrier spacing, ferrite placement, and all decoupling before power.
4. Measure resistance to ground and across the isolation barrier with power off.
5. Power one node from a current-limited 3.3 V supply; verify VCC and VISOOUT before attaching a cable.
6. Confirm DE/RE defaults and local TxD/RxD behavior.
7. Connect two nodes over short twisted pair with intentional termination.
8. Exercise half-duplex at low rate, then increase to 500 kbps while recording errors and current.
9. Repeat with longer cable and controlled noise sources; log error rate, current, temperatures, and waveform quality.
10. Only after the low-voltage link is understood should system-level protection, common-mode tests, or industrial equipment be considered.

## 8. Promotion gates

Do not add the ADM2587E to the contest Core carrier unless all are true:

- a contest workflow requires isolated wired communications;
- that workflow passes the Core scope-change test;
- the connector and cable strategy are defined;
- the reference-layout EMI approach fits the board stackup and enclosure;
- the isolation barrier and safety assumptions are reviewed;
- two-node bring-up is complete;
- the schedule impact is accepted in a recorded decision.

For post-contest use, prefer a removable module so isolation, EMI, protection, and field connectors remain independently testable.

## 9. Open questions

- What exact industrial protocol and connector should the first module target: generic UART-over-RS-485, Modbus RTU, or a project-specific framing layer?
- Is half-duplex sufficient for the first node?
- What maximum cable length and baud rate matter for the target workflow?
- What surge/EFT environment must the connector withstand?
- Will a newer recommended-for-new-designs ADM256xE family part replace this device in a production revision after the received samples are used for learning?
- What assembly process will be used, and what does the physical bag label state for MSL, seal date, desiccant, and humidity indicator?

## 10. Primary references

- [ADM2587E product page](https://www.analog.com/en/products/adm2587e.html)
- [ADM2582E/ADM2587E Rev. H datasheet](https://www.analog.com/media/en/technical-documentation/data-sheets/adm2582e-2587e.pdf)
- [AN-1349: PCB implementation guidelines](https://www.analog.com/en/resources/app-notes/an-1349.html)
- [UG-044: four-layer emissions-compliant evaluation board](https://www.analog.com/UG-044)
- [AN-0971: control of radiated emissions with isoPower devices](https://www.analog.com/en/resources/app-notes/an-0971.html)

## 11. Evidence still to attach

- [ ] External bag and label photos
- [ ] Date/source received
- [ ] Bag seal, desiccant, humidity-card, and MSL-label condition
- [ ] Marking/orientation photo when first opened under ESD control
- [ ] Selected reference schematic/layout revision
- [ ] First-node and two-node bring-up logs

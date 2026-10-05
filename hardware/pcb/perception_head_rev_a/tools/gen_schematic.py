# ruff: noqa: E501 -- long human-readable schematic notes and part fields are data
"""Bootstrap generator for the Perception Head Rev A KiCad schematic.

WHY THIS EXISTS: the first schematic pass was authored without the KiCad GUI.
The circuit (parts, nets, provisional labels, notes) is declared below as data
and emitted as hierarchical .kicad_sch files that KiCad 9 opens natively and
that `kicad-cli sch erc` checks.

OWNERSHIP RULE: once anyone edits the schematic in the KiCad GUI, the
.kicad_sch files become the source of truth and this script is retired (do not
re-run it over hand edits). Until then, edit the data here and regenerate.

Connection style: every pin gets a short wire stub ending in a net label
(local label inside a sheet, global label for nets that cross sheets, GND
symbol for ground). That is deliberate for a pre-layout netlist-first pass;
tidy wiring is a GUI task once the architecture stops moving.
"""

import os
import uuid
from pathlib import Path

from sexpr import Sym, dumps, find, first, parse

HERE = os.path.dirname(os.path.abspath(__file__))
PROJ_DIR = os.path.normpath(os.path.join(HERE, ".."))
PROJECT = "perception_head_rev_a"
STOCK = "/usr/share/kicad/symbols"
LIBS = {"PlatypusOne": os.path.join(PROJ_DIR, "lib", "PlatypusOne.kicad_sym")}

NS = uuid.UUID("6f1c2a52-6d0b-4c47-9a53-3a1e0c1d7a01")


def uid(*parts):
    """Deterministic UUIDs so regeneration produces stable diffs."""
    return str(uuid.uuid5(NS, "/".join(str(p) for p in parts)))


ROOT_UUID = uid("root")

# --------------------------------------------------------------------------
# Nets that cross sheets are global labels; everything else is local. GND is a
# power symbol. Rails are global labels plus PWR_FLAG where only passives or
# connectors drive them.
# --------------------------------------------------------------------------
GLOBAL = {
    "+5V_HOST",
    "+5V_HEAD",
    "+5V_ILLUM",
    "+3V3_HOST",
    "+3V3_LOGIC",
    "+3V3_TOF",
    "TOF_LDO_OUT",
    "TOF_AVDD",
    "TOF_CORE_1V8",
    "I2C_A_SCL",
    "I2C_A_SDA",
    "I2C_B_SCL",
    "I2C_B_SDA",
    "TOF_INT_N",
    "TOF_SYNC",
    "TOF_LPN",
    "TOF_PWR_EN",
    "TOF_I2C_RST_OPT",
    "IMU_INT1",
    "IMU_INT2",
    "ILLUM_SET",
    "ILLUM_VSET",
    "ILLUM_EN",
    "ILLUM_EN_REQ",
    "ILLUM_FAULT_N",
    "ILED1_SENSE",
    "ILED2_SENSE",
    "EEPROM_WP",
    "EXP_INT_N",
    "STATUS_LED",
}
NC = None  # explicit no-connect

# Footprint shorthands
R0603 = "Resistor_SMD:R_0603_1608Metric"
R0805 = "Resistor_SMD:R_0805_2012Metric"
R1206 = "Resistor_SMD:R_1206_3216Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C0805 = "Capacitor_SMD:C_0805_2012Metric"
TP_PAD = "TestPoint:TestPoint_Pad_D1.5mm"
TP_LOOP = "TestPoint:TestPoint_Keystone_5019_Miniature"


def R(ref, val, a, b, pos, fp=R0603, dnp=False, note="", spec="1% 0.1W"):
    if val == "0":
        spec = "0 ohm jumper"
    return dict(
        ref=ref,
        lib="Device:R",
        value=val,
        fp=fp,
        pos=pos,
        conns={"1": a, "2": b},
        dnp=dnp,
        fields={"Spec": spec, "Note": note},
    )


def C(ref, val, a, b, pos, fp=C0603, dnp=False, note="", spec="X7R/X5R, >=10 V"):
    return dict(
        ref=ref,
        lib="Device:C",
        value=val,
        fp=fp,
        pos=pos,
        conns={"1": a, "2": b},
        dnp=dnp,
        fields={"Spec": spec, "Note": note},
    )


def TP(ref, net, pos, fp=TP_PAD):
    return dict(
        ref=ref, lib="Connector:TestPoint", value=net, fp=fp, pos=pos, conns={"1": net}, fields={}
    )


def FLAG(net, pos):
    return dict(
        ref=f"#FLG_{net}",
        lib="power:PWR_FLAG",
        value="PWR_FLAG",
        fp="",
        pos=pos,
        conns={"1": net},
        power=True,
        fields={},
    )


# --------------------------------------------------------------------------
# Sheets. Positions are in mm on an A3 sheet (420 x 297), grid 2.54.
# --------------------------------------------------------------------------
SHEETS = []


def sheet(name, filename, title, notes, parts):
    SHEETS.append(dict(name=name, file=filename, title=title, notes=notes, parts=parts))


sheet(
    "HOST_INTERFACE",
    "host_interface.kicad_sch",
    "Host interface + control expander",
    [
        "HOST INTERFACE — provisional (connector family NOT frozen; see hardware/pcb/ICD_PERCEPTION_HEAD.md)",
        "J1 = single locking host cable. JST GH 1.25 mm 14-pin chosen for Rev A bench: locking, 1 A/contact, cheap prebuilt cables.",
        "J1 pin order is a BENCH/MECH decision: re-order freely until the host-carrier connector is frozen.",
        "Host-side mapping (Rev A bench harness, UNO Q, NOT final pins):",
        "  I2C_A = UNO Q Qwiic I2C4 PD12/PD13 (2.2k pull-ups on UNO Q)   I2C_B = JDIGITAL D21/D20 I2C2 PB10/PB11 (TBD_HOST_I2C_B)",
        "  TOF_INT_N, TOF_SYNC, IMU_INT1 -> TBD_INT on JDIGITAL 3.3 V pins   ILLUM_SET -> TBD_PWM or A0/A1 DAC (STM32 DAC1)",
        "  +5V_HOST -> JANALOG 5V (BENCH_VERIFY: present when UNO Q is powered from DC_IN?)   +3V3_HOST -> Qwiic/JANALOG 3V3",
        "J2 Qwiic = bench/daisy-chain path for bus A only. Never connect J1 and J2 to two different hosts (3V3 rails would tie).",
        "R1/R2 bus-A pull-ups DNP_OPTION: host already has 2.2k. R3/R4 bus-B pull-ups DNP_OPTION: Pololu shifter has 10k to VIN.",
        "U6 TCA9534 @0x20 (A2..A0 = GND). NO internal pull-ups (TCA9534 != TCA9554): external resistors define safe power-up states.",
        "FIRMWARE RULE: write Output reg (0x01) = 0x00 BEFORE setting Config reg (0x03); POR default output reg is 0xFF.",
        "FIRMWARE RULE: keep P1 (TOF_LPN) as INPUT while +3V3_TOF is off (avoid back-feeding the Pololu shifter).",
    ],
    [
        dict(
            ref="J1",
            lib="Connector_Generic_MountingPin:Conn_01x14_MountingPin",
            value="HOST_14P_TBD",
            fp="Connector_JST:JST_GH_SM14B-GHS-TB_1x14-1MP_P1.25mm_Horizontal",
            pos=(60, 90),
            conns={
                "1": "+5V_HOST",
                "2": "+5V_HOST",
                "3": "GND",
                "4": "GND",
                "5": "+3V3_HOST",
                "6": "GND",
                "7": "I2C_A_SCL",
                "8": "I2C_A_SDA",
                "9": "I2C_B_SCL",
                "10": "I2C_B_SDA",
                "11": "TOF_INT_N",
                "12": "TOF_SYNC",
                "13": "IMU_INT1",
                "14": "ILLUM_SET",
                "MP": "GND",
            },
            fields={
                "Manufacturer": "JST",
                "MPN": "SM14B-GHS-TB(LF)(SN)",
                "Status": "PROVISIONAL",
                "Note": "Connector family/pin order not frozen; mates GHR-14V-S",
            },
        ),
        dict(
            ref="J2",
            lib="Connector_Generic_MountingPin:Conn_01x04_MountingPin",
            value="QWIIC_BUS_A",
            fp="Connector_JST:JST_SH_SM04B-SRSS-TB_1x04-1MP_P1.00mm_Horizontal",
            pos=(60, 160),
            conns={"1": "GND", "2": "+3V3_HOST", "3": "I2C_A_SDA", "4": "I2C_A_SCL", "MP": "GND"},
            fields={
                "Manufacturer": "JST",
                "MPN": "SM04B-SRSS-TB(LF)(SN)",
                "Status": "SELECTED",
                "Note": "Qwiic pinout 1 GND 2 3V3 3 SDA 4 SCL (matches UNO Q QWIIC1)",
            },
        ),
        dict(
            ref="D1",
            lib="Device:D_Zener",
            value="SMF5V0A",
            fp="Diode_SMD:D_SMF",
            pos=(120, 60),
            conns={"1": "+5V_HOST", "2": "GND"},
            fields={
                "Manufacturer": "Vishay/Littelfuse",
                "MPN": "SMF5V0A",
                "Status": "CANDIDATE",
                "Note": "Unidirectional 5 V TVS on host 5 V input (Zener symbol used for K/A polarity)",
            },
        ),
        R(
            "R1",
            "4.7k",
            "I2C_A_SCL",
            "+3V3_LOGIC",
            (120, 100),
            dnp=True,
            note="DNP_OPTION host has 2.2k",
        ),
        R(
            "R2",
            "4.7k",
            "I2C_A_SDA",
            "+3V3_LOGIC",
            (135, 100),
            dnp=True,
            note="DNP_OPTION host has 2.2k",
        ),
        R(
            "R3",
            "4.7k",
            "I2C_B_SCL",
            "+3V3_TOF",
            (150, 100),
            dnp=True,
            note="DNP_OPTION Pololu shifter has 10k",
        ),
        R(
            "R4",
            "4.7k",
            "I2C_B_SDA",
            "+3V3_TOF",
            (165, 100),
            dnp=True,
            note="DNP_OPTION Pololu shifter has 10k",
        ),
        dict(
            ref="U6",
            lib="Interface_Expansion:TCA9534",
            value="TCA9534PWR",
            fp="Package_SO:TSSOP-16_4.4x5mm_P0.65mm",
            pos=(250, 110),
            conns={
                "14": "I2C_A_SCL",
                "15": "I2C_A_SDA",
                "13": "EXP_INT_N",
                "1": "GND",
                "2": "GND",
                "3": "GND",
                "16": "+3V3_LOGIC",
                "8": "GND",
                "4": "TOF_PWR_EN",
                "5": "TOF_LPN",
                "6": "TOF_I2C_RST_OPT",
                "7": "ILLUM_EN_REQ",
                "9": "EEPROM_WP",
                "10": "ILLUM_FAULT_N",
                "11": "IMU_INT2",
                "12": "STATUS_LED",
            },
            fields={
                "Manufacturer": "Texas Instruments",
                "MPN": "TCA9534PWR",
                "Status": "SELECTED",
                "Note": "0x20. P0 TOF_PWR_EN out, P1 TOF_LPN (input unless addr change), P2 spare/TOF I2C reset opt, P3 ILLUM_EN_REQ out (via R27), P4 EEPROM_WP out, P5 ILLUM_FAULT_N in, P6 IMU_INT2 in, P7 STATUS_LED out",
            },
        ),
        C("C1", "100n", "+3V3_LOGIC", "GND", (220, 160)),
        R("R26", "10k", "EXP_INT_N", "+3V3_LOGIC", (300, 160), note="TCA9534 INT is open-drain"),
    ],
)

sheet(
    "POWER",
    "power.kicad_sch",
    "Power entry, measurement shunts, switchable ToF rail",
    [
        "POWER — rails come from the host; the head makes only the switchable ToF rail.",
        "+5V_HOST -(SH1 0.05R)-> +5V_HEAD : feeds ToF LDO and illumination switch. Head 5 V current = V(SH1)/0.05.",
        "+3V3_HOST -(SH3 1R)-> +3V3_LOGIC : IMU, EEPROM, expander, op-amp. Logic current = V(SH3)/1 (expect low mA).",
        "U1 TLV75801P -> TOF_LDO_OUT -(SH2 0.1R)-> +3V3_TOF : Pololu VIN. Off by default (R7 pulls EN low).",
        "Vout = 0.55 V x (1 + R5/R6) = 0.55 x (1 + 52.3k/10k) = 3.43 V nominal. BENCH_VERIFY.",
        "Why 3.43 V and not 3.3 V: Pololu says AVDD sags below 3.3 V when VIN < ~3.4 V; VL53L8CX AVDD min is 3.13 V.",
        "  Host I/O on bus B then idles at ~3.3-3.43 V: inside STM32 VDD+0.3 V and 0.7*VDD limits. BENCH_VERIFY AVDD under ranging.",
        "Why a switchable rail: VL53L8CX reset = remove all supplies for 10 ms (ST UM3109 sec 4.2). TLV758P has active discharge.",
        "R8 DNP_OPTION 'TOF_FORCE_ON': populate to power ToF without firmware (bench only).",
        "PWR_FLAGs mark rails driven through shunts/connectors so ERC can check power-pin drive.",
    ],
    [
        R(
            "SH1",
            "0.05",
            "+5V_HOST",
            "+5V_HEAD",
            (50, 60),
            fp=R1206,
            spec="1% 0.5W current shunt",
            note="Head 5 V current measurement; Kelvin TPs TP1/TP2",
        ),
        C("C2", "10u", "+5V_HEAD", "GND", (85, 60), fp=C0805, spec="X5R 10 V"),
        C("C3", "100n", "+5V_HEAD", "GND", (100, 60)),
        R(
            "SH3",
            "1",
            "+3V3_HOST",
            "+3V3_LOGIC",
            (50, 120),
            fp=R0603,
            spec="1% 0.1W current shunt",
            note="Logic 3V3 current measurement; Kelvin TPs TP4/TP5",
        ),
        C("C4", "4.7u", "+3V3_LOGIC", "GND", (85, 120), fp=C0805, spec="X5R 10 V"),
        C("C5", "100n", "+3V3_LOGIC", "GND", (100, 120)),
        dict(
            ref="U1",
            lib="Regulator_Linear:TLV75801PDBV",
            value="TLV75801PDBVR",
            fp="Package_TO_SOT_SMD:SOT-23-5",
            pos=(190, 90),
            conns={
                "1": "+5V_HEAD",
                "3": "TOF_PWR_EN",
                "2": "GND",
                "5": "TOF_LDO_OUT",
                "4": "TOF_LDO_FB",
            },
            fields={
                "Manufacturer": "Texas Instruments",
                "MPN": "TLV75801PDBVR",
                "Status": "SELECTED",
                "Note": "500 mA adj LDO, 1% Vref, active discharge, ISC 350 mA; Pololu peak ~150 mA",
            },
        ),
        C("C6", "1u", "+5V_HEAD", "GND", (160, 130)),
        R("R5", "52.3k", "TOF_LDO_OUT", "TOF_LDO_FB", (230, 130), note="Vout set (top)"),
        R("R6", "10k", "TOF_LDO_FB", "GND", (245, 130), note="Vout set (bottom)"),
        C(
            "C7",
            "2.2u",
            "TOF_LDO_OUT",
            "GND",
            (260, 130),
            fp=C0805,
            spec="X5R 10 V (>=1 uF effective)",
        ),
        R("R7", "100k", "TOF_PWR_EN", "GND", (175, 130), note="ToF rail OFF at power-up"),
        R(
            "R8",
            "10k",
            "TOF_PWR_EN",
            "+3V3_LOGIC",
            (175, 160),
            dnp=True,
            note="DNP_OPTION TOF_FORCE_ON (bench)",
        ),
        R(
            "SH2",
            "0.1",
            "TOF_LDO_OUT",
            "+3V3_TOF",
            (300, 90),
            fp=R0805,
            spec="1% 0.25W current shunt",
            note="ToF current measurement; Kelvin TPs TP6/TP7",
        ),
        FLAG("+5V_HOST", (50, 200)),
        FLAG("+3V3_HOST", (70, 200)),
        FLAG("GND", (90, 200)),
        FLAG("+5V_HEAD", (110, 200)),
        FLAG("+3V3_LOGIC", (130, 200)),
        FLAG("+3V3_TOF", (150, 200)),
    ],
)

sheet(
    "TOF",
    "tof.kicad_sch",
    "VL53L8CX ToF — Rev A hosts the Pololu #3419 carrier",
    [
        "TOF — DECISION B: host the proven Pololu #3419 carrier, not the bare VL53L8CX (rationale: hardware/pcb/RESEARCH_LEDGER.md).",
        "Carrier regulates AVDD 3.3 V + CORE/IOVDD 1.8 V and level-shifts all I/O to VIN (NXS0108). VIN = +3V3_TOF (3.43 V).",
        "Dedicated bus I2C_B: Pololu warns its shifter is load-sensitive (keep wires < 8 cm, few devices) and the",
        "  ToF rail is power-cycled for reset — isolating it keeps IMU/EEPROM on bus A alive during ToF recovery.",
        "Address 0x29 (7-bit; 0x52 8-bit). Firmware (~84 KB) is uploaded by the ULD at every power-up.",
        "SPI/I2C pin strapped LOW by R9 (I2C mode). R10 DNP_OPTION lets expander P2 toggle it (I2C-interface reset only).",
        "R28 100k keeps expander P2 defined while R10 is not fitted (if R10 is fitted, firmware must drive P2 low).",
        "MISO and CS unused in I2C mode (carrier pulls CS high). SPI is a Rev B option, not wired.",
        "Bus B crosses the host cable: C18/C19 DNP_OPTION (22 pF) per Pololu note on shifter oscillation; scope it (BENCH_VERIFY).",
        "TOF_SYNC: host-driven ranging trigger for camera/ToF timing experiments. Keep LOW/Hi-Z while ToF rail is off.",
        "AVDD and CORE/IOVDD carrier outputs go only to test points (BENCH_VERIFY AVDD >= 3.13 V during 8x8 ranging).",
        "MECHANICAL: carrier is soldered on headers + 2x M2 to the head PCB; its pose is part of the camera<->ToF extrinsic.",
        "BENCH_VERIFY: 1x4 header pin order (GND, SPI/I2C, LP, SYNC) by continuity before layout freeze.",
    ],
    [
        dict(
            ref="M1",
            lib="PlatypusOne:Pololu_3419_VL53L8CX_Carrier",
            value="Pololu #3419",
            fp="PlatypusOne:Pololu_3419_VL53L8CX_Carrier_THT",
            pos=(150, 110),
            conns={
                "3": "+3V3_TOF",
                "4": "GND",
                "10": "GND",
                "5": "I2C_B_SDA",
                "6": "I2C_B_SCL",
                "7": NC,
                "8": NC,
                "1": "TOF_CORE_1V8",
                "2": "TOF_AVDD",
                "9": "TOF_INT_N",
                "11": "TOF_SPI_I2C_N",
                "12": "TOF_LPN",
                "13": "TOF_SYNC",
            },
            fields={
                "Manufacturer": "Pololu",
                "MPN": "3419",
                "Status": "SELECTED (Rev A)",
                "Note": "Contains VL53L8CX; 100 mA typ / 150 mA peak per Pololu",
            },
        ),
        C(
            "C8",
            "4.7u",
            "+3V3_TOF",
            "GND",
            (90, 160),
            fp=C0805,
            spec="X5R 10 V",
            note="Local bulk at carrier VIN",
        ),
        C(
            "C18",
            "22p",
            "I2C_B_SDA",
            "GND",
            (60, 200),
            dnp=True,
            spec="C0G 50 V",
            note="DNP_OPTION: Pololu suggests tens of pF if its shifter oscillates on long leads",
        ),
        C(
            "C19",
            "22p",
            "I2C_B_SCL",
            "GND",
            (75, 200),
            dnp=True,
            spec="C0G 50 V",
            note="DNP_OPTION: as C18; BENCH_VERIFY with the real host cable length",
        ),
        R("R9", "0", "TOF_SPI_I2C_N", "GND", (220, 160), note="I2C mode strap (populated)"),
        R(
            "R28",
            "100k",
            "TOF_I2C_RST_OPT",
            "GND",
            (260, 160),
            note="Defines expander P2 when R10 is DNP",
        ),
        R(
            "R10",
            "0",
            "TOF_SPI_I2C_N",
            "TOF_I2C_RST_OPT",
            (240, 160),
            dnp=True,
            note="DNP_OPTION: expander controls SPI/I2C pin (remove R9 first)",
        ),
    ],
)

sheet(
    "IMU",
    "imu.kicad_sch",
    "BMI270 IMU",
    [
        "IMU — Bosch BMI270 on bus A (I2C), VDD = VDDIO = +3V3_LOGIC (both inside 1.71/1.2-3.6 V).",
        "CSB tied to VDDIO = I2C mode (Bosch DS sec 6.3). SDO -> GND = 0x68 (R11). R12 DNP_OPTION -> 0x69.",
        "ASDx/ASCx/OCSB/OSDO not connected (DNC), as in Bosch I2C connection diagram 7.2.3 and allowed by DS Table 22.",
        "INT1 -> host (time-critical, direct). INT2 -> expander P6 (non-critical status).",
        "R30/R29 100k pull-downs define INT1/INT2 before firmware enables the outputs (BMI270 INT outputs start disabled);",
        "  firmware must configure INT1/INT2 push-pull, active-high.",
        "100 nF on VDD and on VDDIO per Bosch connection diagram 7.2.3; place within 1-2 mm of pins.",
        "PLACEMENT (layout): rigid area near camera/ToF datum, away from illumination BJTs/LDO heat (>=10 mm),",
        "  away from mounting screws/board flex zones; no vias in pad, no silkscreen under part; land per Bosch sec 8.3.",
        "Firmware: 8 kB config upload after every POR (Bosch DS sec 4.4); allow >=2 ms supply-to-interface.",
    ],
    [
        dict(
            ref="U2",
            lib="PlatypusOne:BMI270",
            value="BMI270",
            fp="PlatypusOne:Bosch_LGA-14_3x2.5mm_P0.5mm_BoschLand",
            pos=(150, 110),
            conns={
                "1": "IMU_SDO",
                "14": "I2C_A_SDA",
                "13": "I2C_A_SCL",
                "12": "+3V3_LOGIC",
                "4": "IMU_INT1",
                "9": "IMU_INT2",
                "5": "+3V3_LOGIC",
                "6": "GND",
                "8": "+3V3_LOGIC",
                "7": "GND",
                "2": NC,
                "3": NC,
                "10": NC,
                "11": NC,
            },
            fields={
                "Manufacturer": "Bosch Sensortec",
                "MPN": "BMI270",
                "Status": "SELECTED",
                "Note": "Active part (BNO055 is NRND). I2C 0x68.",
            },
        ),
        C("C9", "100n", "+3V3_LOGIC", "GND", (90, 70), note="VDD decoupling"),
        C("C10", "100n", "+3V3_LOGIC", "GND", (105, 70), note="VDDIO decoupling"),
        R("R11", "0", "IMU_SDO", "GND", (220, 160), note="I2C address 0x68 (populated)"),
        R("R29", "100k", "IMU_INT2", "GND", (260, 160), note="Defined level until INT2 enabled"),
        R("R30", "100k", "IMU_INT1", "GND", (275, 160), note="Defined level until INT1 enabled"),
        R(
            "R12",
            "0",
            "IMU_SDO",
            "+3V3_LOGIC",
            (240, 160),
            dnp=True,
            note="DNP_OPTION address 0x69",
        ),
    ],
)

sheet(
    "ILLUMINATION",
    "illumination.kicad_sch",
    "Controlled white illumination — 2-channel linear constant-current sink",
    [
        "ILLUMINATION — LED current NOT frozen. Topology frozen-candidate: switched 5 V + linear constant-current sinks.",
        "Why linear CC (not resistor, not buck): brightness independent of 5 V droop and LED Vf; no switching noise next to",
        "  camera/ToF; analog set-point means NO PWM banding on the IMX219 rolling shutter (PWM-dim only as last resort).",
        "I_LED(per ch) = V(ILLUM_VSET) / 1 ohm. ILLUM_VSET = ILLUM_SET x 10k/(120k+10k): 3.3 V -> 254 mA full scale.",
        "ILLUM_SET source: STM32 DAC (A0/A1) preferred, or TBD_PWM >= 20 kHz filtered by R17/C13 (tau ~2 ms).",
        "Off paths (defence in depth): ILLUM_SET low/floating -> R17 pulls VSET to 0; ILLUM_EN low -> U3 off (R13);",
        "  JP1 closed -> ILLUM_EN hard-grounded (illumination kill); R27 1k keeps the expander from fighting JP1.",
        "FIRMWARE RULE: set ILLUM_SET = 0 before toggling ILLUM_EN, then ramp (op-amps saturate while U3 is off).",
        "U3 TPS2553 limit: R15 = 40.2k -> IOS 592/647/709 mA min/nom/max (SLVS841F eq.1) > 2 x 254 mA. Protects host 5 V.",
        "Q1/Q2 dissipation at 254 mA: (5.0 - Vf ~3.0 - 0.25) x 0.25 = ~0.45 W each continuous -> SOT-223 + copper; BENCH thermal.",
        "LEDs live on small off-board light boards (J3/J4) so position/angle/diffuser can iterate without respinning the head.",
        "  LED part NOT selected (high-CRI mid-power candidates in BOM). Keep LEDs outside camera FOV cone; symmetric about axis.",
        "C15/C16 DNP_OPTION loop compensation if the op-amp/BJT loop rings (BENCH_VERIFY with scope on SENSE nodes).",
    ],
    [
        dict(
            ref="U3",
            lib="PlatypusOne:TPS2553DBV",
            value="TPS2553DBVR",
            fp="Package_TO_SOT_SMD:SOT-23-6",
            pos=(80, 70),
            conns={
                "1": "+5V_HEAD",
                "3": "ILLUM_EN",
                "2": "GND",
                "6": "+5V_ILLUM",
                "4": "ILLUM_FAULT_N",
                "5": "ILIM_SET",
            },
            fields={
                "Manufacturer": "Texas Instruments",
                "MPN": "TPS2553DBVR",
                "Status": "SELECTED",
                "Note": "Current-limited 5 V switch for LED rail; FAULT -> expander P5",
            },
        ),
        C("C11", "1u", "+5V_HEAD", "GND", (40, 110)),
        C("C12", "10u", "+5V_ILLUM", "GND", (130, 110), fp=C0805, spec="X5R 10 V"),
        R("R13", "100k", "ILLUM_EN", "GND", (60, 110), note="Illumination OFF at power-up"),
        R(
            "R27",
            "1k",
            "ILLUM_EN_REQ",
            "ILLUM_EN",
            (30, 150),
            note="Series so JP1 can override the expander without a short",
        ),
        R("R14", "10k", "ILLUM_FAULT_N", "+3V3_LOGIC", (100, 140), note="FAULT open-drain pull-up"),
        R(
            "R15",
            "40.2k",
            "ILIM_SET",
            "GND",
            (115, 140),
            note="Current limit ~650 mA nom (BENCH_VERIFY vs host 5 V)",
        ),
        dict(
            ref="JP1",
            lib="Jumper:SolderJumper_2_Open",
            value="ILLUM_KILL",
            fp="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm",
            pos=(60, 150),
            conns={"1": "ILLUM_EN", "2": "GND"},
            fields={
                "Status": "OPEN by default",
                "Note": "Close to permanently disable illumination",
            },
        ),
        R("R16", "120k", "ILLUM_SET", "ILLUM_VSET", (190, 60), note="Set divider top"),
        R(
            "R17",
            "10k",
            "ILLUM_VSET",
            "GND",
            (205, 60),
            note="Set divider bottom; defines OFF when host floats",
        ),
        C("C13", "220n", "ILLUM_VSET", "GND", (220, 60), note="PWM filter"),
        dict(
            ref="U4",
            unit=1,
            lib="Amplifier_Operational:TLV9062xD",
            value="TLV9062IDR",
            fp="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
            pos=(200, 120),
            conns={"3": "ILLUM_VSET", "2": "ILED1_FB", "1": "Q1_DRV"},
            fields={
                "Manufacturer": "Texas Instruments",
                "MPN": "TLV9062IDR",
                "Status": "SELECTED",
                "Note": "RRIO dual op-amp, 1.8-5.5 V; supplied from +3V3_LOGIC",
            },
        ),
        dict(
            ref="U4",
            unit=2,
            lib="Amplifier_Operational:TLV9062xD",
            value="TLV9062IDR",
            fp="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
            pos=(200, 190),
            conns={"5": "ILLUM_VSET", "6": "ILED2_FB", "7": "Q2_DRV"},
            fields={},
        ),
        dict(
            ref="U4",
            unit=3,
            lib="Amplifier_Operational:TLV9062xD",
            value="TLV9062IDR",
            fp="Package_SO:SOIC-8_3.9x4.9mm_P1.27mm",
            pos=(160, 250),
            conns={"8": "+3V3_LOGIC", "4": "GND"},
            fields={},
        ),
        C("C14", "100n", "+3V3_LOGIC", "GND", (180, 250)),
        R("R18", "470", "Q1_DRV", "Q1_B", (250, 110), note="Base drive"),
        R("R19", "470", "Q2_DRV", "Q2_B", (250, 180), note="Base drive"),
        R("R20", "1k", "ILED1_SENSE", "ILED1_FB", (230, 140), note="Feedback"),
        R("R21", "1k", "ILED2_SENSE", "ILED2_FB", (230, 210), note="Feedback"),
        C("C15", "1n", "Q1_DRV", "ILED1_FB", (270, 140), dnp=True, note="DNP_OPTION compensation"),
        C("C16", "1n", "Q2_DRV", "ILED2_FB", (270, 210), dnp=True, note="DNP_OPTION compensation"),
        dict(
            ref="Q1",
            lib="Transistor_BJT:BCP56",
            value="BCP56-16",
            fp="Package_TO_SOT_SMD:SOT-223-3_TabPin2",
            pos=(300, 110),
            conns={"1": "Q1_B", "2": "LED1_K", "4": "LED1_K", "3": "ILED1_SENSE"},
            fields={
                "Manufacturer": "Nexperia",
                "MPN": "BCP56-16,115",
                "Status": "CANDIDATE",
                "Note": "Linear pass element; BJT chosen for linear-mode SOA over small trench MOSFETs",
            },
        ),
        dict(
            ref="Q2",
            lib="Transistor_BJT:BCP56",
            value="BCP56-16",
            fp="Package_TO_SOT_SMD:SOT-223-3_TabPin2",
            pos=(300, 180),
            conns={"1": "Q2_B", "2": "LED2_K", "4": "LED2_K", "3": "ILED2_SENSE"},
            fields={
                "Manufacturer": "Nexperia",
                "MPN": "BCP56-16,115",
                "Status": "CANDIDATE",
                "Note": "",
            },
        ),
        R(
            "R22",
            "1",
            "ILED1_SENSE",
            "GND",
            (320, 140),
            fp=R1206,
            spec="1% 0.25W+ sense",
            note="I_LED1 = V(TP27) / 1 ohm",
        ),
        R(
            "R23",
            "1",
            "ILED2_SENSE",
            "GND",
            (320, 210),
            fp=R1206,
            spec="1% 0.25W+ sense",
            note="I_LED2 = V(TP28) / 1 ohm",
        ),
        dict(
            ref="J3",
            lib="Connector_Generic_MountingPin:Conn_01x02_MountingPin",
            value="LIGHT_BOARD_1",
            fp="Connector_JST:JST_GH_SM02B-GHS-TB_1x02-1MP_P1.25mm_Horizontal",
            pos=(370, 90),
            conns={"1": "+5V_ILLUM", "2": "LED1_K", "MP": "GND"},
            fields={
                "Manufacturer": "JST",
                "MPN": "SM02B-GHS-TB(LF)(SN)",
                "Status": "PROVISIONAL",
                "Note": "Pin1 LED anode (+5V_ILLUM), pin2 LED cathode (sink)",
            },
        ),
        dict(
            ref="J4",
            lib="Connector_Generic_MountingPin:Conn_01x02_MountingPin",
            value="LIGHT_BOARD_2",
            fp="Connector_JST:JST_GH_SM02B-GHS-TB_1x02-1MP_P1.25mm_Horizontal",
            pos=(370, 160),
            conns={"1": "+5V_ILLUM", "2": "LED2_K", "MP": "GND"},
            fields={
                "Manufacturer": "JST",
                "MPN": "SM02B-GHS-TB(LF)(SN)",
                "Status": "PROVISIONAL",
                "Note": "",
            },
        ),
    ],
)

sheet(
    "ID_CALIBRATION",
    "id_calibration.kicad_sch",
    "Board identity + calibration storage",
    [
        "ID / CALIBRATION — smallest robust option: one 32-Kbit EEPROM with a factory 128-bit unique serial number.",
        "AT24CS32 on bus A: array at 0x50, read-only serial number in the separate 0x58 'security' address space",
        "  (exact read sequence: verify against Microchip AT24CS32 datasheet during firmware bring-up).",
        "WP pulled HIGH by R24 = write-protected by default. Host enables writes via expander P4 (EEPROM_WP low).",
        "Contents: compact versioned record (schema id, board rev, assembly variant, camera id, ToF carrier id,",
        "  extrinsic camera<->ToF, intrinsics reference/hash, calibration rev/date, test state) + CRC.",
        "Full calibration files stay on the host, keyed by the serial number; EEPROM holds identity + compact pose.",
    ],
    [
        dict(
            ref="U5",
            lib="Memory_EEPROM:AT24CS32-STUM",
            value="AT24CS32-STUM-T",
            fp="Package_TO_SOT_SMD:SOT-23-5",
            pos=(150, 100),
            conns={
                "4": "+3V3_LOGIC",
                "2": "GND",
                "3": "I2C_A_SDA",
                "1": "I2C_A_SCL",
                "5": "EEPROM_WP",
            },
            fields={
                "Manufacturer": "Microchip",
                "MPN": "AT24CS32-STUM-T",
                "Status": "SELECTED",
                "Note": "32 Kbit + 128-bit unique serial; SOT-23-5 has no address pins (fixed 0x50)",
            },
        ),
        C("C17", "100n", "+3V3_LOGIC", "GND", (110, 100)),
        R("R24", "10k", "EEPROM_WP", "+3V3_LOGIC", (200, 140), note="Write-protect by default"),
    ],
)

TPS = [
    ("TP1", "+5V_HOST", TP_PAD),
    ("TP2", "+5V_HEAD", TP_PAD),
    ("TP3", "+5V_ILLUM", TP_PAD),
    ("TP4", "+3V3_HOST", TP_PAD),
    ("TP5", "+3V3_LOGIC", TP_PAD),
    ("TP6", "TOF_LDO_OUT", TP_PAD),
    ("TP7", "+3V3_TOF", TP_PAD),
    ("TP8", "TOF_AVDD", TP_PAD),
    ("TP9", "TOF_CORE_1V8", TP_PAD),
    ("TP10", "GND", TP_LOOP),
    ("TP11", "GND", TP_LOOP),
    ("TP12", "GND", TP_LOOP),
    ("TP13", "I2C_A_SCL", TP_PAD),
    ("TP14", "I2C_A_SDA", TP_PAD),
    ("TP15", "I2C_B_SCL", TP_PAD),
    ("TP16", "I2C_B_SDA", TP_PAD),
    ("TP17", "TOF_INT_N", TP_PAD),
    ("TP18", "TOF_SYNC", TP_PAD),
    ("TP19", "TOF_LPN", TP_PAD),
    ("TP20", "TOF_PWR_EN", TP_PAD),
    ("TP21", "IMU_INT1", TP_PAD),
    ("TP22", "IMU_INT2", TP_PAD),
    ("TP23", "ILLUM_SET", TP_PAD),
    ("TP24", "ILLUM_VSET", TP_PAD),
    ("TP25", "ILLUM_EN", TP_PAD),
    ("TP26", "ILLUM_FAULT_N", TP_PAD),
    ("TP27", "ILED1_SENSE", TP_PAD),
    ("TP28", "ILED2_SENSE", TP_PAD),
    ("TP29", "EXP_INT_N", TP_PAD),
    ("TP30", "TOF_I2C_RST_OPT", TP_PAD),
    ("TP31", "EEPROM_WP", TP_PAD),
]
debug_parts = []
for i, (ref, net, fp) in enumerate(TPS):
    col, row = divmod(i, 11)
    debug_parts.append(TP(ref, net, (50 + col * 70, 50 + row * 17), fp))
debug_parts += [
    dict(
        ref="D2",
        lib="Device:LED",
        value="GREEN",
        fp="LED_SMD:LED_0603_1608Metric",
        pos=(270, 60),
        conns={"2": "STATUS_LED", "1": "STATUS_LED_K"},
        fields={"Status": "GENERIC", "Note": "Firmware status LED via expander P7 (active high)"},
    ),
    R("R25", "1k", "STATUS_LED_K", "GND", (300, 60), note="~1 mA status LED"),
    dict(
        ref="H1",
        lib="Mechanical:MountingHole",
        value="DATUM_PRIMARY",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(260, 120),
        conns={},
        fields={"Note": "Primary locating hole (pin/shoulder screw). Position from Fusion."},
    ),
    dict(
        ref="H2",
        lib="Mechanical:MountingHole",
        value="DATUM_SLOT",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(290, 120),
        conns={},
        fields={"Note": "Becomes a slot at layout (anti-rotation). Position from Fusion."},
    ),
    dict(
        ref="H3",
        lib="Mechanical:MountingHole",
        value="CLAMP",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(320, 120),
        conns={},
        fields={"Note": "Clamp-only screw; does not locate."},
    ),
    dict(
        ref="H4",
        lib="Mechanical:MountingHole",
        value="CAM_MNT",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(260, 160),
        conns={},
        fields={
            "Note": "Camera module standoff. PROVISIONAL Pi-camera 21 x 12.5 mm pattern; MEASURE selected Arducam board (T5)."
        },
    ),
    dict(
        ref="H5",
        lib="Mechanical:MountingHole",
        value="CAM_MNT",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(290, 160),
        conns={},
        fields={"Note": "Camera module standoff (PROVISIONAL)"},
    ),
    dict(
        ref="H6",
        lib="Mechanical:MountingHole",
        value="CAM_MNT",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(320, 160),
        conns={},
        fields={"Note": "Camera module standoff (PROVISIONAL)"},
    ),
    dict(
        ref="H7",
        lib="Mechanical:MountingHole",
        value="CAM_MNT",
        fp="MountingHole:MountingHole_2.2mm_M2",
        pos=(350, 160),
        conns={},
        fields={"Note": "Camera module standoff (PROVISIONAL)"},
    ),
    dict(
        ref="FID1",
        lib="Mechanical:Fiducial",
        value="Fiducial",
        fp="Fiducial:Fiducial_1mm_Mask2mm",
        pos=(260, 200),
        conns={},
        fields={},
    ),
    dict(
        ref="FID2",
        lib="Mechanical:Fiducial",
        value="Fiducial",
        fp="Fiducial:Fiducial_1mm_Mask2mm",
        pos=(290, 200),
        conns={},
        fields={},
    ),
]
sheet(
    "DEBUG_TEST",
    "debug_test.kicad_sch",
    "Test points, status LED, mechanical datums",
    [
        "DEBUG / TEST — every rail, every bus and every control/interrupt line has a test point.",
        "Shunt Kelvin pairs: SH1 = TP1/TP2, SH3 = TP4/TP5, SH2 = TP6/TP7; LED current = TP27/TP28 to GND (1 ohm).",
        "GND loops TP10-TP12 spread across the board for scope ground clips.",
        "Layout rules (later): TPs on one side, labelled, >= 2.54 mm apart, reachable with the head assembled to its bracket.",
        "MECHANICAL: H1/H2/H3 implement a 3-2-1 style locate-then-clamp scheme to the sensor datum bracket;",
        "  H4-H7 hold the camera module so the head PCB is the rigid camera<->ToF datum. ALL positions come from Fusion.",
        "Board marking (layout): 'PLATYPUS ONE PERCEPTION HEAD REV A', date code, serial label area, pin-1 marks on J1-J4.",
    ],
    debug_parts,
)


# --------------------------------------------------------------------------
# Emission
# --------------------------------------------------------------------------
_LIB_CACHE = {}


def load_lib(nick):
    if nick not in _LIB_CACHE:
        path = LIBS.get(nick, f"{STOCK}/{nick}.kicad_sym")
        root = parse(Path(path).read_text())[0]
        _LIB_CACHE[nick] = {s[1]: s for s in find(root, "symbol")}
    return _LIB_CACHE[nick]


def resolve_symbol(lib_id):
    """Return the symbol flattened (extends resolved) and renamed for lib_symbols."""
    nick, name = lib_id.split(":")
    syms = load_lib(nick)
    sym = syms[name]
    ext = first(sym, "extends")
    if ext:
        base = syms[ext[1]]
        node = parse(dumps(base))[0]
        own = {p[1]: p for p in find(sym, "property")}
        node = [x for x in node if not (isinstance(x, list) and x[0] == "property" and x[1] in own)]
        insert_at = next(i for i, x in enumerate(node) if isinstance(x, list) and x[0] == "symbol")
        for p in reversed(list(own.values())):
            node.insert(insert_at, p)
        text = dumps(node).replace(f'(symbol "{ext[1]}_', f'(symbol "{name}_')
        node = parse(text)[0]
        node[1] = name
        sym = node
    out = parse(dumps(sym))[0]
    out[1] = lib_id
    return out


def symbol_pins(sym, unit):
    pins = {}
    for sub in find(sym, "symbol"):
        tail = sub[1].rsplit("_", 2)
        u, style = int(tail[1]), int(tail[2])
        if style not in (0, 1) or u not in (0, unit):
            continue
        for p in find(sub, "pin"):
            at = first(p, "at")
            num = first(p, "number")[1]
            pins[num] = (
                float(at[1]),
                float(at[2]),
                int(float(at[3])) if len(at) > 3 else 0,
                str(p[1]),
            )
    return pins


def symbol_units(sym):
    units = set()
    for sub in find(sym, "symbol"):
        u = int(sub[1].rsplit("_", 2)[1])
        if u:
            units.add(u)
    return units or {1}


def S(x):
    return Sym(f"{round(x, 4):g}")


def at(x, y, a=0):
    return [Sym("at"), S(x), S(y), Sym(str(a))]


def effects(size=1.27, justify=None, hide=False):
    e = [Sym("effects"), [Sym("font"), [Sym("size"), S(size), S(size)]]]
    if justify:
        e.append([Sym("justify")] + [Sym(j) for j in justify.split()])
    if hide:
        e.append([Sym("hide"), Sym("yes")])
    return e


def snap(v):
    return round(v / 1.27) * 1.27


class SheetWriter:
    def __init__(self, path_uuid, file_uuid, sheet_key):
        self.items = []
        self.lib_ids = []
        self.path = path_uuid
        self.file_uuid = file_uuid
        self.key = sheet_key
        self.n = 0

    def nid(self, *p):
        self.n += 1
        return uid(self.key, self.n, *p)

    def wire(self, x1, y1, x2, y2):
        self.items.append(
            [
                Sym("wire"),
                [Sym("pts"), [Sym("xy"), S(x1), S(y1)], [Sym("xy"), S(x2), S(y2)]],
                [Sym("stroke"), [Sym("width"), Sym("0")], [Sym("type"), Sym("default")]],
                [Sym("uuid"), self.nid("w")],
            ]
        )

    def label(self, net, x, y, ang):
        just = {0: "left bottom", 90: "left bottom", 180: "right bottom", 270: "right bottom"}[ang]
        if net in GLOBAL:
            gj = {0: "left", 90: "left", 180: "right", 270: "right"}[ang]
            self.items.append(
                [
                    Sym("global_label"),
                    net,
                    [Sym("shape"), Sym("bidirectional")],
                    at(x, y, ang),
                    [Sym("fields_autoplaced"), Sym("yes")],
                    effects(justify=gj),
                    [Sym("uuid"), self.nid("g", net)],
                    [
                        Sym("property"),
                        "Intersheetrefs",
                        "${INTERSHEET_REFS}",
                        at(x, y, 0),
                        effects(justify="left", hide=True),
                    ],
                ]
            )
        else:
            self.items.append(
                [
                    Sym("label"),
                    net,
                    at(x, y, ang),
                    [Sym("fields_autoplaced"), Sym("yes")],
                    effects(justify=just),
                    [Sym("uuid"), self.nid("l", net)],
                ]
            )

    def no_connect(self, x, y):
        self.items.append(
            [Sym("no_connect"), [Sym("at"), S(x), S(y)], [Sym("uuid"), self.nid("nc")]]
        )

    def text(self, s, x, y, size=1.27):
        self.items.append(
            [
                Sym("text"),
                s,
                [Sym("exclude_from_sim"), Sym("no")],
                at(x, y, 0),
                effects(size=size, justify="left top"),
                [Sym("uuid"), self.nid("t")],
            ]
        )

    def place(
        self, lib_id, ref, value, x, y, unit=1, fp="", fields=None, dnp=False, ang=0, power=False
    ):
        if lib_id not in self.lib_ids:
            self.lib_ids.append(lib_id)
        sym = resolve_symbol(lib_id)
        node = [
            Sym("symbol"),
            [Sym("lib_id"), lib_id],
            at(x, y, ang),
            [Sym("unit"), Sym(str(unit))],
            [Sym("exclude_from_sim"), Sym("no")],
            [Sym("in_bom"), Sym("no" if power else "yes")],
            [Sym("on_board"), Sym("no" if power else "yes")],
            [Sym("dnp"), Sym("yes" if dnp else "no")],
            [Sym("fields_autoplaced"), Sym("yes")],
            [Sym("uuid"), uid(self.key, "sym", ref, unit)],
        ]
        libprops = {p[1]: p for p in find(sym, "property")}

        def field_at(name, dflt_dy):
            lp = libprops.get(name)
            if lp is None:
                return at(x + 3.81, y + dflt_dy), "left"
            la = first(lp, "at")
            ang = int(float(la[3])) if len(la) > 3 else 0
            j = first(first(lp, "effects") or [], "justify")
            just = " ".join(str(t) for t in j[1:]) if j else None
            return at(x + float(la[1]), y - float(la[2]), ang), just

        rat, rj = field_at("Reference", -1.27)
        vat, vj = field_at("Value", 1.27)
        node.append([Sym("property"), "Reference", ref, rat, effects(justify=rj, hide=power)])
        node.append([Sym("property"), "Value", value, vat, effects(justify=vj)])
        node.append([Sym("property"), "Footprint", fp, at(x, y), effects(hide=True)])
        ds = next((p[2] for p in find(sym, "property") if p[1] == "Datasheet"), "~")
        node.append([Sym("property"), "Datasheet", ds, at(x, y), effects(hide=True)])
        node.append([Sym("property"), "Description", "", at(x, y), effects(hide=True)])
        for k, v in (fields or {}).items():
            if v:
                node.append([Sym("property"), k, v, at(x, y), effects(hide=True)])
        pins = symbol_pins(sym, unit)
        for num in pins:
            node.append([Sym("pin"), num, [Sym("uuid"), uid(self.key, "pin", ref, unit, num)]])
        node.append(
            [
                Sym("instances"),
                [
                    Sym("project"),
                    PROJECT,
                    [
                        Sym("path"),
                        self.path,
                        [Sym("reference"), ref],
                        [Sym("unit"), Sym(str(unit))],
                    ],
                ],
            ]
        )
        self.items.append(node)
        return pins

    def connect_part(self, part):
        x, y = part["pos"]
        x, y = snap(x), snap(y)
        power = part.get("power", False)
        pins = self.place(
            part["lib"],
            part["ref"],
            part["value"],
            x,
            y,
            unit=part.get("unit", 1),
            fp=part.get("fp", ""),
            fields=part.get("fields"),
            dnp=part.get("dnp", False),
            power=power,
        )
        for num, net in part["conns"].items():
            px, py, pang, _ = pins[num]
            sx, sy = x + px, y - py  # symbol y-up -> schematic y-down
            if power:
                if net == "GND":
                    self.place(
                        "power:GND", f"#PWR_{self.key}_{part['ref']}", "GND", sx, sy, power=True
                    )
                else:
                    self.label(net, sx, sy, 90)
                continue
            if net is NC:
                self.no_connect(sx, sy)
                continue
            # outward direction from body
            dx, dy, lang = {0: (-1, 0, 180), 180: (1, 0, 0), 90: (0, 1, 270), 270: (0, -1, 90)}[
                pang
            ]
            ex, ey = sx + dx * 2.54, sy + dy * 2.54
            self.wire(sx, sy, ex, ey)
            if net == "GND":
                self.place(
                    "power:GND",
                    f"#PWR_{self.key}_{part['ref']}_{part.get('unit', 1)}_{num}",
                    "GND",
                    ex,
                    ey,
                    fp="",
                    power=True,
                )
            else:
                self.label(net, ex, ey, lang)
        # pins not mentioned in conns: leave unconnected so ERC flags them
        missing = set(pins) - set(part["conns"])
        if (
            missing
            and not power
            and part["lib"] not in ("Mechanical:MountingHole", "Mechanical:Fiducial")
        ):
            raise SystemExit(f"{part['ref']}: pins without a decision: {sorted(missing)}")

    def render(self, title, rev_note, paper="A3", page="1"):
        lib_symbols = [Sym("lib_symbols")] + [resolve_symbol(lib_id) for lib_id in self.lib_ids]
        doc = [
            Sym("kicad_sch"),
            [Sym("version"), Sym("20250114")],
            [Sym("generator"), "eeschema"],
            [Sym("generator_version"), "9.0"],
            [Sym("uuid"), self.file_uuid],
            [Sym("paper"), paper],
            [
                Sym("title_block"),
                [Sym("title"), title],
                [Sym("date"), "2026-10-05"],
                [Sym("rev"), "A0-prelim"],
                [Sym("company"), "Platypus One"],
                [Sym("comment"), Sym("1"), rev_note],
                [
                    Sym("comment"),
                    Sym("2"),
                    "PRE-LAYOUT: not for fabrication. Provisional nets/values are labelled.",
                ],
            ],
            lib_symbols,
        ] + self.items
        return doc


def write_sheet(sh, idx):
    sheet_uuid = uid("sheet", sh["name"])
    w = SheetWriter(f"/{ROOT_UUID}/{sheet_uuid}", uid("file", sh["name"]), sh["name"])
    y = 12
    w.text(sh["title"].upper(), 10, 8, size=2.5)
    for line in sh["notes"]:
        w.text(line, 10, y + 2)
        y += 3.2
    off = max(0, y - 30)
    for part in sh["parts"]:
        p = dict(part)
        p["pos"] = (p["pos"][0] + 10, p["pos"][1] + off)
        w.connect_part(p)
    doc = w.render(f"Perception Head Rev A — {sh['name']}", sh["title"])
    doc.append([Sym("embedded_fonts"), Sym("no")])
    with open(os.path.join(PROJ_DIR, sh["file"]), "w") as f:
        f.write(dumps(doc) + "\n")
    return sheet_uuid


def write_root(sheet_uuids):
    w = SheetWriter(f"/{ROOT_UUID}", ROOT_UUID, "ROOT")
    notes = [
        "PLATYPUS ONE — PERCEPTION HEAD PCB, REV A (PRE-LAYOUT SCHEMATIC)",
        "Status/gates: hardware/pcb/README.md   ICD: hardware/pcb/ICD_PERCEPTION_HEAD.md   Power: hardware/pcb/POWER_TREE_REV_A.md",
        "Purpose: rigid sensing head = camera module datum + VL53L8CX ToF (Pololu #3419) + BMI270 IMU + controlled illumination",
        "  + identity/calibration EEPROM + test access, on ONE low-speed cable to the UNO Q host. No MIPI on this board.",
        "The IMX219 camera stays a replaceable module on the Media Carrier CSI path; this board only holds it mechanically.",
        "Failure isolation: ToF, IMU and illumination are all optional to the RGB CSI path; the head draws only host 5 V/3V3.",
        "Label convention: TBD_* = host pin not frozen; DNP_OPTION = footprint fitted but unpopulated; BENCH_VERIFY = needs measurement.",
        "Do NOT route, generate Gerbers or order until the gates in hardware/pcb/README.md are released.",
    ]
    y = 15
    for i, line in enumerate(notes):
        w.text(line, 15, y, size=2.0 if i == 0 else 1.5)
        y += 5 if i == 0 else 3.5
    items = []
    for i, (sh, su) in enumerate(zip(SHEETS, sheet_uuids, strict=True)):
        col, row = i % 4, i // 4
        x, yy = 20 + col * 95, 70 + row * 45
        items.append(
            [
                Sym("sheet"),
                [Sym("at"), S(x), S(yy)],
                [Sym("size"), S(80), S(30)],
                [Sym("exclude_from_sim"), Sym("no")],
                [Sym("in_bom"), Sym("yes")],
                [Sym("on_board"), Sym("yes")],
                [Sym("dnp"), Sym("no")],
                [Sym("fields_autoplaced"), Sym("yes")],
                [Sym("stroke"), [Sym("width"), Sym("0.1524")], [Sym("type"), Sym("solid")]],
                [Sym("fill"), [Sym("color"), Sym("0"), Sym("0"), Sym("0"), Sym("0.0000")]],
                [Sym("uuid"), su],
                [
                    Sym("property"),
                    "Sheetname",
                    sh["name"],
                    at(x, yy - 0.7),
                    effects(justify="left bottom"),
                ],
                [
                    Sym("property"),
                    "Sheetfile",
                    sh["file"],
                    at(x, yy + 30.6),
                    effects(justify="left top"),
                ],
                [
                    Sym("instances"),
                    [
                        Sym("project"),
                        PROJECT,
                        [Sym("path"), f"/{ROOT_UUID}", [Sym("page"), str(i + 2)]],
                    ],
                ],
            ]
        )
        w.text(sh["title"], x + 2, yy + 4)
    doc = w.render("Platypus One — Perception Head Rev A", "Root: sheet index and design intent")
    doc += items
    doc.append([Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]])
    doc.append([Sym("embedded_fonts"), Sym("no")])
    with open(os.path.join(PROJ_DIR, f"{PROJECT}.kicad_sch"), "w") as f:
        f.write(dumps(doc) + "\n")


def main():
    uuids = [write_sheet(sh, i) for i, sh in enumerate(SHEETS)]
    write_root(uuids)
    print("wrote", len(SHEETS) + 1, "schematic files to", os.path.relpath(PROJ_DIR))


if __name__ == "__main__":
    main()

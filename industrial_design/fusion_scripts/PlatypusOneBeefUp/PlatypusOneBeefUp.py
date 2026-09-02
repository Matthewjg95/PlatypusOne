"""Platypus One "beef up" script for Autodesk Fusion 360.

Run this with the PlatypusOne enclosure design OPEN and ACTIVE
(Utilities tab -> ADD-INS -> Scripts and Add-Ins -> green "+" -> pick this
folder -> Run).

What it does, in order:
  1. Renames the existing enclosure bodies (ENCLOSURE_TOP / ENCLOSURE_BOTTOM,
     or ENCLOSURE_SHELL when there is only one body).
  2. GRIP          - chunky raked pistol grip under the rear of the body,
                     with a USB-C slot cut into its base.
  3. SENSOR_HEAD   - forward head block with camera opening, ToF opening and
                     an LED illumination window, plus internal CAMERA_ENVELOPE,
                     TOF_ENVELOPE, LED_ZONE and a generic SENSOR_CARRIER.
  4. UNO_Q, DISPLAY_ENVELOPE, BATTERY - internal packaging envelopes using the
                     real board outline / BOM envelope numbers.
  5. CONTROLS      - ROTARY_ENCODER at the thumb, TRIGGER at the index finger,
                     USB_C_PORT in the grip base.

Everything is created inside its own named component so the browser tree reads
like the packaging plan. The script NEVER cuts or modifies your existing
enclosure bodies (only renames them); all cuts happen on bodies it creates
itself.

Re-running is safe: components created by a previous run are deleted first,
so you can edit CONFIG below and run again until the layout looks right.

All CONFIG dimensions are in millimetres, angles in degrees. (The Fusion API
works in cm internally; conversion is handled for you.)
"""

import math
import traceback

import adsk.core
import adsk.fusion

# ---------------------------------------------------------------------------
# CONFIG - all mm / degrees. Edit and re-run.
# ---------------------------------------------------------------------------
CONFIG = {
    # Frame. The script auto-detects axes from the enclosure bounding box:
    #   forward = longest axis, up = shortest axis. If the grip/head come out
    # mirrored, flip these and re-run.
    "FLIP_FORWARD": False,
    "FLIP_UP": False,
    "FLIP_SIDE": False,

    # Assumed enclosure wall for internal packaging placement (placeholder).
    "WALL": 2.5,

    # Grip (raked pistol grip under the rear half).
    "GRIP_DEPTH": 45.0,            # front-to-back of the grip section
    "GRIP_WIDTH": 34.0,            # side-to-side of the grip section
    "GRIP_LENGTH": 115.0,          # along the grip axis
    "GRIP_RAKE_DEG": 15.0,         # lean angle (top tips forward)
    "GRIP_EMBED": 4.0,             # how far the grip sinks into the body
    "GRIP_SETBACK_FROM_REAR": 55.0,  # grip centreline, measured from rear face

    # Trigger (index finger) and rotary encoder (thumb).
    "TRIGGER_DIAMETER": 14.0,
    "TRIGGER_WIDTH": 22.0,
    "TRIGGER_DROP": 32.0,          # below the enclosure underside
    "ENCODER_BODY": 24.0,          # EC11-style 24 x 24 body (BOM)
    "ENCODER_BODY_H": 12.0,
    "ENCODER_SHAFT_D": 7.0,
    "ENCODER_SHAFT_H": 14.0,
    "ENCODER_KNOB_D": 18.0,
    "ENCODER_KNOB_H": 12.0,
    "ENCODER_FROM_REAR": 22.0,     # encoder centre, from the rear face
    "ENCODER_SIDE_OFFSET": -24.0,  # + = one side, - = the other (thumb side)

    # USB-C at the grip base.
    "USBC_SLOT_W": 9.2,
    "USBC_SLOT_H": 3.4,
    "USBC_SLOT_DEPTH": 14.0,
    "USBC_PORT_W": 8.94,           # USB-C receptacle envelope
    "USBC_PORT_H": 3.26,
    "USBC_PORT_DEPTH": 7.35,

    # Sensor head on the front face.
    "HEAD_FORWARD": 25.0,          # how far the head sticks out past the front
    "HEAD_EMBED": 3.0,             # overlap back into the enclosure
    "HEAD_WIDTH": 72.0,            # capped to enclosure width - 10
    "CAMERA_OPENING_D": 14.0,
    "CAMERA_OPENING_DEPTH": 13.0,
    "CAMERA_ENVELOPE": (25.0, 25.0, 10.0),   # 13 MP autofocus module (BOM)
    "TOF_OPENING": 8.0,                       # square opening
    "TOF_OPENING_DEPTH": 6.0,
    "TOF_ENVELOPE": (6.0, 6.0, 3.0),          # VL53L8CX (BOM)
    "LED_WINDOW_D": 11.0,
    "LED_WINDOW_DEPTH": 2.0,
    "CARRIER_ENVELOPE": (55.0, 18.0, 5.0),    # generic sensor/carrier PCB zone

    # Internal packaging envelopes.
    "UNO_Q_OUTLINE": (68.58, 53.34),  # official board outline - do NOT change
    "UNO_Q_CLEARANCE_H": 16.0,        # 3D clearance TBD from vendor STEP
    "UNO_Q_REAR_GAP": 8.0,            # connector clearance behind the board
    "DISPLAY_ENVELOPE": (121.11, 77.93, 13.0),  # Waveshare 5in HDMI LCD (H)
    "DISPLAY_TOP_GAP": 2.0,           # display face below the outer top face
    "BATTERY_ENVELOPE": (100.0, 60.0, 8.0),     # flat Li-ion (BOM)

    # Fallback outer envelope if no bodies exist in the design (mm).
    "FALLBACK_ENVELOPE": (165.1, 96.52, 27.94),
}

MM = 0.1  # mm -> internal cm

# Components this script owns; deleted and rebuilt on every run.
OWNED_COMPONENTS = (
    "GRIP", "SENSOR_HEAD", "UNO_Q", "DISPLAY_ENVELOPE", "BATTERY", "CONTROLS",
)


# ---------------------------------------------------------------------------
# Small geometry helpers around the Fusion API (which works in cm).
# ---------------------------------------------------------------------------
class Frame:
    """Canonical device frame: F = forward, S = side, U = up, origin at the
    centre of the enclosure bounding box. Coordinates in mm."""

    def __init__(self, center, v_forward, v_side, v_up):
        self.center = center      # Point3D (cm)
        self.vF = v_forward       # unit Vector3D
        self.vS = v_side
        self.vU = v_up

    def point(self, f, s, u):
        """(f, s, u) in mm -> Point3D in model space."""
        p = self.center.copy()
        for vec, dist in ((self.vF, f), (self.vS, s), (self.vU, u)):
            step = vec.copy()
            step.scaleBy(dist * MM)
            p.translateBy(step)
        return p

    def tilted(self, rake_deg):
        """A copy of this frame with F/U rotated about S by rake_deg."""
        m = adsk.core.Matrix3D.create()
        m.setToRotation(math.radians(rake_deg), self.vS,
                        adsk.core.Point3D.create(0, 0, 0))
        vf = self.vF.copy()
        vu = self.vU.copy()
        vf.transformBy(m)
        vu.transformBy(m)
        return Frame(self.center, vf, self.vS.copy(), vu)


def make_box(tmp, center_pt, v_len, v_wid, length, width, height):
    """Axis lengths in mm; returns a temporary BRepBody."""
    obb = adsk.core.OrientedBoundingBox3D.create(
        center_pt, v_len.copy(), v_wid.copy(),
        length * MM, width * MM, height * MM)
    return tmp.createBox(obb)


def make_cylinder(tmp, pt_a, pt_b, diameter):
    r = diameter * MM / 2.0
    return tmp.createCylinderOrCone(pt_a, r, pt_b, r)


def offset_point(pt, vec, dist_mm):
    p = pt.copy()
    step = vec.copy()
    step.scaleBy(dist_mm * MM)
    p.translateBy(step)
    return p


def union(tmp, target, tool):
    tmp.booleanOperation(target, tool,
                         adsk.fusion.BooleanTypes.UnionBooleanType)
    return target


def cut(tmp, target, tool):
    tmp.booleanOperation(target, tool,
                         adsk.fusion.BooleanTypes.DifferenceBooleanType)
    return target


def new_component(root, name):
    occ = root.occurrences.addNewComponent(adsk.core.Matrix3D.create())
    occ.component.name = name
    return occ.component


def add_bodies(design, comp, named_bodies):
    """Add [(name, tempBody), ...] to a component, parametric-safe."""
    start = comp.bRepBodies.count
    if design.designType == adsk.fusion.DesignTypes.ParametricDesignType:
        base = comp.features.baseFeatures.add()
        base.startEdit()
        for _, body in named_bodies:
            comp.bRepBodies.add(body, base)
        base.finishEdit()
    else:
        for _, body in named_bodies:
            comp.bRepBodies.add(body)
    for i, (name, _) in enumerate(named_bodies):
        comp.bRepBodies.item(start + i).name = name


# ---------------------------------------------------------------------------
# Steps
# ---------------------------------------------------------------------------
def cleanup_previous_run(root):
    removed = []
    for occ in list(root.occurrences):
        if occ.component.name in OWNED_COMPONENTS:
            removed.append(occ.component.name)
            occ.deleteMe()
    return removed


def measure_envelope(root, cfg):
    """Bounding box of the bodies living directly in the root component."""
    boxes = [b.boundingBox for b in root.bRepBodies]
    if not boxes:
        half = [d * MM / 2.0 for d in cfg["FALLBACK_ENVELOPE"]]
        return (adsk.core.Point3D.create(-half[0], -half[1], -half[2]),
                adsk.core.Point3D.create(half[0], half[1], half[2]), False)
    lo = [min(b.minPoint.asArray()[i] for b in boxes) for i in range(3)]
    hi = [max(b.maxPoint.asArray()[i] for b in boxes) for i in range(3)]
    return (adsk.core.Point3D.create(*lo), adsk.core.Point3D.create(*hi), True)


def build_frame(lo, hi, cfg):
    extents = [hi.asArray()[i] - lo.asArray()[i] for i in range(3)]
    i_fwd = extents.index(max(extents))
    i_up = extents.index(min(extents))
    i_side = ({0, 1, 2} - {i_fwd, i_up}).pop()

    def axis(i, flip):
        v = [0.0, 0.0, 0.0]
        v[i] = -1.0 if flip else 1.0
        return adsk.core.Vector3D.create(*v)

    center = adsk.core.Point3D.create(
        (lo.asArray()[0] + hi.asArray()[0]) / 2.0,
        (lo.asArray()[1] + hi.asArray()[1]) / 2.0,
        (lo.asArray()[2] + hi.asArray()[2]) / 2.0)
    frame = Frame(center,
                  axis(i_fwd, cfg["FLIP_FORWARD"]),
                  axis(i_side, cfg["FLIP_SIDE"]),
                  axis(i_up, cfg["FLIP_UP"]))
    dims_mm = (extents[i_fwd] / MM, extents[i_side] / MM, extents[i_up] / MM)
    axis_names = "XYZ"
    mapping = ("forward={}{} side={}{} up={}{}".format(
        "-" if cfg["FLIP_FORWARD"] else "+", axis_names[i_fwd],
        "-" if cfg["FLIP_SIDE"] else "+", axis_names[i_side],
        "-" if cfg["FLIP_UP"] else "+", axis_names[i_up]))
    return frame, dims_mm, mapping


def rename_enclosure_bodies(root, frame):
    bodies = list(root.bRepBodies)
    if not bodies:
        return "no existing bodies found (fallback envelope used)"
    if len(bodies) == 1:
        bodies[0].name = "ENCLOSURE_SHELL"
        return ("1 body -> ENCLOSURE_SHELL (split it into top/bottom later "
                "for ENCLOSURE_TOP / ENCLOSURE_BOTTOM)")

    def height_of(body):
        bb = body.boundingBox
        c = adsk.core.Vector3D.create(
            (bb.minPoint.x + bb.maxPoint.x) / 2.0 - frame.center.x,
            (bb.minPoint.y + bb.maxPoint.y) / 2.0 - frame.center.y,
            (bb.minPoint.z + bb.maxPoint.z) / 2.0 - frame.center.z)
        return c.dotProduct(frame.vU)

    ordered = sorted(bodies, key=height_of)
    ordered[0].name = "ENCLOSURE_BOTTOM"
    ordered[-1].name = "ENCLOSURE_TOP"
    for i, body in enumerate(ordered[1:-1]):
        body.name = "ENCLOSURE_MID_{}".format(i + 1)
    return "{} bodies -> ENCLOSURE_BOTTOM / ENCLOSURE_TOP".format(len(bodies))


def build_grip(design, root, tmp, frame, dims, cfg):
    length, _, height = dims
    grip = frame.tilted(cfg["GRIP_RAKE_DEG"])

    f_center = -length / 2.0 + cfg["GRIP_SETBACK_FROM_REAR"]
    top_center = frame.point(f_center, 0.0, -height / 2.0 + cfg["GRIP_EMBED"])

    body_center = offset_point(
        top_center, grip.vU, -(cfg["GRIP_LENGTH"] / 2.0 - cfg["GRIP_EMBED"]))
    grip_body = make_box(tmp, body_center, grip.vF, grip.vS,
                         cfg["GRIP_DEPTH"], cfg["GRIP_WIDTH"],
                         cfg["GRIP_LENGTH"])

    # USB-C slot cut into the grip base.
    bottom_center = offset_point(
        top_center, grip.vU, -(cfg["GRIP_LENGTH"] - cfg["GRIP_EMBED"]))
    slot_center = offset_point(bottom_center, grip.vU, 2.0)
    slot = make_box(tmp, slot_center, grip.vS, grip.vF,
                    cfg["USBC_SLOT_W"], cfg["USBC_SLOT_H"],
                    cfg["USBC_SLOT_DEPTH"])
    cut(tmp, grip_body, slot)

    comp = new_component(root, "GRIP")
    add_bodies(design, comp, [("GRIP_BODY", grip_body)])
    return grip, top_center, bottom_center


def build_controls(design, root, tmp, frame, grip, grip_top, grip_bottom,
                   dims, cfg):
    length, _, height = dims
    comp = new_component(root, "CONTROLS")

    # Trigger: cylinder across the front face of the grip, index height.
    trig_center = offset_point(
        offset_point(grip_top, grip.vU, -cfg["TRIGGER_DROP"]),
        grip.vF, cfg["GRIP_DEPTH"] / 2.0 + 6.0)
    trig_a = offset_point(trig_center, grip.vS, -cfg["TRIGGER_WIDTH"] / 2.0)
    trig_b = offset_point(trig_center, grip.vS, cfg["TRIGGER_WIDTH"] / 2.0)
    trigger = make_cylinder(tmp, trig_a, trig_b, cfg["TRIGGER_DIAMETER"])

    # Rotary encoder: body flush with the top face at the thumb, shaft + knob
    # standing proud.
    enc_f = -length / 2.0 + cfg["ENCODER_FROM_REAR"]
    enc_s = cfg["ENCODER_SIDE_OFFSET"]
    top_u = height / 2.0
    enc_body_center = frame.point(enc_f, enc_s,
                                  top_u - cfg["ENCODER_BODY_H"] / 2.0)
    encoder = make_box(tmp, enc_body_center, frame.vF, frame.vS,
                       cfg["ENCODER_BODY"], cfg["ENCODER_BODY"],
                       cfg["ENCODER_BODY_H"])
    shaft_a = frame.point(enc_f, enc_s, top_u)
    shaft_b = frame.point(enc_f, enc_s, top_u + cfg["ENCODER_SHAFT_H"])
    union(tmp, encoder, make_cylinder(tmp, shaft_a, shaft_b,
                                      cfg["ENCODER_SHAFT_D"]))
    knob_b = frame.point(enc_f, enc_s,
                         top_u + cfg["ENCODER_SHAFT_H"] + cfg["ENCODER_KNOB_H"])
    union(tmp, encoder, make_cylinder(tmp, shaft_b, knob_b,
                                      cfg["ENCODER_KNOB_D"]))

    # USB-C port envelope, recessed just inside the grip-base slot.
    port_center = offset_point(grip_bottom, grip.vU,
                               cfg["USBC_PORT_DEPTH"] / 2.0 + 1.0)
    usb_port = make_box(tmp, port_center, grip.vS, grip.vF,
                        cfg["USBC_PORT_W"], cfg["USBC_PORT_H"],
                        cfg["USBC_PORT_DEPTH"])

    add_bodies(design, comp, [("TRIGGER", trigger),
                              ("ROTARY_ENCODER", encoder),
                              ("USB_C_PORT", usb_port)])


def build_sensor_head(design, root, tmp, frame, dims, cfg):
    length, width, height = dims
    head_w = min(cfg["HEAD_WIDTH"], width - 10.0)
    depth = cfg["HEAD_FORWARD"] + cfg["HEAD_EMBED"]
    f_face = length / 2.0 + cfg["HEAD_FORWARD"]   # front face of the head

    head_center = frame.point(f_face - depth / 2.0, 0.0, 0.0)
    head = make_box(tmp, head_center, frame.vF, frame.vS, depth, head_w,
                    height)

    # Camera opening (round bore in from the front face).
    cam_s, cam_u = -18.0, 1.0
    cam_out = frame.point(f_face + 1.0, cam_s, cam_u)
    cam_in = frame.point(f_face - cfg["CAMERA_OPENING_DEPTH"], cam_s, cam_u)
    cut(tmp, head, make_cylinder(tmp, cam_out, cam_in,
                                 cfg["CAMERA_OPENING_D"]))

    # ToF opening (square pocket).
    tof_s, tof_u = 8.0, 5.0
    tof_center = frame.point(f_face - cfg["TOF_OPENING_DEPTH"] / 2.0 + 0.5,
                             tof_s, tof_u)
    cut(tmp, head, make_box(tmp, tof_center, frame.vS, frame.vU,
                            cfg["TOF_OPENING"], cfg["TOF_OPENING"],
                            cfg["TOF_OPENING_DEPTH"] + 1.0))

    # LED illumination window (shallow round recess).
    led_s, led_u = 22.0, -4.0
    led_out = frame.point(f_face + 1.0, led_s, led_u)
    led_in = frame.point(f_face - cfg["LED_WINDOW_DEPTH"], led_s, led_u)
    cut(tmp, head, make_cylinder(tmp, led_out, led_in, cfg["LED_WINDOW_D"]))

    # Internal envelopes behind the openings.
    cw, ch, cd = cfg["CAMERA_ENVELOPE"]
    cam_env = make_box(
        tmp, frame.point(f_face - cfg["CAMERA_OPENING_DEPTH"] - cd / 2.0 + 1,
                         cam_s, cam_u),
        frame.vS, frame.vU, cw, ch, cd)

    tw, th, td = cfg["TOF_ENVELOPE"]
    tof_env = make_box(
        tmp, frame.point(f_face - cfg["TOF_OPENING_DEPTH"] - td / 2.0,
                         tof_s, tof_u),
        frame.vS, frame.vU, tw, th, td)

    led_zone = make_cylinder(
        tmp, frame.point(f_face - cfg["LED_WINDOW_DEPTH"], led_s, led_u),
        frame.point(f_face - cfg["LED_WINDOW_DEPTH"] - 2.0, led_s, led_u),
        cfg["LED_WINDOW_D"] + 1.0)

    kw, kh, kd = cfg["CARRIER_ENVELOPE"]
    carrier = make_box(tmp, frame.point(f_face - depth + kd / 2.0 + 1.0,
                                        0.0, -4.5),
                       frame.vS, frame.vU, kw, kh, kd)

    comp = new_component(root, "SENSOR_HEAD")
    add_bodies(design, comp, [("HEAD_BLOCK", head),
                              ("CAMERA_ENVELOPE", cam_env),
                              ("TOF_ENVELOPE", tof_env),
                              ("LED_ZONE", led_zone),
                              ("SENSOR_CARRIER", carrier)])


def build_packaging(design, root, tmp, frame, dims, cfg):
    """UNO Q, display and battery envelopes. Returns packaging warnings."""
    length, _, height = dims
    wall = cfg["WALL"]
    u_floor = -height / 2.0 + wall
    warnings = []

    def span(f_center, f_len, u_center, u_len):
        return (f_center - f_len / 2.0, f_center + f_len / 2.0,
                u_center - u_len / 2.0, u_center + u_len / 2.0)

    def overlap_mm(a, b):
        f = min(a[1], b[1]) - max(a[0], b[0])
        u = min(a[3], b[3]) - max(a[2], b[2])
        return (f, u) if (f > 0.1 and u > 0.1) else None

    # UNO Q against the rear wall, sitting on the floor.
    uno_l, uno_w = cfg["UNO_Q_OUTLINE"]
    uno_h = cfg["UNO_Q_CLEARANCE_H"]
    uno_f = -length / 2.0 + wall + cfg["UNO_Q_REAR_GAP"] + uno_l / 2.0
    uno_u = u_floor + uno_h / 2.0
    comp = new_component(root, "UNO_Q")
    add_bodies(design, comp, [(
        "UNO_Q_CLEARANCE",
        make_box(tmp, frame.point(uno_f, 0.0, uno_u), frame.vF, frame.vS,
                 uno_l, uno_w, uno_h))])

    # Display under the top face, centred.
    dsp_l, dsp_w, dsp_t = cfg["DISPLAY_ENVELOPE"]
    dsp_u = height / 2.0 - cfg["DISPLAY_TOP_GAP"] - dsp_t / 2.0
    comp = new_component(root, "DISPLAY_ENVELOPE")
    add_bodies(design, comp, [(
        "DISPLAY",
        make_box(tmp, frame.point(0.0, 0.0, dsp_u), frame.vF, frame.vS,
                 dsp_l, dsp_w, dsp_t))])

    # Battery against the front wall, on the floor.
    bat_l, bat_w, bat_t = cfg["BATTERY_ENVELOPE"]
    bat_f = length / 2.0 - wall - bat_l / 2.0
    bat_u = u_floor + bat_t / 2.0
    comp = new_component(root, "BATTERY")
    add_bodies(design, comp, [(
        "BATTERY_PACK",
        make_box(tmp, frame.point(bat_f, 0.0, bat_u), frame.vF, frame.vS,
                 bat_l, bat_w, bat_t))])

    # Honest packaging report.
    uno = span(uno_f, uno_l, uno_u, uno_h)
    dsp = span(0.0, dsp_l, dsp_u, dsp_t)
    bat = span(bat_f, bat_l, bat_u, bat_t)
    for name_a, a, name_b, b in (("UNO_Q", uno, "DISPLAY", dsp),
                                 ("UNO_Q", uno, "BATTERY", bat),
                                 ("BATTERY", bat, "DISPLAY", dsp)):
        hit = overlap_mm(a, b)
        if hit:
            warnings.append(
                "{} and {} overlap ~{:.1f} mm (fwd) x {:.1f} mm (up)".format(
                    name_a, name_b, hit[0], hit[1]))
    inner_h = height - 2.0 * wall
    stack = dsp_t + uno_h
    if stack > inner_h:
        warnings.append(
            "display ({} mm) + UNO Q clearance ({} mm) = {} mm stack vs "
            "~{:.1f} mm inner height -> body needs to grow ~{:.1f} mm "
            "thicker, or the UNO Q moves out from under the display".format(
                dsp_t, uno_h, stack, inner_h, stack - inner_h))
    return warnings


# ---------------------------------------------------------------------------
def run(context):
    ui = None
    try:
        app = adsk.core.Application.get()
        ui = app.userInterface
        design = adsk.fusion.Design.cast(app.activeProduct)
        if not design:
            ui.messageBox("Open the PlatypusOne design first, then run this "
                          "script.", "Platypus One")
            return

        root = design.rootComponent
        tmp = adsk.fusion.TemporaryBRepManager.get()
        cfg = CONFIG

        removed = cleanup_previous_run(root)
        lo, hi, measured = measure_envelope(root, cfg)
        frame, dims, mapping = build_frame(lo, hi, cfg)
        rename_note = rename_enclosure_bodies(root, frame)

        grip, grip_top, grip_bottom = build_grip(design, root, tmp, frame,
                                                 dims, cfg)
        build_controls(design, root, tmp, frame, grip, grip_top, grip_bottom,
                       dims, cfg)
        build_sensor_head(design, root, tmp, frame, dims, cfg)
        warnings = build_packaging(design, root, tmp, frame, dims, cfg)

        lines = [
            "Platypus One beef-up complete.",
            "",
            "Enclosure envelope {}: {:.1f} x {:.1f} x {:.1f} mm ({})".format(
                "measured" if measured else "FALLBACK (no bodies found)",
                dims[0], dims[1], dims[2], mapping),
            "Existing bodies: " + rename_note,
            "Created: GRIP, CONTROLS (TRIGGER / ROTARY_ENCODER / USB_C_PORT),",
            "  SENSOR_HEAD (+ camera / ToF / LED openings and envelopes),",
            "  UNO_Q, DISPLAY_ENVELOPE, BATTERY.",
        ]
        if removed:
            lines.append("Replaced previous run: " + ", ".join(removed))
        if warnings:
            lines.append("")
            lines.append("PACKAGING WARNINGS:")
            lines.extend("  - " + w for w in warnings)
        lines.append("")
        lines.append("Wrong way round? Flip FLIP_FORWARD / FLIP_UP in CONFIG "
                     "and run again - re-runs replace the script's own "
                     "components and never touch yours.")
        report = "\n".join(lines)
        app.log(report)
        ui.messageBox(report, "Platypus One beef-up")
    except Exception:  # noqa: BLE001 - surface everything to the user
        if ui:
            ui.messageBox("Script failed:\n{}".format(traceback.format_exc()),
                          "Platypus One beef-up")

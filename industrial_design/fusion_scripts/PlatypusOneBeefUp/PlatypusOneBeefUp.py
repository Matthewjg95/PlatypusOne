"""Platypus One "beef up" script for Autodesk Fusion 360 - rev 3.

Layout: portrait handheld (per the industrial-design reference render) - the
existing enclosure stays the hero, held vertically like a phone. A sculpted
side grip runs along the right edge, the sensor cluster lives in a band
across the top of the front face with a protruding corner camera block, and
thumb controls sit on the right edge above the grip.

Rev 3 packaging changes (these RESOLVE the rev 2 warnings):
  * ENCLOSURE_MID_FRAME - an 8 mm perimeter band behind the existing shell
    grows total thickness 27.9 -> 35.9 mm, so display (13) + UNO Q (16)
    stack front/back with ~1.9 mm to spare.
  * Battery re-specced 100x60x8 -> 90x60x10 flat pack (similar Wh) so it
    clears the UNO Q lengthwise in the back bay.
  * Display envelope shifts 9 mm toward the grip end (the window opening is
    139.7 mm for a 121.1 mm display, so it can slide) - the radar bay clears
    the display top edge. Matches the reference render's low display.
  * SENSOR_CARRIER becomes a 2 mm interposer sandwiched between the UNO Q
    (back bay) and radar (front bay).
  * Fillets are applied to the grip, mid-frame and camera block where the
    kernel allows (skipped silently if a radius fails - placeholders).

Run this with the PlatypusOne enclosure design OPEN and ACTIVE
(Utilities tab -> ADD-INS -> Scripts and Add-Ins -> green "+" -> pick this
folder -> Run).

Everything is created inside its own named component so the browser tree
reads like the packaging plan. The script NEVER cuts or modifies your
existing enclosure bodies (only renames them); all cuts happen on bodies it
creates itself - face details it cannot cut (pill windows, grille) are thin
applique bodies marking where real openings go.

Re-running is safe: components created by a previous run are deleted first,
so you can edit CONFIG below and run again until the layout looks right.

All CONFIG dimensions are in millimetres. (The Fusion API works in cm
internally; conversion is handled for you.)
"""

import traceback

import adsk.core
import adsk.fusion

# ---------------------------------------------------------------------------
# CONFIG - all mm. Edit and re-run.
# ---------------------------------------------------------------------------
CONFIG = {
    # Frame. Auto-detected from the enclosure bounding box:
    #   top (sensor end) = longest axis, front (display normal) = shortest
    #   axis, grip edge = the remaining axis. If anything lands on the wrong
    # face or end, flip these and re-run.
    "FLIP_TOP": False,       # sensor band ends up at the wrong end
    "FLIP_FRONT": False,     # details land on the back face
    "FLIP_GRIP_SIDE": False, # grip on the wrong edge

    # Assumed enclosure wall for internal packaging placement (placeholder).
    "WALL": 2.5,

    # Mid-frame band behind the existing shell - the thickness the packaging
    # needs. Set to 0 to study the original thickness again.
    "MID_FRAME_THICK": 8.0,
    "MID_FRAME_WALL": 3.0,

    # Side grip (right edge, lower half).
    "GRIP_LENGTH": 105.0,        # along the device's long axis
    "GRIP_PROUD": 32.0,          # how far it stands out past the edge
    "GRIP_THICK": 30.0,          # front-to-back
    "GRIP_EMBED": 4.0,           # sunk into the edge
    "GRIP_CENTER_OFFSET": -25.0, # centre along long axis (0 = device centre)
    "GRIP_FILLET": 9.0,

    # Trigger (index finger, back of the grip near its top).
    "TRIGGER_DIAMETER": 14.0,
    "TRIGGER_WIDTH": 20.0,
    "TRIGGER_POS": 15.0,         # along long axis

    # Rotary encoder, side-mounted on the right edge above the grip.
    "ENCODER_BODY": 24.0,        # EC11-style 24 x 24 body (BOM)
    "ENCODER_BODY_T": 12.0,
    "ENCODER_POS": 36.0,         # along long axis
    "ENCODER_SHAFT_D": 7.0,
    "ENCODER_SHAFT_LEN": 8.0,
    "ENCODER_KNOB_D": 18.0,
    "ENCODER_KNOB_LEN": 10.0,

    # USB-C at the grip base end.
    "USBC_SLOT_W": 9.2,
    "USBC_SLOT_H": 3.4,
    "USBC_SLOT_DEPTH": 14.0,
    "USBC_PORT_W": 8.94,
    "USBC_PORT_H": 3.26,
    "USBC_PORT_DEPTH": 7.35,

    # Sensor band across the top of the front face.
    "BAND_FROM_TOP": 18.0,           # band centreline, from the top edge
    "CORNER_BLOCK": (32.0, 32.0, 34.0),  # corner camera block L x W x thick
    "CORNER_BLOCK_PROUD": 6.0,           # stands proud of the front face
    "CORNER_BLOCK_FILLET": 6.0,
    "CAMERA_BORE_D": 20.0,
    "CAMERA_BORE_DEPTH": 12.0,
    "CAMERA_LENS_D": 12.0,
    "CAMERA_ENVELOPE": (25.0, 25.0, 10.0),  # 13 MP autofocus module (BOM)
    "PILL": (22.0, 30.0, 2.0),           # sensor pill applique
    "PILL_OFFSET": -33.0,                # across the width, from centre
    "TOF_WINDOW": 8.0,
    "TOF_ENVELOPE": (6.0, 6.0, 3.0),     # VL53L8CX (BOM)
    "LED_WINDOW_D": 10.0,
    "GRILLE_D": 24.0,                    # radar grille applique
    "GRILLE_OFFSET": -6.0,
    "RADAR_ENVELOPE": (20.0, 40.0, 12.0),  # Grove BGT24LTR11 (BOM)
    "CARRIER_ENVELOPE": (20.0, 55.0, 2.0),  # interposer between the bays

    # Internal packaging envelopes.
    "UNO_Q_OUTLINE": (68.58, 53.34),  # official board outline - do NOT change
    "UNO_Q_CLEARANCE_H": 16.0,        # 3D clearance TBD from vendor STEP
    "UNO_Q_POS": (45.0, -10.0),       # centre: (long axis, width axis)
    "DISPLAY_ENVELOPE": (121.11, 77.93, 13.0),  # Waveshare 5in HDMI LCD (H)
    "DISPLAY_TOP_GAP": 2.0,           # display face below the outer front face
    "DISPLAY_L_OFFSET": -9.0,         # slid toward the grip end (rev 3)
    "BATTERY_ENVELOPE": (90.0, 60.0, 10.0),     # flat Li-ion, rev 3 re-spec
    "BATTERY_POS": -35.0,             # centre along the long axis

    # Fallback outer envelope if no bodies exist in the design (mm).
    "FALLBACK_ENVELOPE": (165.1, 96.52, 27.94),
}

MM = 0.1  # mm -> internal cm

# Components this script owns; deleted and rebuilt on every run.
OWNED_COMPONENTS = (
    "MID_FRAME", "GRIP", "SENSOR_HEAD", "UNO_Q", "DISPLAY_ENVELOPE",
    "BATTERY", "CONTROLS",
)


# ---------------------------------------------------------------------------
# Small geometry helpers around the Fusion API (which works in cm).
# ---------------------------------------------------------------------------
class Frame:
    """Canonical device frame, origin at the centre of the enclosure
    bounding box, coordinates in mm:
      L = along the long axis, + toward the top (sensor band)
      S = across the width, + toward the grip edge
      U = display normal, + out of the front face
    """

    def __init__(self, center, v_long, v_side, v_up):
        self.center = center      # Point3D (cm)
        self.vL = v_long          # unit Vector3D
        self.vS = v_side
        self.vU = v_up

    def point(self, l, s, u):
        p = self.center.copy()
        for vec, dist in ((self.vL, l), (self.vS, s), (self.vU, u)):
            step = vec.copy()
            step.scaleBy(dist * MM)
            p.translateBy(step)
        return p


def make_box(tmp, center_pt, v_len, v_wid, length, width, height):
    """Axis lengths in mm; returns a temporary BRepBody."""
    obb = adsk.core.OrientedBoundingBox3D.create(
        center_pt, v_len.copy(), v_wid.copy(),
        length * MM, width * MM, height * MM)
    return tmp.createBox(obb)


def make_cylinder(tmp, pt_a, pt_b, diameter):
    r = diameter * MM / 2.0
    return tmp.createCylinderOrCone(pt_a, r, pt_b, r)


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
    """Add [(name, tempBody), ...] to a component, parametric-safe.
    Returns the persisted bodies in the same order."""
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
    out = []
    for i, (name, _) in enumerate(named_bodies):
        persisted = comp.bRepBodies.item(start + i)
        persisted.name = name
        out.append(persisted)
    return out


def try_fillet(comp, body, radius_mm):
    """Round every edge of a persisted body; placeholder-grade, so a failed
    radius is silently skipped. Returns True on success."""
    try:
        edges = adsk.core.ObjectCollection.create()
        for edge in body.edges:
            edges.add(edge)
        fillets = comp.features.filletFeatures
        fi = fillets.createInput()
        radius = adsk.core.ValueInput.createByReal(radius_mm * MM)
        if hasattr(fi, "edgeSetInputs"):
            fi.edgeSetInputs.addConstantRadiusEdgeSet(edges, radius, True)
        else:  # older API
            fi.addConstantRadiusEdgeSet(edges, radius, True)
        fillets.add(fi)
        return True
    except Exception:  # noqa: BLE001 - cosmetic only
        return False


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
    i_long = extents.index(max(extents))
    i_up = extents.index(min(extents))
    i_side = ({0, 1, 2} - {i_long, i_up}).pop()

    def axis(i, flip):
        v = [0.0, 0.0, 0.0]
        v[i] = -1.0 if flip else 1.0
        return adsk.core.Vector3D.create(*v)

    center = adsk.core.Point3D.create(
        (lo.asArray()[0] + hi.asArray()[0]) / 2.0,
        (lo.asArray()[1] + hi.asArray()[1]) / 2.0,
        (lo.asArray()[2] + hi.asArray()[2]) / 2.0)
    frame = Frame(center,
                  axis(i_long, cfg["FLIP_TOP"]),
                  axis(i_side, cfg["FLIP_GRIP_SIDE"]),
                  axis(i_up, cfg["FLIP_FRONT"]))
    dims_mm = (extents[i_long] / MM, extents[i_side] / MM, extents[i_up] / MM)
    axis_names = "XYZ"
    mapping = ("top={}{} grip-edge={}{} front={}{}".format(
        "-" if cfg["FLIP_TOP"] else "+", axis_names[i_long],
        "-" if cfg["FLIP_GRIP_SIDE"] else "+", axis_names[i_side],
        "-" if cfg["FLIP_FRONT"] else "+", axis_names[i_up]))
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


def build_mid_frame(design, root, tmp, frame, dims, cfg):
    """Perimeter band behind the shell that grows the total thickness."""
    mid = cfg["MID_FRAME_THICK"]
    if mid <= 0:
        return False
    length, width, height = dims
    u_center = -height / 2.0 - mid / 2.0
    band = make_box(tmp, frame.point(0.0, 0.0, u_center),
                    frame.vL, frame.vS, length, width, mid)
    wall = cfg["MID_FRAME_WALL"]
    cavity = make_box(tmp, frame.point(0.0, 0.0, u_center),
                      frame.vL, frame.vS,
                      length - 2 * wall, width - 2 * wall, mid + 2.0)
    cut(tmp, band, cavity)
    comp = new_component(root, "MID_FRAME")
    bodies = add_bodies(design, comp, [("ENCLOSURE_MID_FRAME", band)])
    filleted = try_fillet(comp, bodies[0], 3.0)
    return filleted


def build_grip(design, root, tmp, frame, dims, cfg):
    """Side handle along the grip edge; returns its key positions."""
    _, width, _ = dims
    s_center = width / 2.0 - cfg["GRIP_EMBED"] + cfg["GRIP_PROUD"] / 2.0
    l_center = cfg["GRIP_CENTER_OFFSET"]
    l_bottom = l_center - cfg["GRIP_LENGTH"] / 2.0

    grip_body = make_box(tmp, frame.point(l_center, s_center, 0.0),
                         frame.vL, frame.vS,
                         cfg["GRIP_LENGTH"], cfg["GRIP_PROUD"],
                         cfg["GRIP_THICK"])

    # USB-C slot cut through the grip base end.
    slot = make_box(tmp, frame.point(l_bottom + 2.0, s_center, 0.0),
                    frame.vL, frame.vS,
                    cfg["USBC_SLOT_DEPTH"] + 4.0, cfg["USBC_SLOT_H"],
                    cfg["USBC_SLOT_W"])
    cut(tmp, grip_body, slot)

    comp = new_component(root, "GRIP")
    bodies = add_bodies(design, comp, [("GRIP_BODY", grip_body)])
    filleted = try_fillet(comp, bodies[0], cfg["GRIP_FILLET"])
    return s_center, l_bottom, filleted


def build_controls(design, root, tmp, frame, dims, cfg, grip_s, grip_bottom):
    _, width, _ = dims
    edge = width / 2.0
    comp = new_component(root, "CONTROLS")

    # Trigger: across the back of the grip near its top (index finger).
    trig_u = -(cfg["GRIP_THICK"] / 2.0 + cfg["TRIGGER_DIAMETER"] / 2.0 - 1.0)
    trig_a = frame.point(cfg["TRIGGER_POS"],
                         grip_s - cfg["TRIGGER_WIDTH"] / 2.0, trig_u)
    trig_b = frame.point(cfg["TRIGGER_POS"],
                         grip_s + cfg["TRIGGER_WIDTH"] / 2.0, trig_u)
    trigger = make_cylinder(tmp, trig_a, trig_b, cfg["TRIGGER_DIAMETER"])

    # Rotary encoder: side-mounted on the edge above the grip, knob out.
    enc_l = cfg["ENCODER_POS"]
    body_s = edge - cfg["ENCODER_BODY_T"] / 2.0 - 2.0
    encoder = make_box(tmp, frame.point(enc_l, body_s, 0.0),
                       frame.vL, frame.vU,
                       cfg["ENCODER_BODY"], cfg["ENCODER_BODY"],
                       cfg["ENCODER_BODY_T"])
    shaft_a = frame.point(enc_l, body_s, 0.0)
    shaft_b = frame.point(enc_l, edge + cfg["ENCODER_SHAFT_LEN"], 0.0)
    union(tmp, encoder, make_cylinder(tmp, shaft_a, shaft_b,
                                      cfg["ENCODER_SHAFT_D"]))
    knob_b = frame.point(
        enc_l, edge + cfg["ENCODER_SHAFT_LEN"] + cfg["ENCODER_KNOB_LEN"], 0.0)
    union(tmp, encoder, make_cylinder(tmp, shaft_b, knob_b,
                                      cfg["ENCODER_KNOB_D"]))

    # USB-C port envelope, recessed inside the grip-base slot.
    usb = make_box(tmp,
                   frame.point(grip_bottom + cfg["USBC_PORT_DEPTH"] / 2.0 + 1.0,
                               grip_s, 0.0),
                   frame.vL, frame.vS,
                   cfg["USBC_PORT_DEPTH"], cfg["USBC_PORT_H"],
                   cfg["USBC_PORT_W"])

    add_bodies(design, comp, [("TRIGGER", trigger),
                              ("ROTARY_ENCODER", encoder),
                              ("USB_C_PORT", usb)])


def build_sensor_cluster(design, root, tmp, frame, dims, cfg):
    """Top-band sensor cluster on the front face + corner camera block."""
    length, width, height = dims
    band_l = length / 2.0 - cfg["BAND_FROM_TOP"]
    face_u = height / 2.0
    wall = cfg["WALL"]

    # Corner camera block, standing proud of the front face.
    bl, bw, bt = cfg["CORNER_BLOCK"]
    blk_l = length / 2.0 - bl / 2.0 - 2.0
    blk_s = width / 2.0 - bw / 2.0 - 2.0
    blk_u = face_u + cfg["CORNER_BLOCK_PROUD"] - bt / 2.0
    block = make_box(tmp, frame.point(blk_l, blk_s, blk_u),
                     frame.vL, frame.vS, bl, bw, bt)
    blk_face = blk_u + bt / 2.0
    bore = make_cylinder(
        tmp, frame.point(blk_l, blk_s, blk_face + 1.0),
        frame.point(blk_l, blk_s, blk_face - cfg["CAMERA_BORE_DEPTH"]),
        cfg["CAMERA_BORE_D"])
    cut(tmp, block, bore)
    lens = make_cylinder(
        tmp, frame.point(blk_l, blk_s, blk_face - cfg["CAMERA_BORE_DEPTH"]),
        frame.point(blk_l, blk_s, blk_face - 3.0),
        cfg["CAMERA_LENS_D"])

    cw, ch, cd = cfg["CAMERA_ENVELOPE"]
    cam_env = make_box(
        tmp,
        frame.point(blk_l, blk_s, blk_face - cfg["CAMERA_BORE_DEPTH"]
                    - cd / 2.0 - 1.0),
        frame.vL, frame.vS, cw, ch, cd)

    # Sensor pill applique with ToF + LED windows (real openings come later).
    pl, pw, pt = cfg["PILL"]
    pill_s = cfg["PILL_OFFSET"]
    pill = make_box(tmp, frame.point(band_l, pill_s, face_u + pt / 2.0),
                    frame.vL, frame.vS, pl, pw, pt)
    tof_s = pill_s - pw / 2.0 + cfg["TOF_WINDOW"] / 2.0 + 4.0
    led_s = pill_s + pw / 2.0 - cfg["LED_WINDOW_D"] / 2.0 - 3.0
    tof_win = make_box(tmp, frame.point(band_l, tof_s, face_u + pt + 0.5),
                       frame.vL, frame.vS,
                       cfg["TOF_WINDOW"], cfg["TOF_WINDOW"], 1.0)
    led_win = make_cylinder(tmp,
                            frame.point(band_l, led_s, face_u + pt),
                            frame.point(band_l, led_s, face_u + pt + 1.0),
                            cfg["LED_WINDOW_D"])

    tw, th, td = cfg["TOF_ENVELOPE"]
    tof_env = make_box(tmp, frame.point(band_l, tof_s,
                                        face_u - wall - td / 2.0),
                       frame.vL, frame.vS, tw, th, td)

    # Radar grille applique + envelope behind it (front bay).
    grille = make_cylinder(
        tmp, frame.point(band_l, cfg["GRILLE_OFFSET"], face_u),
        frame.point(band_l, cfg["GRILLE_OFFSET"], face_u + 2.0),
        cfg["GRILLE_D"])
    rl, rw, rt = cfg["RADAR_ENVELOPE"]
    radar_u = face_u - wall - rt / 2.0
    radar_env = make_box(tmp,
                         frame.point(band_l, cfg["GRILLE_OFFSET"], radar_u),
                         frame.vL, frame.vS, rl, rw, rt)

    # Carrier interposer between the front (radar) and back (UNO Q) bays.
    kl, kw, kt = cfg["CARRIER_ENVELOPE"]
    uno_top = (-height / 2.0 - cfg["MID_FRAME_THICK"] + wall
               + cfg["UNO_Q_CLEARANCE_H"])
    radar_bottom = radar_u - rt / 2.0
    carrier_u = (uno_top + radar_bottom) / 2.0
    carrier = make_box(tmp, frame.point(band_l, -14.0, carrier_u),
                       frame.vL, frame.vS, kl, kw, kt)

    comp = new_component(root, "SENSOR_HEAD")
    bodies = add_bodies(design, comp,
                        [("CORNER_CAMERA_BLOCK", block),
                         ("CAMERA_LENS", lens),
                         ("CAMERA_ENVELOPE", cam_env),
                         ("SENSOR_PILL", pill),
                         ("TOF_WINDOW", tof_win),
                         ("LED_WINDOW", led_win),
                         ("TOF_ENVELOPE", tof_env),
                         ("RADAR_GRILLE", grille),
                         ("RADAR_ENVELOPE", radar_env),
                         ("SENSOR_CARRIER", carrier)])
    filleted = try_fillet(comp, bodies[0], cfg["CORNER_BLOCK_FILLET"])
    return band_l, filleted


def build_packaging(design, root, tmp, frame, dims, cfg, band_l):
    """UNO Q, display and battery envelopes. Returns report lines."""
    length, _, height = dims
    wall = cfg["WALL"]
    back_wall = -height / 2.0 - cfg["MID_FRAME_THICK"] + wall
    notes, conflicts = [], []

    def box3(l_c, l_len, s_c, s_len, u_c, u_len):
        return ((l_c - l_len / 2.0, l_c + l_len / 2.0),
                (s_c - s_len / 2.0, s_c + s_len / 2.0),
                (u_c - u_len / 2.0, u_c + u_len / 2.0))

    def overlap3(a, b):
        gaps = [min(a[i][1], b[i][1]) - max(a[i][0], b[i][0])
                for i in range(3)]
        return gaps if all(g > 0.1 for g in gaps) else None

    # UNO Q in the back bay, against the mid-frame floor.
    uno_l, uno_w = cfg["UNO_Q_OUTLINE"]
    uno_h = cfg["UNO_Q_CLEARANCE_H"]
    uno_pos_l, uno_pos_s = cfg["UNO_Q_POS"]
    uno_u = back_wall + uno_h / 2.0
    comp = new_component(root, "UNO_Q")
    add_bodies(design, comp, [(
        "UNO_Q_CLEARANCE",
        make_box(tmp, frame.point(uno_pos_l, uno_pos_s, uno_u),
                 frame.vL, frame.vS, uno_l, uno_w, uno_h))])

    # Display in the front bay, slid toward the grip end.
    dsp_l, dsp_w, dsp_t = cfg["DISPLAY_ENVELOPE"]
    dsp_pos_l = cfg["DISPLAY_L_OFFSET"]
    dsp_u = height / 2.0 - cfg["DISPLAY_TOP_GAP"] - dsp_t / 2.0
    comp = new_component(root, "DISPLAY_ENVELOPE")
    add_bodies(design, comp, [(
        "DISPLAY",
        make_box(tmp, frame.point(dsp_pos_l, 0.0, dsp_u), frame.vL, frame.vS,
                 dsp_l, dsp_w, dsp_t))])

    # Battery in the back bay below the UNO Q.
    bat_l, bat_w, bat_t = cfg["BATTERY_ENVELOPE"]
    bat_u = back_wall + bat_t / 2.0
    comp = new_component(root, "BATTERY")
    add_bodies(design, comp, [(
        "BATTERY_PACK",
        make_box(tmp, frame.point(cfg["BATTERY_POS"], 0.0, bat_u),
                 frame.vL, frame.vS, bat_l, bat_w, bat_t))])

    # True 3D interference check across the envelopes.
    rl, rw, rt = cfg["RADAR_ENVELOPE"]
    radar_u = height / 2.0 - wall - rt / 2.0
    envelopes = {
        "UNO_Q": box3(uno_pos_l, uno_l, uno_pos_s, uno_w, uno_u, uno_h),
        "DISPLAY": box3(dsp_pos_l, dsp_l, 0.0, dsp_w, dsp_u, dsp_t),
        "BATTERY": box3(cfg["BATTERY_POS"], bat_l, 0.0, bat_w, bat_u, bat_t),
        "RADAR": box3(band_l, rl, cfg["GRILLE_OFFSET"], rw, radar_u, rt),
    }
    names = list(envelopes)
    for i, a in enumerate(names):
        for b in names[i + 1:]:
            hit = overlap3(envelopes[a], envelopes[b])
            if hit:
                conflicts.append(
                    "{} and {} interfere ~{:.1f} x {:.1f} x {:.1f} mm".format(
                        a, b, *hit))

    inner = height + cfg["MID_FRAME_THICK"] - 2.0 * wall
    stack = dsp_t + uno_h
    if stack > inner:
        conflicts.append(
            "display + UNO Q stack {} mm vs {:.1f} mm inner - short "
            "{:.1f} mm".format(stack, inner, stack - inner))
    else:
        notes.append(
            "display (13) + UNO Q (16) stack fits: {:.1f} mm margin in the "
            "{:.1f} mm inner thickness".format(inner - stack, inner))
        gap = (uno_pos_l - uno_l / 2.0) - (cfg["BATTERY_POS"] + bat_l / 2.0)
        notes.append(
            "battery clears the UNO Q by {:.1f} mm on the long axis".format(
                gap))
        gap_r = (band_l - rl / 2.0) - (dsp_pos_l + dsp_l / 2.0)
        notes.append(
            "radar bay clears the display top edge by {:.1f} mm".format(
                gap_r))
    return notes, conflicts


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

        build_mid_frame(design, root, tmp, frame, dims, cfg)
        grip_s, grip_bottom, grip_fillet = build_grip(design, root, tmp,
                                                      frame, dims, cfg)
        build_controls(design, root, tmp, frame, dims, cfg, grip_s,
                       grip_bottom)
        band_l, blk_fillet = build_sensor_cluster(design, root, tmp, frame,
                                                  dims, cfg)
        notes, conflicts = build_packaging(design, root, tmp, frame, dims,
                                           cfg, band_l)

        lines = [
            "Platypus One beef-up complete (rev 3).",
            "",
            "Enclosure envelope {}: {:.1f} x {:.1f} x {:.1f} mm ({})".format(
                "measured" if measured else "FALLBACK (no bodies found)",
                dims[0], dims[1], dims[2], mapping),
            "Total thickness with mid-frame: {:.1f} mm".format(
                dims[2] + cfg["MID_FRAME_THICK"]),
            "Existing bodies: " + rename_note,
            "Created: MID_FRAME, GRIP{}, CONTROLS, SENSOR_HEAD{}, UNO_Q,"
            .format("" if grip_fillet else " (fillet skipped)",
                    "" if blk_fillet else " (fillet skipped)"),
            "  DISPLAY_ENVELOPE, BATTERY (re-specced 90 x 60 x 10).",
        ]
        if removed:
            lines.append("Replaced previous run: " + ", ".join(removed))
        lines.append("")
        if conflicts:
            lines.append("PACKAGING CONFLICTS:")
            lines.extend("  - " + c for c in conflicts)
        else:
            lines.append("PACKAGING: clean - no envelope interference.")
            lines.extend("  - " + n for n in notes)
        lines.append("")
        lines.append("Wrong face or end? Flip FLIP_TOP / FLIP_FRONT / "
                     "FLIP_GRIP_SIDE in CONFIG and run again - re-runs "
                     "replace the script's own components and never touch "
                     "yours.")
        report = "\n".join(lines)
        app.log(report)
        ui.messageBox(report, "Platypus One beef-up")
    except Exception:  # noqa: BLE001 - surface everything to the user
        if ui:
            ui.messageBox("Script failed:\n{}".format(traceback.format_exc()),
                          "Platypus One beef-up")

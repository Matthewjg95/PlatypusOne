// PlatypusOne — lid for EnclosurePrototypeTall (Fusion export, 116.1 x 190 x 33)
// Modelled in the enclosure's own coordinates (lid sits on the wall tops at
// z = 33), then flipped for printing. Flipping an L mirrors its footprint, so
// the print-orientation STL looks mirrored on the bed: that is correct.
//
// Shell numbers below were measured from the STL, not guessed:
//   outer L: x 0..116.1, y 0..190, notch x 81.1..116.1 / y 0..140; walls 2.
//   slots (bottom z = 2, run y 145..185):  ToF x 94.8..96.1, camera x 111.0..112.2
//   right-wall window y 142..180, z 4..24.
//   display tray: walls x 6..75.1 / y 6..118.3 to z 27; panel pocket
//   x 7.5..73.6 / y 7.5..116.8 sitting on a ledge at z 21.2.
//
// Render (print orientation): openscad -o enclosure_prototype_lid.stl enclosure_prototype_lid.scad
// Fit check against the shell: -D 'part="assembly"' -D 'shell="EnclosurePrototypeTall.stl"'

part  = "print";            // "print" | "assembly"
shell = "";                 // path to the enclosure STL for the assembly view

wall_top = 33;
lid_t    = 2.0;
lip_d    = 4.0;             // locating lip depth into the shell (stops at z 29, tray is 27)
lip_t    = 1.6;
fit      = 0.3;             // lip-to-wall clearance per side

// ---- boards standing in the floor slots ----
slot_bottom = 2.0;
slot_y      = [145, 185];
// [slot centre x, board height standing in the slot]
cam = [111.6, 24.0];        // B0394 nominal 25 x 24, 24 standing (FFC edge down)
tof = [95.45, 22.9];        // Pololu #3419 12.7 x 22.9, long side standing
groove_w  = 1.5;            // board 1.0 + clearance
groove_d  = 2.0;            // how far the clamp grips down each board face
clamp_t   = 1.2;            // clamp wall each side of the groove
preload   = 0.1;            // groove ceiling this much below the board top: light press

// ---- display ----
ledge_z     = 21.2;
panel_t     = 7.9;          // ledge to glass top. 4-DSI-TOUCH-A back is 7.9: CALIPER YOURS
pocket      = [[7.5, 7.5], [73.6, 116.8]];
tray        = [[6, 6], [75.1, 118.3]];
border      = 2.5;          // how much of the glass edge the frame covers, per side
win_chamfer = 1.5;          // bevel on the outside of the window

inner_L = [[2,2],[79.1,2],[79.1,142],[114.1,142],[114.1,188],[2,188]];
outer_L = [[0,0],[81.1,0],[81.1,140],[116.1,140],[116.1,190],[0,190]];

function win() = [pocket[0] + [border, border], pocket[1] - [border, border]];

module rect(r, grow = 0) translate(r[0] - [grow, grow]) square(r[1] - r[0] + [2*grow, 2*grow]);

module lid() {
    glass_top = ledge_z + panel_t - preload;
    difference() {
        union() {
            // plate
            translate([0,0,wall_top]) linear_extrude(lid_t) polygon(outer_L);
            // display frame: presses the glass border down onto the ledge
            translate([0,0,glass_top]) linear_extrude(wall_top - glass_top + 0.01)
                difference() { rect(tray); rect(win()); }
        }
        // display window through the plate, bevelled outward
        translate([0,0,wall_top - 1]) linear_extrude(lid_t + 2) rect(win());
        translate([0,0,wall_top + lid_t - win_chamfer]) hull() {
            linear_extrude(0.01) rect(win());
            translate([0,0,win_chamfer]) linear_extrude(0.01) rect(win(), win_chamfer);
        }
    }
    // locating lip, follows the inner wall all the way round
    translate([0,0,wall_top - lip_d]) linear_extrude(lip_d + 0.01)
        difference() {
            offset(delta = -fit) polygon(inner_L);
            offset(delta = -fit - lip_t) polygon(inner_L);
        }
    clamp(cam); clamp(tof);
}

// grooved rib that comes down onto the top edge of a board
module clamp(b) {
    x = b[0]; top = slot_bottom + b[1];
    ceil_z = top - preload;
    w = groove_w + 2 * clamp_t;
    difference() {
        translate([x - w/2, slot_y[0], ceil_z - groove_d]) cube([w, slot_y[1] - slot_y[0], wall_top - ceil_z + groove_d + 0.01]);
        translate([x - groove_w/2, slot_y[0] - 1, ceil_z - groove_d - 1]) cube([groove_w, slot_y[1] - slot_y[0] + 2, groove_d + 1]);
        // lead-in chamfer so the board edge finds the groove
        translate([x, slot_y[0] - 1, ceil_z - groove_d]) rotate([-90,0,0]) cylinder(d = groove_w + 1.4, h = slot_y[1] - slot_y[0] + 2, $fn = 4);
    }
}

if (part == "assembly") {
    if (shell != "") color("gray", 0.5) import(shell);
    color("orange") lid();
} else {
    // flip onto the plate: top face on the bed, lip and clamps up
    translate([0, 0, wall_top + lid_t]) rotate([180, 0, 0]) lid();
}

// PlatypusOne — IMX219 camera test bracket (bench fixture for #40)
// Two parts: tilting camera plate + yoke stand with 1/4"-20 tripod nut trap.
// Pi-camera-style hole pattern is PROVISIONAL: caliper each module first
// (hardware/pcb/MECHANICAL_FUSION_INPUTS.md §5) and edit the params below.
//
// Render: openscad -D 'part="plate"' -o plate.stl camera_test_bracket.scad
//         openscad -D 'part="yoke"'  -o yoke.stl  camera_test_bracket.scad

part = "assembly";          // "plate" | "yoke" | "assembly"
tilt = 30;                  // assembly preview only: 0 = level, 90 = straight down

// ---- camera board (Pi camera v2 style; B0390/B0393/B0394 ~25 x 24) ----
board_w     = 25;           // along X
board_h     = 24;           // along Y, +Y = edge away from FFC
hole_dx     = 21;           // horizontal hole pitch
hole_dy     = 12.5;         // vertical hole pitch
hole_top    = 2;            // top hole row centre from board top edge
pilot_d     = 1.75;         // M2 self-tapping / thread-forming into plastic
standoff_d  = 5;
standoff_h  = 5;            // clears back-side FFC connector + parts

// ---- camera plate ----
plate_w = 34; plate_h = 34; plate_t = 4;
ffc_notch_w = 18; ffc_notch_d = 4;
cheek_w = 6; cheek_l = 12; cheek_h = 10;   // pivot blocks on the plate sides
pivot_z = 5;                                // pivot axis height in plate coords
m3_clear = 3.4; m3_nut_af = 5.7; m3_nut_t = 2.6;

// ---- yoke stand ----
clear     = 0.5;            // per side between cheek and arm
arm_t     = 5;  arm_w = 14;
pivot_h   = 50;             // pivot axis above table
base_w    = 72; base_d = 56; base_t = 8;
tripod_nut_af = 11.3; tripod_nut_t = 5.8; tripod_bolt_d = 6.8;  // 1/4"-20
bench_hole_d = 4.5;

$fn = 48;
half_span = plate_w/2 + cheek_w;            // cheek outer face
arm_in    = half_span + clear;

module hex(af, h) cylinder(d = af / cos(30), h = h, $fn = 6);

module plate() {
    difference() {
        union() {
            translate([-plate_w/2, -plate_h/2, 0]) cube([plate_w, plate_h, plate_t]);
            for (s = [-1, 1])
                translate([s > 0 ? plate_w/2 : -half_span, -cheek_l/2, 0])
                    cube([cheek_w, cheek_l, cheek_h]);
            for (p = holes()) translate([p[0], p[1], 0])
                cylinder(d = standoff_d, h = plate_t + standoff_h);
        }
        for (p = holes()) translate([p[0], p[1], -1])
            cylinder(d = pilot_d, h = plate_t + standoff_h + 2);
        // FFC exit notch at the bottom edge
        translate([-ffc_notch_w/2, -plate_h/2 - 1, -1]) cube([ffc_notch_w, ffc_notch_d + 1, plate_t + 2]);
        // pivot bolt holes + captive M3 nut slots dropped in from the top
        for (s = [-1, 1]) {
            translate([s > 0 ? plate_w/2 - 0.01 : -half_span - 1, 0, pivot_z]) rotate([0, 90, 0])
                cylinder(d = m3_clear, h = cheek_w + 1.01);
            translate([s * (plate_w/2 + cheek_w/2) - m3_nut_t/2, -m3_nut_af/2, pivot_z - m3_nut_af/2 / cos(30)])
                cube([m3_nut_t, m3_nut_af, cheek_h]);
        }
        // orientation text on the back (bed side, mirrored so it reads from behind)
        translate([0, 8, -0.01]) mirror([1, 0, 0])
            linear_extrude(0.61) text("TOP", size = 4, halign = "center", valign = "center");
    }
}

function holes() = let(ty = board_h/2 - hole_top)
    [[-hole_dx/2, ty], [hole_dx/2, ty], [-hole_dx/2, ty - hole_dy], [hole_dx/2, ty - hole_dy]];

module arm(s) {
    x0 = s > 0 ? arm_in : -arm_in - arm_t;
    difference() {
        translate([x0, 0, 0]) rotate([90, 0, 90]) linear_extrude(arm_t)
            hull() {
                translate([-arm_w/2, 0]) square([arm_w, base_t]);
                translate([0, pivot_h]) circle(d = arm_w);
            }
        translate([0, 0, pivot_h]) rotate([0, 90, 0]) cylinder(d = m3_clear, h = 2 * (arm_in + arm_t) + 2, center = true);
    }
}

module yoke() {
    difference() {
        union() {
            translate([-base_w/2, -base_d/2, 0]) cube([base_w, base_d, base_t]);
            arm(1); arm(-1);
        }
        // 1/4"-20 tripod nut trap from the top, bolt hole through the bottom
        translate([0, 0, base_t - tripod_nut_t]) hex(tripod_nut_af, tripod_nut_t + 1);
        translate([0, 0, -1]) cylinder(d = tripod_bolt_d, h = base_t + 2);
        // bench screw / clamp holes
        for (x = [-1, 1], y = [-1, 1]) translate([x * (base_w/2 - 6), y * (base_d/2 - 6), -1])
            cylinder(d = bench_hole_d, h = base_t + 2);
    }
}

if (part == "plate") plate();
else if (part == "yoke") yoke();
else {
    color("orange") yoke();
    color("steelblue") translate([0, 0, pivot_h]) rotate([90 + tilt, 0, 0]) translate([0, 0, -pivot_z]) plate();
}

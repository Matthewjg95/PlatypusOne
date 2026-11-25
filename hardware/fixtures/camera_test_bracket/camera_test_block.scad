// PlatypusOne — slide-in camera test block (bench fixture for #40)
// One solid block. The camera board drops into edge grooves from the top,
// lens facing -Y (out the front window). No screws. Swap B0394/B0393/B0390 by
// lifting one out and dropping the next in.
// Board size: B0394 vendor drawing, 25 x 24 mm (FFC on a 25 mm edge), the
// same nominal used for the bench perception plate. Not yet calipered: check
// a board against board_w/board_h before printing.
//
// Render: openscad -o camera_test_block.stl camera_test_block.scad

board_w   = 25;     // across the slot (FFC edge)
board_h   = 24;     // slide direction
board_t   = 1.0;    // PCB thickness (nominal)

slot_t    = 1.5;    // groove width: board_t + FDM clearance; 1.4 if loose
side_gap  = 0.3;    // per side across the width
lip       = 1.5;    // how far the groove lips overlap each board edge
front_t   = 2.0;    // front wall in front of the board
rear_rib  = 2.5;    // groove back wall depth
floor_h   = 8.0;    // board bottom edge sits this high
grip      = 5.0;    // board top sticks out this much for pulling it
wall      = 4.0;    // side wall beyond the groove
depth     = 32;     // front-to-back, for a stable footprint
ffc_w     = 18;     // ribbon/connector opening
ffc_drop  = 6.5;    // drop hole depth (Y) from the slot front: connector + bend
tunnel_h  = 3.0;    // ribbon tunnel under the floor, out the back

slot_w  = board_w + 2 * side_gap;
win_w   = board_w - 2 * lip;
block_w = slot_w + 2 * wall;
block_h = floor_h + board_h - grip;
slot_y0 = front_t;                    // slot front face
slot_y1 = front_t + slot_t;           // slot back face

difference() {
    translate([-block_w/2, 0, 0]) cube([block_w, depth, block_h]);
    // front window, open to the top: lens / M12 holder / VCM clear
    translate([-win_w/2, -1, floor_h + lip]) cube([win_w, front_t + 2, block_h]);
    // board slot (full width incl. grooves), open to the top
    translate([-slot_w/2, slot_y0, floor_h]) cube([slot_w, slot_t, block_h]);
    // rear cavity behind the board, between the groove ribs, open top + back:
    // clears the back-side connector and parts and gives finger access
    translate([-win_w/2, slot_y1 - 0.01, floor_h]) cube([win_w, depth, block_h]);
    // ribbon exit: drops through the floor under the board, then runs out the
    // back in a tunnel under the floor (bridged; floor stays one piece)
    translate([-ffc_w/2, slot_y0, -1]) cube([ffc_w, ffc_drop, floor_h + 2]);
    translate([-ffc_w/2, slot_y0, -1]) cube([ffc_w, depth, tunnel_h + 1]);
    // chamfered lead-in at the slot mouth
    translate([0, slot_y0 + slot_t/2, block_h]) rotate([0, 90, 0])
        cylinder(d = slot_t + 2, h = slot_w, center = true, $fn = 4);
}

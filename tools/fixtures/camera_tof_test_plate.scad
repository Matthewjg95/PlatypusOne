// PlatypusOne removable perception-head test plate. Millimeters.
// Board component/optics sides face +Z, away from the plate.
// Face-on view: camera left, ToF right, camera ribbon exits toward -Y.
$fn = 48;
plate_t = 3.2;
cam_origin = [0,-5]; // center of the camera's rectangular hole pattern
cam_pitch = [21,12.5]; // nominal from the B0394 drawing, bench fit pending
tof_center = [33,4.6]; // nominal sensor location, not a measured baseline
tof_hole_x = 10.2-6.3; // carrier hole and sensor X, both from the left edge
tof_pitch = 22.9-2*2.5;
ear_centers = [-20,48];

module slot(span, width, angle=0) {
    rotate(angle) hull() {
        translate([-span/2,0]) circle(d=width);
        translate([ span/2,0]) circle(d=width);
    }
}

module rounded_outline(x0,y0,x1,y1,r) {
    hull() for(x=[x0+r,x1-r]) for(y=[y0+r,y1-r])
        translate([x,y]) circle(r=r);
}

linear_extrude(height=plate_t, convexity=8)
difference() {
    rounded_outline(-26,-22,54,21,3);
    // Rear FFC exit and latch access. The bottom of the notch is open.
    translate([-8.2,-23]) square([16.4,14]);
    // Relief below the ToF through-hole wiring. Fit spacers for the rest.
    rounded_outline(27,-8,34.5,17.5,1.5);
    // Short horizontal adjustment on each B0394 fastener.
    for(x=[-1,1]) for(y=[-1,1])
        translate(cam_origin + [x*cam_pitch.x/2,y*cam_pitch.y/2]) slot(0.8,2.9);
    // Pololu drawing: 17.9 mm pitch, not the 11.4 mm sensor-center dimension.
    for(y=[-1,1])
        translate(tof_center+[tof_hole_x,y*tof_pitch/2]) slot(0.9,2.4,90);
    // Two external M3 slots for a test stand, clamp plate, or future shell.
    for(x=ear_centers) translate([x,3]) slot(3,3.5,90);
    // Orientation index, points UP from the optics side.
    polygon([[18,15],[24,15],[21,19]]);
}

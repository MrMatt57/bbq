// ============================================================
// Pit Claw Temperature Controller Enclosure
// Parametric design — 3 printable parts
// Open in OpenSCAD, set 'part' variable, render, export STL
// ============================================================

// === PART SELECTOR ===
// Set to "all", "bezel", "shell", or "kickstand" to render
part = "all";

// === Board Dimensions ===
wt32_pcb_w = 60;       // WT32-SC01 Plus width (mm)
wt32_pcb_h = 92;       // WT32-SC01 Plus height (mm)
wt32_pcb_d = 10.8;     // WT32-SC01 Plus depth (mm)
display_w = 50;         // Active display area width
display_h = 74;         // Active display area height
carrier_w = 50;         // Carrier perfboard width
carrier_h = 70;         // Carrier perfboard height
carrier_d = 15;         // Carrier board component height

// === Enclosure ===
wall = 2.5;             // Wall thickness
corner_r = 4;           // Corner radius
tolerance = 0.5;        // Fit tolerance per side
screw_d = 3.0;          // M3 screw hole diameter

// === Panel Mount Holes ===
probe_jack_d = 6.2;     // 2.5mm mono jack mounting hole
barrel_jack_d = 12.2;   // DC barrel jack mounting hole
usb_c_w = 10;           // USB-C cutout width
usb_c_h = 4;            // USB-C cutout height

// === Derived Dimensions ===
// Internal cavity sized to hold WT32 PCB + carrier board stack
inner_w = wt32_pcb_w + tolerance * 2;
inner_h = wt32_pcb_h + tolerance * 2;
inner_d = wt32_pcb_d + carrier_d + tolerance * 2;

// Outer shell dimensions
outer_w = inner_w + wall * 2;
outer_h = inner_h + wall * 2;
outer_d = inner_d + wall * 2;

// Bezel depth (front plate portion)
bezel_d = wall + 2;     // Wall + lip depth
bezel_lip = 1.5;        // Lip overlap on display edges

// Shell depth (remainder)
shell_d = outer_d - bezel_d + 2; // +2 for overlap/mating

// Screw boss dimensions
boss_d = screw_d + 4;   // Boss outer diameter
boss_h = 8;             // Boss height for engagement

// WT32 PCB mounting hole positions (approximate, symmetric)
// Holes are near the 4 corners of the 60x92mm board
pcb_hole_inset_w = 3;   // Inset from PCB edge (width direction)
pcb_hole_inset_h = 3;   // Inset from PCB edge (height direction)

// Kickstand dimensions
ks_w = 25;              // Kickstand width
ks_h = outer_h * 0.65;  // Kickstand length (~65% of case height)
ks_thick = 3;           // Kickstand thickness
ks_hinge_d = 5;         // Hinge pin diameter
ks_hinge_slot_w = ks_w + 1; // Slot slightly wider than kickstand

// $fn for curves
$fn = 40;

// ============================================================
// Utility Modules
// ============================================================

// Rounded rectangle (2D) for extrusion
module rounded_rect_2d(w, h, r) {
    offset(r = r)
        square([w - 2*r, h - 2*r], center = true);
}

// Rounded box (3D) — solid block with rounded vertical edges
module rounded_box(w, h, d, r) {
    linear_extrude(height = d)
        rounded_rect_2d(w, h, r);
}

// Screw boss — cylinder with screw hole
module screw_boss(od, id, h) {
    difference() {
        cylinder(d = od, h = h);
        translate([0, 0, -0.5])
            cylinder(d = id, h = h + 1);
    }
}

// Screw hole — simple through hole
module screw_hole(d, h) {
    translate([0, 0, -0.5])
        cylinder(d = d, h = h + 1);
}

// Ventilation slot
module vent_slot(length, width, depth) {
    translate([-length/2, -width/2, 0])
        cube([length, width, depth + 1]);
}

// Panel-mount circular hole
module panel_hole(d, wall_t) {
    rotate([90, 0, 0])
        cylinder(d = d, h = wall_t + 2, center = true);
}

// ============================================================
// Corner screw positions (relative to enclosure center XY)
// ============================================================

// Positions of the 4 corner screw bosses
screw_positions = [
    [ (inner_w/2 - boss_d/2),  (inner_h/2 - boss_d/2)],
    [-(inner_w/2 - boss_d/2),  (inner_h/2 - boss_d/2)],
    [ (inner_w/2 - boss_d/2), -(inner_h/2 - boss_d/2)],
    [-(inner_w/2 - boss_d/2), -(inner_h/2 - boss_d/2)]
];

// WT32 PCB mounting hole positions (relative to enclosure center XY)
pcb_mount_positions = [
    [ (wt32_pcb_w/2 - pcb_hole_inset_w),  (wt32_pcb_h/2 - pcb_hole_inset_h)],
    [-(wt32_pcb_w/2 - pcb_hole_inset_w),  (wt32_pcb_h/2 - pcb_hole_inset_h)],
    [ (wt32_pcb_w/2 - pcb_hole_inset_w), -(wt32_pcb_h/2 - pcb_hole_inset_h)],
    [-(wt32_pcb_w/2 - pcb_hole_inset_w), -(wt32_pcb_h/2 - pcb_hole_inset_h)]
];

// ============================================================
// Part 1: Front Bezel
// ============================================================

module front_bezel() {
    pcb_standoff_h = 3; // Standoff height from bezel inner face

    difference() {
        union() {
            // Main bezel plate with rounded corners
            rounded_box(outer_w, outer_h, bezel_d, corner_r);

            // Screw bosses on inner face (extending inward, mating with shell)
            for (pos = screw_positions) {
                translate([pos[0], pos[1], bezel_d])
                    screw_boss(boss_d, screw_d, boss_h);
            }

            // PCB mounting standoffs on inner face
            for (pos = pcb_mount_positions) {
                translate([pos[0], pos[1], bezel_d])
                    cylinder(d = screw_d + 2.5, h = pcb_standoff_h);
            }
        }

        // Display cutout — through the bezel
        // Centered in XY, cut all the way through Z
        translate([0, 0, -0.5])
            linear_extrude(height = bezel_d + 1)
                rounded_rect_2d(display_w, display_h, 1.5);

        // PCB recess on the back side of the bezel
        // Slightly larger than the display to allow the PCB glass to sit flush
        translate([0, 0, wall])
            linear_extrude(height = bezel_d)
                rounded_rect_2d(wt32_pcb_w + tolerance, wt32_pcb_h + tolerance, 1);

        // PCB mounting screw holes through the standoffs
        for (pos = pcb_mount_positions) {
            translate([pos[0], pos[1], bezel_d - 0.5])
                cylinder(d = screw_d * 0.8, h = pcb_standoff_h + 1);
        }

        // Screw holes through the bosses (for M3 screws from front)
        for (pos = screw_positions) {
            translate([pos[0], pos[1], -0.5])
                cylinder(d = screw_d, h = bezel_d + boss_h + 1);
        }
    }
}

// ============================================================
// Part 2: Rear Shell
// ============================================================

module rear_shell() {
    carrier_standoff_h = 5; // Height of carrier board standoffs from shell floor

    // Carrier board mounting positions (centered, 4 corners of 50x70)
    carrier_mount_inset = 3;
    carrier_positions = [
        [ (carrier_w/2 - carrier_mount_inset),  (carrier_h/2 - carrier_mount_inset)],
        [-(carrier_w/2 - carrier_mount_inset),  (carrier_h/2 - carrier_mount_inset)],
        [ (carrier_w/2 - carrier_mount_inset), -(carrier_h/2 - carrier_mount_inset)],
        [-(carrier_w/2 - carrier_mount_inset), -(carrier_h/2 - carrier_mount_inset)]
    ];

    difference() {
        union() {
            // Hollow box: outer shape minus inner cavity
            difference() {
                // Outer shell
                rounded_box(outer_w, outer_h, shell_d, corner_r);

                // Inner cavity (open on top, which is the mating face)
                translate([0, 0, wall])
                    rounded_box(inner_w, inner_h, shell_d, corner_r - wall/2);
            }

            // Corner screw bosses (receiving holes, inside the shell)
            for (pos = screw_positions) {
                translate([pos[0], pos[1], wall])
                    screw_boss(boss_d, screw_d, shell_d - wall);
            }

            // Carrier board standoffs
            for (pos = carrier_positions) {
                translate([pos[0], pos[1], wall])
                    cylinder(d = screw_d + 2.5, h = carrier_standoff_h);
            }
        }

        // --- Bottom edge panel-mount holes ---
        // Bottom edge is at Y = -outer_h/2
        // Holes go through the wall in Y direction

        bottom_y = -outer_h/2;
        hole_z = wall + 8; // Height of hole centers above shell floor

        // USB-C cutout (leftmost)
        usb_x = -inner_w/2 + 12;
        translate([usb_x, bottom_y, hole_z])
            rotate([90, 0, 0])
                linear_extrude(height = wall + 2, center = true)
                    square([usb_c_w, usb_c_h], center = true);

        // 3x Probe jack holes (centered group)
        probe_spacing = 14;
        probe_start_x = -probe_spacing;
        for (i = [0:2]) {
            translate([probe_start_x + i * probe_spacing, bottom_y, hole_z])
                panel_hole(probe_jack_d, wall);
        }

        // DC barrel jack (rightmost)
        barrel_x = inner_w/2 - 12;
        translate([barrel_x, bottom_y, hole_z])
            panel_hole(barrel_jack_d, wall);

        // --- Right side edge cable slots ---
        // Right edge is at X = outer_w/2
        right_x = outer_w/2;
        cable_slot_w = 8;
        cable_slot_h = 5;

        // Fan cable slot
        translate([right_x, outer_h/4 - 8, wall + 5])
            rotate([0, 90, 0])
                linear_extrude(height = wall + 2, center = true)
                    square([cable_slot_h, cable_slot_w], center = true);

        // Servo cable slot
        translate([right_x, outer_h/4 + 8, wall + 5])
            rotate([0, 90, 0])
                linear_extrude(height = wall + 2, center = true)
                    square([cable_slot_h, cable_slot_w], center = true);

        // --- Top area ventilation slots ---
        vent_count = 5;
        vent_len = 20;
        vent_w = 2;
        vent_spacing = 5;
        vent_start_y = outer_h/2 - 20;

        for (i = [0 : vent_count - 1]) {
            // Left vent group
            translate([-inner_w/4, vent_start_y - i * vent_spacing, shell_d - wall - 0.5])
                vent_slot(vent_len, vent_w, wall);
            // Right vent group
            translate([inner_w/4, vent_start_y - i * vent_spacing, shell_d - wall - 0.5])
                vent_slot(vent_len, vent_w, wall);
        }

        // --- Kickstand hinge slot on the back face (Z = 0) ---
        // Slot through the back wall for the kickstand hinge tab
        translate([0, -outer_h/2 + ks_h * 0.4 + 10, -0.5])
            linear_extrude(height = wall + 1)
                hull() {
                    translate([-ks_hinge_slot_w/2, 0]) circle(d = ks_hinge_d + 1);
                    translate([ ks_hinge_slot_w/2, 0]) circle(d = ks_hinge_d + 1);
                }

        // Carrier board standoff screw holes
        for (pos = carrier_positions) {
            translate([pos[0], pos[1], wall - 0.5])
                cylinder(d = screw_d * 0.8, h = carrier_standoff_h + 1);
        }

        // Screw holes through the bosses
        for (pos = screw_positions) {
            translate([pos[0], pos[1], wall - 0.5])
                cylinder(d = screw_d, h = shell_d);
        }
    }
}

// ============================================================
// Part 3: Kickstand
// ============================================================

module kickstand() {
    hinge_r = ks_hinge_d / 2;

    // Main arm
    difference() {
        union() {
            // Flat arm body
            translate([-ks_w/2, 0, 0])
                cube([ks_w, ks_h, ks_thick]);

            // Rounded hinge tab at one end
            translate([0, 0, ks_thick/2])
                rotate([0, 0, 0])
                    hull() {
                        translate([-ks_w/2 + hinge_r, 0, 0])
                            rotate([0, 90, 0])
                                cylinder(r = hinge_r, h = ks_w - ks_hinge_d);
                    }
        }

        // Snap-fit groove near the hinge end
        // A small notch that lets the hinge tab click into the slot
        translate([-ks_w/2 - 0.5, -hinge_r, ks_thick/2 - 0.4])
            cube([ks_w + 1, 1.5, 0.8]);
    }

    // Anti-slip foot at the bottom end
    translate([-ks_w/2, ks_h - 1, 0])
        cube([ks_w, 1, ks_thick + 1.5]);
}

// ============================================================
// Assembly View / Part Selector
// ============================================================

if (part == "bezel") {
    front_bezel();
}
else if (part == "shell") {
    rear_shell();
}
else if (part == "kickstand") {
    kickstand();
}
else {
    // "all" — arranged for visualization
    // Front bezel at origin
    front_bezel();

    // Rear shell offset to the right
    translate([outer_w + 15, 0, 0])
        rear_shell();

    // Kickstand offset further right
    translate([(outer_w + 15) * 2, -outer_h/2 + 10, 0])
        kickstand();
}

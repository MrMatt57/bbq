// ============================================================
// Pit Claw Temperature Controller Enclosure
// Parametric design — 3 printable parts (snap-fit assembly)
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

// === Snap-Fit Parameters ===
// Bezel has a perimeter rim that drops into the shell cavity.
// Wedge-shaped catches on the rim seat into grooves in the shell walls.
// PETG wall flex provides the snap action.
snap_rim_h = 6;         // How deep the bezel rim extends into the shell
snap_rim_wall = 1.5;    // Rim wall thickness
snap_rim_gap = 0.25;    // Clearance between rim and shell wall (per side)
snap_catch_depth = 0.5; // How far each catch protrudes past the rim face
snap_catch_w = 12;      // Width of each catch bump
snap_catch_h = 1.5;     // Height of each catch bump (Z extent)
snap_catch_pos = 2;     // Distance from rim tip to top of catch (ramp end)

// === PCB Mounting (screws for display PCB only — not case closure) ===
screw_d = 3.0;          // M3 screw hole diameter
pcb_hole_inset_w = 3;   // Inset from PCB edge (width, approximate)
pcb_hole_inset_h = 3;   // Inset from PCB edge (height, approximate)

// === Panel Mount Holes ===
probe_jack_d = 6.2;     // 2.5mm mono jack mounting hole
barrel_jack_d = 12.2;   // DC barrel jack mounting hole
usb_c_w = 10;           // USB-C cutout width
usb_c_h = 4;            // USB-C cutout height

// === Derived Dimensions ===
inner_w = wt32_pcb_w + tolerance * 2;
inner_h = wt32_pcb_h + tolerance * 2;
inner_d = wt32_pcb_d + carrier_d + tolerance * 2;

outer_w = inner_w + wall * 2;
outer_h = inner_h + wall * 2;
outer_d = inner_d + wall * 2;

bezel_d = wall + 2;     // Bezel plate depth (wall + PCB lip)
shell_d = outer_d - bezel_d;

// Snap rim dimensions (fits inside shell cavity)
snap_rim_outer_w = inner_w - snap_rim_gap * 2;
snap_rim_outer_h = inner_h - snap_rim_gap * 2;
snap_rim_inner_w = snap_rim_outer_w - snap_rim_wall * 2;
snap_rim_inner_h = snap_rim_outer_h - snap_rim_wall * 2;

// Rim corner radii (must be positive)
rim_outer_r = max(corner_r - wall - snap_rim_gap, 1);
rim_inner_r = max(rim_outer_r - snap_rim_wall, 0.5);

// === Kickstand ===
ks_w = 25;
ks_h = outer_h * 0.65;
ks_thick = 3;
ks_hinge_d = 5;
ks_hinge_slot_w = ks_w + 1;

$fn = 40;

// ============================================================
// Utility Modules
// ============================================================

module rounded_rect_2d(w, h, r) {
    offset(r = r)
        square([w - 2*r, h - 2*r], center = true);
}

module rounded_box(w, h, d, r) {
    linear_extrude(height = d)
        rounded_rect_2d(w, h, r);
}

module vent_slot(length, width, depth) {
    translate([-length/2, -width/2, 0])
        cube([length, width, depth + 1]);
}

module panel_hole(d, wall_t) {
    rotate([90, 0, 0])
        cylinder(d = d, h = wall_t + 2, center = true);
}

// Snap catch wedge — protrudes in +Y from origin
// Ramped top (easy insertion), flat bottom (catch/retention)
// Width in X, height in Z, depth in Y
module snap_catch_wedge() {
    translate([-snap_catch_w/2, 0, 0])
        hull() {
            // Bottom: full protrusion (flat catch face)
            cube([snap_catch_w, snap_catch_depth, 0.01]);
            // Top: zero protrusion (ramp for easy entry)
            translate([0, 0, snap_catch_h])
                cube([snap_catch_w, 0.01, 0.01]);
        }
}

// ============================================================
// Mounting Positions
// ============================================================

// WT32 PCB mounting holes (approximate — 3mm inset from 60x92mm board edges)
pcb_mount_positions = [
    [ (wt32_pcb_w/2 - pcb_hole_inset_w),  (wt32_pcb_h/2 - pcb_hole_inset_h)],
    [-(wt32_pcb_w/2 - pcb_hole_inset_w),  (wt32_pcb_h/2 - pcb_hole_inset_h)],
    [ (wt32_pcb_w/2 - pcb_hole_inset_w), -(wt32_pcb_h/2 - pcb_hole_inset_h)],
    [-(wt32_pcb_w/2 - pcb_hole_inset_w), -(wt32_pcb_h/2 - pcb_hole_inset_h)]
];

// Carrier board standoff positions (3mm inset from 50x70mm board edges)
carrier_mount_inset = 3;
carrier_positions = [
    [ (carrier_w/2 - carrier_mount_inset),  (carrier_h/2 - carrier_mount_inset)],
    [-(carrier_w/2 - carrier_mount_inset),  (carrier_h/2 - carrier_mount_inset)],
    [ (carrier_w/2 - carrier_mount_inset), -(carrier_h/2 - carrier_mount_inset)],
    [-(carrier_w/2 - carrier_mount_inset), -(carrier_h/2 - carrier_mount_inset)]
];

// ============================================================
// Part 1: Front Bezel (snap-fit)
// ============================================================

module front_bezel() {
    pcb_standoff_h = 3;

    // Z position of catch flat face (module origin)
    // catch_pos is distance from rim tip to ramp end
    catch_z = bezel_d + snap_rim_h - snap_catch_pos - snap_catch_h;

    difference() {
        union() {
            // --- Main bezel plate with rounded corners ---
            rounded_box(outer_w, outer_h, bezel_d, corner_r);

            // --- Snap-fit rim (hollow perimeter wall) ---
            // Extends from bezel inner face into the shell cavity
            translate([0, 0, bezel_d])
                difference() {
                    linear_extrude(height = snap_rim_h)
                        rounded_rect_2d(snap_rim_outer_w, snap_rim_outer_h, rim_outer_r);
                    translate([0, 0, -0.5])
                        linear_extrude(height = snap_rim_h + 1)
                            rounded_rect_2d(snap_rim_inner_w, snap_rim_inner_h, rim_inner_r);
                }

            // --- Snap catches (4 wedge bumps on rim exterior) ---
            // +Y wall
            translate([0, snap_rim_outer_h/2, catch_z])
                snap_catch_wedge();
            // -Y wall (rotate 180)
            translate([0, -snap_rim_outer_h/2, catch_z])
                rotate([0, 0, 180])
                    snap_catch_wedge();
            // +X wall (rotate -90)
            translate([snap_rim_outer_w/2, 0, catch_z])
                rotate([0, 0, -90])
                    snap_catch_wedge();
            // -X wall (rotate 90)
            translate([-snap_rim_outer_w/2, 0, catch_z])
                rotate([0, 0, 90])
                    snap_catch_wedge();

            // --- PCB mounting standoffs ---
            for (pos = pcb_mount_positions) {
                translate([pos[0], pos[1], bezel_d])
                    cylinder(d = screw_d + 2.5, h = pcb_standoff_h);
            }
        }

        // --- Display cutout (through bezel plate) ---
        translate([0, 0, -0.5])
            linear_extrude(height = bezel_d + 1)
                rounded_rect_2d(display_w, display_h, 1.5);

        // --- PCB recess on back of bezel ---
        // The PCB edge sits in this step; components extend into the cavity
        translate([0, 0, wall])
            linear_extrude(height = bezel_d)
                rounded_rect_2d(wt32_pcb_w + tolerance, wt32_pcb_h + tolerance, 1);

        // --- PCB standoff screw holes ---
        for (pos = pcb_mount_positions) {
            translate([pos[0], pos[1], bezel_d - 0.5])
                cylinder(d = screw_d * 0.8, h = pcb_standoff_h + 1);
        }
    }
}

// ============================================================
// Part 2: Rear Shell (snap-fit)
// ============================================================

module rear_shell() {
    carrier_standoff_h = 5;

    // Groove position in shell Z coordinates
    // Must align with catch position when bezel is fully seated
    groove_z = shell_d - snap_rim_h + snap_catch_pos;
    groove_depth = snap_catch_depth + 0.3;  // Slightly deeper for tolerance
    groove_h = snap_catch_h + 0.5;          // Slightly taller for tolerance
    groove_w = snap_catch_w + 0.5;          // Slightly wider for tolerance

    difference() {
        union() {
            // --- Hollow box shell ---
            difference() {
                rounded_box(outer_w, outer_h, shell_d, corner_r);
                translate([0, 0, wall])
                    rounded_box(inner_w, inner_h, shell_d, max(corner_r - wall/2, 1));
            }

            // --- Carrier board standoffs (plain pegs, no screws) ---
            // Board rests on pegs; panel-mount hardware and case closure hold it
            for (pos = carrier_positions) {
                translate([pos[0], pos[1], wall])
                    cylinder(d = screw_d + 2.5, h = carrier_standoff_h);
            }
        }

        // --- Snap-fit grooves (4 notches in shell inner walls) ---

        // +Y wall groove
        translate([-groove_w/2, inner_h/2 - groove_depth, groove_z])
            cube([groove_w, groove_depth + 0.5, groove_h]);

        // -Y wall groove
        translate([-groove_w/2, -inner_h/2 - 0.5, groove_z])
            cube([groove_w, groove_depth + 0.5, groove_h]);

        // +X wall groove
        translate([inner_w/2 - groove_depth, -groove_w/2, groove_z])
            cube([groove_depth + 0.5, groove_w, groove_h]);

        // -X wall groove
        translate([-inner_w/2 - 0.5, -groove_w/2, groove_z])
            cube([groove_depth + 0.5, groove_w, groove_h]);

        // --- Bottom edge panel-mount holes ---
        // Bottom edge is at Y = -outer_h/2, holes through the wall in Y direction
        bottom_y = -outer_h/2;
        hole_z = wall + 8;  // Hole center height above shell floor

        // USB-C cutout (leftmost — for debug/reflash access)
        usb_x = -inner_w/2 + 12;
        translate([usb_x, bottom_y, hole_z])
            rotate([90, 0, 0])
                linear_extrude(height = wall + 2, center = true)
                    square([usb_c_w, usb_c_h], center = true);

        // 3x Probe jack holes (centered group, 14mm spacing)
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

        // --- Top area ventilation slots (on back face) ---
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

        // --- Kickstand hinge slot (through back wall) ---
        translate([0, -outer_h/2 + ks_h * 0.4 + 10, -0.5])
            linear_extrude(height = wall + 1)
                hull() {
                    translate([-ks_hinge_slot_w/2, 0]) circle(d = ks_hinge_d + 1);
                    translate([ ks_hinge_slot_w/2, 0]) circle(d = ks_hinge_d + 1);
                }
    }
}

// ============================================================
// Part 3: Kickstand
// ============================================================

module kickstand() {
    hinge_r = ks_hinge_d / 2;

    difference() {
        union() {
            // Flat arm body
            translate([-ks_w/2, 0, 0])
                cube([ks_w, ks_h, ks_thick]);

            // Rounded hinge tab at one end
            translate([0, 0, ks_thick/2])
                hull() {
                    translate([-ks_w/2 + hinge_r, 0, 0])
                        rotate([0, 90, 0])
                            cylinder(r = hinge_r, h = ks_w - ks_hinge_d);
                }
        }

        // Snap-fit groove near hinge (lets hinge click into slot)
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
    // "all" — parts arranged side-by-side for visualization
    front_bezel();

    translate([outer_w + 15, 0, 0])
        rear_shell();

    translate([(outer_w + 15) * 2, -outer_h/2 + 10, 0])
        kickstand();
}

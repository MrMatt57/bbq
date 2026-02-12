// ============================================================
// BBQ Fan + Damper Assembly for UDS (Ugly Drum Smoker)
// Parametric design — 2 printable parts
// Open in OpenSCAD, set 'part' variable, render, export STL
// ============================================================

// === PART SELECTOR ===
// Set to "all", "housing", or "adapter" to render
part = "all";

// === Fan (5015 blower) ===
fan_w = 50;             // Fan body width
fan_h = 50;             // Fan body height
fan_d = 15;             // Fan body depth
fan_outlet_d = 20;      // Fan outlet inner diameter
fan_screw_d = 3.0;      // M3 mounting screws
fan_screw_spacing = 42; // Between mounting holes

// === Servo (MG90S) ===
servo_w = 23;           // Servo body width
servo_h = 12.2;         // Servo body height
servo_d = 28.5;         // Servo depth with mounting tabs
servo_tab_w = 32.5;     // Width including mounting tabs
servo_tab_h = 2.5;      // Tab thickness
servo_shaft_offset = 6; // Shaft center offset from one end

// === Damper ===
damper_d = 20;          // Butterfly plate diameter (matches fan inlet)
damper_thickness = 2;   // Plate thickness

// === UDS Adapter ===
npt_od = 26.7;          // 3/4" NPT pipe outer diameter
adapter_id = 27.2;      // Inner diameter with tolerance
adapter_len = 30;       // Length for grip
clamp_groove_d = 2;     // Hose clamp groove depth
clamp_groove_w = 12;    // Hose clamp width

// === Duct ===
duct_od = 28;           // Output duct outer diameter
duct_id = 24;           // Output duct inner diameter
duct_len = 15;          // Duct length

// === Housing ===
housing_wall = 3;       // Housing wall thickness
housing_tol = 0.5;      // Tolerance around fan body

// === Derived Dimensions ===
housing_inner_w = fan_w + housing_tol * 2;
housing_inner_h = fan_h + housing_tol * 2;
housing_inner_d = fan_d + housing_tol * 2;
housing_outer_w = housing_inner_w + housing_wall * 2;
housing_outer_h = housing_inner_h + housing_wall * 2;
housing_outer_d = housing_inner_d + housing_wall * 2;

// Intake opening diameter (where damper sits)
intake_d = damper_d + 2; // Slightly larger than damper plate

$fn = 60;

// ============================================================
// Utility Modules
// ============================================================

// Rounded rectangle 2D
module rounded_rect_2d(w, h, r) {
    offset(r = r)
        square([w - 2*r, h - 2*r], center = true);
}

// ============================================================
// Part 1: Blower Housing
// ============================================================

module blower_housing() {
    // Fan screw mount positions (4 corners of the fan)
    // 5015 blowers typically have screws at diagonal corners
    fan_screw_pos = [
        [-fan_screw_spacing/2, -fan_screw_spacing/2],
        [ fan_screw_spacing/2,  fan_screw_spacing/2]
    ];

    // Servo mount position: centered on intake side
    // Servo mounts on the -X face (intake side), shaft aligned with damper
    servo_mount_y = 0;
    servo_mount_z = housing_outer_d / 2;

    difference() {
        union() {
            // --- Main housing body ---
            // Rectangular box for the fan cavity
            translate([0, 0, housing_outer_d/2])
                cube([housing_outer_w, housing_outer_h, housing_outer_d], center = true);

            // --- Output duct (extends from +Y face) ---
            // Positioned at the fan outlet location
            // 5015 blower outlet is typically at the top-right
            duct_x = 0;
            duct_y = housing_outer_h/2;
            duct_z = housing_outer_d/2;
            translate([duct_x, duct_y, duct_z])
                rotate([-90, 0, 0])
                    difference() {
                        cylinder(d = duct_od, h = duct_len);
                        translate([0, 0, -0.5])
                            cylinder(d = duct_id, h = duct_len + 1);
                    }

            // --- Fan screw mounting bosses ---
            for (pos = fan_screw_pos) {
                translate([pos[0], pos[1], housing_outer_d])
                    cylinder(d = fan_screw_d + 4, h = 3);
            }

            // --- Servo mount bracket on intake side (-X face) ---
            // L-shaped bracket extending from the housing
            servo_bracket_w = servo_tab_w + 4;
            servo_bracket_d = servo_h + housing_wall + 2;

            translate([-housing_outer_w/2 - servo_bracket_d + housing_wall, -servo_bracket_w/2, 0])
                cube([servo_bracket_d, servo_bracket_w, housing_outer_d]);
        }

        // --- Fan cavity ---
        translate([0, 0, housing_wall + housing_outer_d/2])
            cube([housing_inner_w, housing_inner_h, housing_inner_d + 0.1], center = true);

        // --- Intake opening (circular, on -X face) ---
        // This is where outside air enters through the damper
        translate([-housing_outer_w/2 - 0.5, 0, housing_outer_d/2])
            rotate([0, 90, 0])
                cylinder(d = intake_d, h = housing_wall + 1);

        // --- Outlet opening (connects cavity to duct) ---
        translate([0, housing_outer_h/2 - 0.5, housing_outer_d/2])
            rotate([-90, 0, 0])
                cylinder(d = duct_id, h = housing_wall + 1);

        // --- Fan screw holes (through top into cavity) ---
        for (pos = fan_screw_pos) {
            translate([pos[0], pos[1], housing_outer_d - 0.5])
                cylinder(d = fan_screw_d, h = 5);
        }

        // --- Servo pocket (cutout in the bracket) ---
        // The servo body sits in a pocket on the intake side
        servo_pocket_x = -housing_outer_w/2 - servo_h/2;
        translate([servo_pocket_x, 0, housing_outer_d/2])
            cube([servo_h + housing_tol, servo_w + housing_tol, servo_d + housing_tol], center = true);

        // Servo mounting tab slots
        translate([servo_pocket_x, 0, housing_outer_d/2])
            cube([servo_tab_h + 1, servo_tab_w + housing_tol, servo_d/3], center = true);

        // Servo screw holes through the mounting tabs
        for (sy = [-1, 1]) {
            translate([servo_pocket_x, sy * (servo_tab_w/2 - 2), housing_outer_d/2])
                rotate([0, 90, 0])
                    cylinder(d = 2.2, h = servo_h + housing_wall + 4, center = true);
        }

        // --- Damper shaft hole ---
        // Horizontal hole aligned with servo shaft, passing through intake opening
        translate([-housing_outer_w/2 - servo_h - housing_wall - 1, 0, housing_outer_d/2])
            rotate([0, 90, 0])
                cylinder(d = 3, h = housing_outer_w + servo_h + housing_wall + 2);

        // --- Wire channels ---
        // Slot on the top face for routing fan power and servo signal wires
        wire_slot_w = 6;
        wire_slot_d = 4;

        // Fan wire slot (top face, toward back)
        translate([-housing_outer_w/4, -housing_outer_h/2 - 0.5, housing_outer_d - wire_slot_d])
            cube([wire_slot_w, housing_wall + 1, wire_slot_d + 0.5]);

        // Servo wire slot (side, near servo mount)
        translate([-housing_outer_w/2 - servo_h - 1, -wire_slot_w/2, housing_outer_d - wire_slot_d])
            cube([housing_wall + 1, wire_slot_w, wire_slot_d + 0.5]);
    }

    // --- Damper butterfly plate (visualization only, printed separately or on servo horn) ---
    // Shown in position for reference
    %translate([-housing_outer_w/2 + housing_wall/2, 0, housing_outer_d/2])
        rotate([0, 90, 0])
            difference() {
                cylinder(d = damper_d, h = damper_thickness, center = true);
                cylinder(d = 3.2, h = damper_thickness + 1, center = true);
            }
}

// ============================================================
// Part 2: UDS Pipe Adapter
// ============================================================

module uds_pipe_adapter() {
    adapter_od = adapter_id + housing_wall * 2;
    duct_mate_id = duct_od + housing_tol;       // Press-fit over the housing duct
    duct_mate_od = duct_mate_id + housing_wall * 2;
    duct_mate_len = 12;                          // Length of duct mating section
    gasket_groove_w = 3;                         // Gasket groove width
    gasket_groove_d = 1.5;                       // Gasket groove depth
    total_len = adapter_len + duct_mate_len;

    difference() {
        union() {
            // --- Pipe sleeve section ---
            // Cylindrical sleeve that slides over the 3/4" NPT pipe
            cylinder(d = adapter_od, h = adapter_len);

            // --- Duct mating section ---
            // Slightly larger diameter to receive the blower housing duct
            translate([0, 0, adapter_len])
                cylinder(d = duct_mate_od, h = duct_mate_len);

            // --- Transition flange between the two sections ---
            translate([0, 0, adapter_len - 1])
                cylinder(d1 = adapter_od, d2 = duct_mate_od, h = 2);
        }

        // --- Inner bore (pipe side) ---
        translate([0, 0, -0.5])
            cylinder(d = adapter_id, h = adapter_len + 1);

        // --- Inner bore (duct mate side) ---
        translate([0, 0, adapter_len - 0.5])
            cylinder(d = duct_mate_id, h = duct_mate_len + 1);

        // --- Hose clamp groove (pipe side, external) ---
        translate([0, 0, adapter_len / 2 - clamp_groove_w / 2])
            difference() {
                cylinder(d = adapter_od + 1, h = clamp_groove_w);
                translate([0, 0, -0.5])
                    cylinder(d = adapter_od - clamp_groove_d * 2, h = clamp_groove_w + 1);
            }

        // --- Gasket groove (pipe side inner, at the entry) ---
        translate([0, 0, 2])
            difference() {
                cylinder(d = adapter_id + gasket_groove_d * 2, h = gasket_groove_w);
                translate([0, 0, -0.5])
                    cylinder(d = adapter_id - 0.1, h = gasket_groove_w + 1);
            }

        // --- Hose clamp groove (duct mate side, external) ---
        translate([0, 0, adapter_len + duct_mate_len / 2 - clamp_groove_w / 2])
            difference() {
                cylinder(d = duct_mate_od + 1, h = clamp_groove_w);
                translate([0, 0, -0.5])
                    cylinder(d = duct_mate_od - clamp_groove_d * 2, h = clamp_groove_w + 1);
            }
    }
}

// ============================================================
// Assembly View / Part Selector
// ============================================================

if (part == "housing") {
    blower_housing();
}
else if (part == "adapter") {
    uds_pipe_adapter();
}
else {
    // "all" — arranged for visualization
    // Blower housing at origin
    blower_housing();

    // UDS pipe adapter offset to the right, oriented vertically
    translate([housing_outer_w + 30, 0, 0])
        uds_pipe_adapter();
}

# Pit Claw 3D Printed Parts

3D-printable enclosure for the Pit Claw temperature controller and fan/damper assembly for UDS (Ugly Drum Smoker). All designs are parametric OpenSCAD files.

## Files

| File | Description |
|------|-------------|
| `bbq-case.scad` | Controller enclosure (front bezel, rear shell, kickstand) |
| `bbq-fan-assembly.scad` | Fan + damper housing and UDS pipe adapter |
| `stl/` | Pre-exported STL files ready for slicing |

## Print Settings

| Setting | Value |
|---------|-------|
| Material | PETG (80C glass transition -- resists heat and grease) |
| Layer height | 0.2mm |
| Walls | 4 perimeters |
| Infill | 30% |
| Supports | None needed (all parts designed for supportless printing) |

For the UDS pipe adapter, ASA (105C glass transition) is an option if your smoker runs hot near the bottom vent, though PETG is fine for typical 225F cooks where the drum skin near the vent is 100-150F.

## Print Orientations

| Part | Orientation | Notes |
|------|-------------|-------|
| Front bezel | Face-down (display side on bed) | Flat display surface, snap rim prints upward |
| Rear shell | Open-side-up (back wall on bed) | Cavity prints upward, no overhangs |
| Kickstand | Flat | Simple flat piece, no special orientation needed |
| Blower housing | Open-side-up | Fan cavity prints upward |
| UDS pipe adapter | Upright (pipe end down) | Cylindrical, prints cleanly vertical |

## Exporting STLs from OpenSCAD

1. Open the `.scad` file in OpenSCAD
2. Change the `part` variable at the top of the file (e.g., `part = "bezel";`)
3. Press F5 to preview, then F6 to render
4. File > Export > Export as STL
5. Save to the `stl/` directory with the appropriate name

### STL File Names

Controller enclosure (`bbq-case.scad`):
- `stl/front-bezel.stl` (part = "bezel")
- `stl/rear-shell.stl` (part = "shell")
- `stl/kickstand.stl` (part = "kickstand")

Fan assembly (`bbq-fan-assembly.scad`):
- `stl/blower-housing.stl` (part = "housing")
- `stl/uds-pipe-adapter.stl` (part = "adapter")

## Hardware List

### Controller Enclosure

| Qty | Item | Notes |
|-----|------|-------|
| 4 | M3x6mm screws | Mount WT32-SC01 Plus PCB to bezel standoffs |

The front bezel snaps onto the rear shell — no screws needed for case closure. The carrier board rests on standoffs inside the shell and is held in place by panel-mount hardware nuts and case closure.

### Fan Assembly

| Qty | Item | Notes |
|-----|------|-------|
| 2 | M3x10mm screws | Mount 5015 blower fan to housing |
| 2 | M2 or M2.5 screws | Mount MG90S servo (servo-specific) |
| 1 | Small screw + nut | Attach damper plate to servo horn |
| 2 | Small hose clamps (25-40mm range) | One for pipe adapter to pipe nipple, one for duct to adapter (if not press-fit) |
| 1 | High-temp gasket tape (or PTFE tape) | Wrap around NPT pipe before inserting into adapter for air-tight seal |

## Assembly Instructions

### Controller Enclosure

1. **Mount the WT32-SC01 Plus**: Place the display board face-down into the front bezel. The display glass sits against the bezel lip. Secure with 4x M3x6mm screws through the PCB mounting holes into the bezel standoffs.

2. **Mount the carrier board**: Place the carrier perfboard onto the standoffs inside the rear shell. The board rests on the pegs and is held by panel-mount hardware.

3. **Install panel-mount connectors**: Thread the 3x 2.5mm mono probe jacks and 1x DC barrel jack through their respective holes in the rear shell bottom edge. Secure with their mounting nuts from inside.

4. **Connect wiring**: Connect the 8-pin ribbon cable from the carrier board to the WT32-SC01 Plus extension connector. Route fan and servo wires through the right-side cable slots.

5. **Close the enclosure**: Align the bezel's snap-fit rim with the rear shell opening and press firmly until the catches click into place. No screws needed.

6. **Attach kickstand** (optional): Press the kickstand hinge tab into the slot on the rear face of the shell. The kickstand should click in and swing freely.

### Fan + Damper Assembly

1. **Mount the servo**: Press-fit or screw the MG90S servo into the servo pocket on the intake side of the blower housing.

2. **Attach damper plate**: Attach the butterfly damper plate to a servo horn using a small screw. The damper plate should be centered in the intake opening when the servo is at its midpoint.

3. **Install the servo horn**: Press the horn with damper plate onto the servo shaft. Verify the damper opens fully (parallel to airflow) and closes fully (perpendicular to airflow) across the servo's range of motion.

4. **Mount the blower fan**: Place the 5015 blower into the housing cavity. Secure with 2x M3x10mm screws through the top mounting holes.

5. **Prepare the UDS pipe adapter**: Wrap high-temp gasket tape around the 3/4" NPT pipe nipple on the drum. Slide the pipe adapter over the nipple and secure with a hose clamp in the groove.

6. **Connect housing to adapter**: Insert the blower housing output duct into the adapter's duct mating end. If the fit is loose, secure with a second hose clamp. If press-fit is snug, no clamp is needed.

7. **Route wires**: Run the fan power wires (2-pin) and servo signal wires (3-pin) through the wire channels on the housing, back to the controller enclosure.

### Airflow Path

```
Outside air -> [butterfly damper] -> [5015 blower fan] -> [duct] -> [UDS pipe adapter] -> drum
               (servo controlled)    (PWM speed)          (sealed)   (clamped to pipe)
```

The PID controller modulates both the fan speed (PWM) and damper position (servo angle) to maintain the target pit temperature. Cap all other intake pipes on the UDS -- this assembly controls all airflow.

## Future Adapters

The blower housing output duct is a standard round interface (28mm OD / 24mm ID). Adapters for other smoker types can be designed to mate with this same duct:

- Weber Smokey Mountain (WSM) -- adapter plate for the bottom vent
- Kamado-style (Big Green Egg, Kamado Joe) -- adapter for the bottom draft door
- Offset smokers -- adapter plate for the firebox intake

Community contributions welcome. Design your adapter to receive the 28mm OD duct (either press-fit or with a hose clamp groove) on one end, and interface with your smoker's air intake on the other.

## Parametric Customization

Both `.scad` files have all key dimensions as variables at the top. Common reasons to customize:

- **Different fan size**: Update `fan_w`, `fan_h`, `fan_d`, and `fan_screw_spacing`
- **Different servo**: Update `servo_w`, `servo_h`, `servo_d`, `servo_tab_w`
- **Thicker walls**: Increase `wall` (case) or `housing_wall` (fan assembly)
- **Tighter/looser fit**: Adjust `tolerance` or `housing_tol`
- **Different pipe size**: Update `npt_od` and `adapter_id` for your intake pipe

Open the `.scad` file in OpenSCAD, modify the variables, press F5 to preview, and F6 to render.

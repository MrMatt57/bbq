# 009: Enclosure Design

**Branch**: `feature/mvp-scaffold`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

OpenSCAD parametric 3D-printable enclosure consisting of two separate designs: a controller case for the WT32-SC01 Plus display and carrier board, and a fan+damper assembly for mounting to an Ugly Drum Smoker (UDS). Both designs are fully parametric with configurable dimensions, tolerances, and mounting features. Designed for supportless FDM printing in PETG.

## Requirements

### Controller Case (bbq-case.scad)
- Three printable parts: front bezel, rear shell, kickstand
- Front bezel holds WT32-SC01 Plus with display cutout (50×74mm active area) and bezel lip
- 4× M3 screw bosses joining front to rear (heat-set inserts or self-tapping)
- WT32-SC01 Plus mounted on 4× M3 standoffs in front bezel
- Carrier board (50×70mm perfboard) on standoffs in rear shell
- Panel-mount holes on bottom edge: USB-C cutout, 3× 2.5mm probe jacks, DC barrel jack
- Cable slots on right side: fan and servo wire pass-throughs
- Ventilation slots on top (5 per side)
- Detachable kickstand with hinge slot (~30° viewing angle)
- Rounded corners (4mm radius), 2.5mm wall thickness
- All dimensions parametric (variables at top of file)
- No supports needed (designed for supportless printing)

### Fan + Damper Assembly (bbq-fan-assembly.scad)
- Two printable parts: blower housing, UDS pipe adapter
- Blower housing: cavity for 5015 fan (50×50×15mm), servo mount pocket, butterfly damper at intake, output duct (28mm OD / 24mm ID)
- Fan screw mounting bosses (2× M3), wire channels
- Damper shaft hole (3mm) aligned with servo arm
- UDS pipe adapter: cylindrical sleeve for 3/4" NPT pipe nipple (27.2mm ID with tolerance)
- Hose clamp grooves (top and bottom), gasket groove for high-temp gasket tape
- Duct mating section for press-fit to blower housing
- Airflow path: outside air → butterfly damper → 5015 fan → duct → pipe adapter → drum
- Standard duct interface for community adapter designs (Weber WSM, kamado, offset)

### Print Settings
- Material: PETG (80°C glass transition, safe for near-smoker use)
- Layer height: 0.2mm, 4 perimeters, 30% infill
- Front bezel: print face-down; rear shell: print open-side-up; adapter: print upright

## Design

### Parametric Variables (bbq-case.scad)

```scad
// Board dimensions
wt32_pcb_w = 60;     wt32_pcb_h = 92;     wt32_pcb_d = 10.8;
display_w  = 50;     display_h  = 74;
carrier_w  = 50;     carrier_h  = 70;     carrier_d  = 15;

// Enclosure
wall       = 2.5;    corner_r   = 4;      tolerance  = 0.5;
screw_d    = 3.0;    // M3

// Panel mount holes
probe_jack_d  = 6.2;    barrel_jack_d = 12.2;
usb_c_w       = 10;     usb_c_h       = 4;
```

### Parametric Variables (bbq-fan-assembly.scad)

```scad
fan_w = 50;  fan_h = 50;  fan_d = 15;  fan_outlet_d = 20;
servo_w = 23;  servo_h = 12.2;  servo_d = 28.5;
npt_od = 26.7;  adapter_id = 27.2;  adapter_len = 30;
clamp_groove = 2;
```

### Part Selection

Both .scad files use a `part` variable to select which component to render/export. Change the variable, render (F6), and export STL.

### Assembly

- Controller: Heat-set inserts → mount PCB → mount carrier → install connectors → close case → attach kickstand
- Fan assembly: Fan screws into housing → servo press-fits → damper plate on servo horn → housing mates to adapter → adapter clamps to pipe nipple

## Files to Modify

| File | Change |
|------|--------|
| `enclosure/bbq-case.scad` | Parametric controller enclosure (front bezel, rear shell, kickstand) |
| `enclosure/bbq-fan-assembly.scad` | Parametric fan + damper assembly (blower housing, UDS pipe adapter) |
| `enclosure/README.md` | Print settings, hardware list, assembly instructions, STL export workflow |
| `enclosure/stl/.gitkeep` | Directory for exported STL files |

## Test Plan

- [x] bbq-case.scad renders all three parts without errors in OpenSCAD
- [x] bbq-fan-assembly.scad renders both parts without errors in OpenSCAD
- [x] All dimensions are parametric (configurable via variables)
- [x] README documents print settings, hardware, and assembly steps

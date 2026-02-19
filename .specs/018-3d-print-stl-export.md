# 018: 3D Print STL Export & Docs Cleanup

**Branch**: `feature/3d-print-stl-export`
**Created**: 2026-02-18

## Summary

Generate first-draft STL files from the existing OpenSCAD parametric designs, add automated STL generation to the release CI workflow, and clean up the README structure so the main README links out to the enclosure docs page (renamed to "3D Printed Parts") instead of duplicating all the detail inline.

## Requirements

- Generate all 5 STL files from the two OpenSCAD source files and commit them to `enclosure/stl/`
- Add an OpenSCAD step to `.github/workflows/release.yml` that generates STLs from `.scad` source during releases (so committed STLs stay in sync with source)
- Simplify the main `README.md` "3D Printed Parts" section to a brief summary (2-3 sentences about the two assemblies) + bullet list of 5 parts + link to `enclosure/README.md`
- Rename the dev guide table entry from "Enclosure" to "3D Printed Parts" and update its topics column
- Update `enclosure/README.md` heading from "Pit Claw Enclosure & Fan Assembly" to "Pit Claw 3D Printed Parts"

## Design

### STL Generation (Local)

Run OpenSCAD CLI to export each part:

```bash
cd enclosure
openscad -o stl/front-bezel.stl -D 'part="bezel"' bbq-case.scad
openscad -o stl/rear-shell.stl -D 'part="shell"' bbq-case.scad
openscad -o stl/kickstand.stl -D 'part="kickstand"' bbq-case.scad
openscad -o stl/blower-housing.stl -D 'part="housing"' bbq-fan-assembly.scad
openscad -o stl/uds-pipe-adapter.stl -D 'part="adapter"' bbq-fan-assembly.scad
```

Remove the `.gitkeep` from `stl/` since it will now have real files.

### STL Generation (CI — Release Workflow)

Add a step to `.github/workflows/release.yml` before "Collect artifacts" that:
1. Installs OpenSCAD (`sudo apt-get install -y openscad`)
2. Generates all 5 STLs from source using the same CLI commands
3. The existing "Collect artifacts" step already copies `enclosure/stl/*.stl` into `release/`

This ensures release artifacts always match the `.scad` source, even if someone forgets to re-export locally.

### README Changes

**`README.md`** — Replace the current ~25-line "3D Printed Parts" section (lines 66-91) with:

```markdown
### 3D Printed Parts

PETG-printed enclosure and fan assembly. All designs are parametric [OpenSCAD](https://openscad.org/) source files with pre-exported STLs ready for slicing.

- **Front bezel** — holds the display
- **Rear shell** — houses carrier board and panel-mount connectors
- **Kickstand** — detachable flip-out stand
- **Blower housing** — holds fan, servo, and butterfly damper
- **UDS pipe adapter** — mounts to 3/4" NPT pipe nipple on drum

See [3D Printed Parts](enclosure/README.md) for print settings, hardware list, and assembly instructions.
```

**Dev guide table** (line ~160) — Change:
- "Enclosure" → "3D Printed Parts"
- Topics: "3D printing, assembly instructions, parametric customization" (already similar, just verify)

**`enclosure/README.md`** — Change line 1 heading:
- From: `# Pit Claw Enclosure & Fan Assembly`
- To: `# Pit Claw 3D Printed Parts`

## Files to Modify

| File | Change |
|------|--------|
| `enclosure/stl/front-bezel.stl` | New — generated from bbq-case.scad |
| `enclosure/stl/rear-shell.stl` | New — generated from bbq-case.scad |
| `enclosure/stl/kickstand.stl` | New — generated from bbq-case.scad |
| `enclosure/stl/blower-housing.stl` | New — generated from bbq-fan-assembly.scad |
| `enclosure/stl/uds-pipe-adapter.stl` | New — generated from bbq-fan-assembly.scad |
| `enclosure/stl/.gitkeep` | Delete (replaced by real STL files) |
| `.github/workflows/release.yml` | Add OpenSCAD install + STL generation step |
| `README.md` | Simplify 3D Printed Parts section, rename dev guide entry |
| `enclosure/README.md` | Update heading to "3D Printed Parts" |

## Test Plan

- [x] All 5 STL files exist in `enclosure/stl/` and are non-empty
- [x] OpenSCAD renders each part without errors (exit code 0)
- [x] `README.md` 3D Printed Parts section is concise with link to enclosure/README.md
- [x] Dev guide table shows "3D Printed Parts" instead of "Enclosure"
- [x] `enclosure/README.md` heading updated
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)
- [x] Desktop tests pass (`pio test -e native`)

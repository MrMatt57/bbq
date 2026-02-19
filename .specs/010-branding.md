# 010: Branding — Pit Claw

**Branch**: `feature/rebrand-pit-claw`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

Rebranded the project from "BBQ Controller" to "Pit Claw" with a lobster claw logo featuring smoke wisps. Updated all user-facing references across the web UI, touchscreen UI, README, setup wizard, simulator, and enclosure files. Added a CI status badge to the README.

## Requirements

- Project name: "Pit Claw" (from "BBQ Controller")
- Logo: Red cartoon lobster claw with amber ring and smoke wisps rising above (SVG)
- Favicon: Lobster claw SVG used as browser tab icon and PWA icon
- Update all user-facing name references:
  - Web UI header banner
  - PWA manifest (name, short_name)
  - Touchscreen setup wizard welcome screen
  - README title and description
  - Simulator package.json
  - Enclosure README
  - OpenSCAD file headers
- CI status badge in README linking to GitHub Actions workflow
- GitHub repository name: `pitclaw`

## Design

### Logo (favicon.svg)

Red cartoon lobster on dark background with amber ring accent. Smoke wisps rising above represent the BBQ/smoking theme. Simple SVG with stroke-based linework, ~40×40px viewBox. Used as:
- Browser favicon (`<link rel="icon">`)
- PWA home screen icon (via manifest.json)
- Web UI header banner image

### Name Changes

All references to "BBQ Controller", "BBQ", "bbq-controller" updated to "Pit Claw" / "pitclaw" across:
- `firmware/data/index.html` (page title, header)
- `firmware/data/manifest.json` (name, short_name)
- `firmware/src/display/ui_setup_wizard.cpp` (welcome text)
- `firmware/src/main.cpp` (serial boot message)
- `README.md` (title, badge URLs, description)
- `simulator/package.json` (name field)
- `enclosure/README.md` (references)
- `enclosure/bbq-case.scad` (header comment)
- `enclosure/bbq-fan-assembly.scad` (header comment)
- `scripts/setup-dev.ps1` (references)

## Files to Modify

| File | Change |
|------|--------|
| `firmware/data/favicon.svg` | Lobster claw logo with smoke wisps |
| `firmware/data/index.html` | Update title, header banner with logo |
| `firmware/data/manifest.json` | PWA name "Pit Claw", icon reference |
| `firmware/src/display/ui_setup_wizard.cpp` | Welcome screen text |
| `firmware/src/main.cpp` | Serial boot message |
| `README.md` | Project title, CI badge, description, repo URLs |
| `simulator/package.json` | Package name |
| `enclosure/README.md` | Project references |
| `enclosure/bbq-case.scad` | Header comment |
| `enclosure/bbq-fan-assembly.scad` | Header comment |
| `scripts/setup-dev.ps1` | Project references |

## Test Plan

- [x] Web UI displays "Pit Claw" in header and page title
- [x] Favicon renders in browser tab
- [x] PWA manifest uses "Pit Claw" name
- [x] Setup wizard shows "Pit Claw" on welcome screen
- [x] README displays CI status badge
- [x] All user-facing references updated (no "BBQ Controller" remaining)
- [x] Firmware builds (`pio run -e wt32_sc01_plus`)

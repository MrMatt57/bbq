# 012: Restructure README and Documentation

**Branch**: `feature/readme-docs-restructure`
**Created**: 2026-02-18

## Summary

Restructure the README to focus on what new visitors need (what it is, hardware, quick start) and move developer-focused content into dedicated docs. The goal is a concise onboarding experience for people who want to build this project, with an index linking to deeper development guides.

## Requirements

- README should prominently show: what the project is, hardware requirements/BOM, and a quick getting started guide
- Move wiring diagram out of README into `docs/wiring.md`, linked from the hardware section
- Create `docs/firmware-development.md` covering firmware building, testing, architecture, and source layout
- Create `docs/web-development.md` covering simulator usage, cook profiles, web UI development, and WebSocket protocol
- README should have a "Development" section that serves as an index linking to the doc files
- Keep the README hardware section (BOM, probes, 3D printed parts) intact — builders need it
- Preserve all existing content; nothing should be lost, just reorganized
- All internal links should work correctly

## Design

### README.md (streamlined)

Keep these sections in README:
1. **Header** — logo, badge, one-paragraph description, screenshot
2. **Features** — bullet list (as-is)
3. **Hardware** — BOM table, probes, 3D printed parts. Add a link to `docs/wiring.md` for wiring details
4. **Quick Start** — condensed: clone, setup script, build & flash, first boot steps, accessing web UI, factory reset, OTA mention
5. **Development** — brief index paragraph linking to:
   - `docs/firmware-development.md` — building, testing, architecture
   - `docs/web-development.md` — web UI, simulator, WebSocket protocol
   - `docs/wiring.md` — wiring diagram and pin assignments
   - `enclosure/README.md` — 3D printing and assembly
6. **License** — TBD

Remove from README (moved to docs):
- Wiring diagram → `docs/wiring.md`
- Simulator section → `docs/web-development.md`
- Running Tests section → `docs/firmware-development.md`
- Web UI detailed description → `docs/web-development.md`

### docs/wiring.md

- Carrier board wiring diagram (the ASCII art from current README)
- Pin assignment table
- Probe voltage divider schematic

### docs/firmware-development.md

- Prerequisites (manual install list — the non-Windows path)
- Quick setup script mention
- Build & flash commands (firmware targets)
- Running tests (native + embedded)
- OTA update details
- Architecture overview: source layout tree, key modules described
- Configuration persistence (config.json structure)
- Error detection table
- Key technical constants (Steinhart-Hart, PID defaults, fan control)

### docs/web-development.md

- Simulator setup and usage (build, run, options)
- Cook profiles table
- Web UI features overview
- PWA details (manifest, service worker)
- WebSocket protocol documentation (all message types)
- Key constraint: web UI must work identically on simulator and ESP32
- Editing workflow (edit files in firmware/data/, refresh browser)

## Files to Modify

| File | Change |
|------|--------|
| `README.md` | Restructure: keep intro/features/hardware/quick-start, add dev index, remove moved sections |
| `docs/wiring.md` | **New**: wiring diagram, pin assignments from README |
| `docs/firmware-development.md` | **New**: firmware dev guide with build/test/architecture content |
| `docs/web-development.md` | **New**: web UI + simulator dev guide |

## Test Plan

- [x] All links in README resolve correctly (relative paths to docs/)
- [x] No content lost — all information from old README exists somewhere in new structure
- [x] README is concise and focuses on builder onboarding
- [x] docs/wiring.md contains the full wiring diagram
- [x] docs/firmware-development.md covers building, testing, and architecture
- [x] docs/web-development.md covers simulator, web UI, and WebSocket protocol
- [ ] Firmware builds (`pio run -e wt32_sc01_plus`) — docs-only change, no source code modified
- [ ] Simulator builds (`pio run -e simulator`) — docs-only change, no source code modified

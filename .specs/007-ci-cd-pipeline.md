# 007: CI/CD Pipeline

**Branch**: `feature/releases`
**Created**: 2026-02-18
**Status**: Implemented

## Summary

GitHub Actions workflows for continuous integration and automated releases. CI runs on every push and PR: builds firmware for the ESP32-S3 target, runs desktop unit tests, and checks simulator dependencies. The release workflow triggers on pushes to main, builds firmware and LittleFS artifacts, collects enclosure STLs, generates SHA-256 checksums, and creates a GitHub Release with CalVer versioning.

## Requirements

### CI Workflow (ci.yml)
- Trigger on push to any branch and pull requests to main
- Build firmware for `wt32_sc01_plus` environment
- Run desktop unit tests (`pio test -e native`)
- Check simulator Node.js dependencies (`npm ci`)
- Cache PlatformIO packages for faster builds
- Report build and test status

### Release Workflow (release.yml)
- Trigger on push to main branch only
- CalVer versioning: `v{YEAR}.{MONTH}.{PATCH}` (e.g., v2026.2.4)
- Patch number auto-incremented from existing tags in the same month
- Build firmware binary (`.bin`)
- Build LittleFS image for web UI assets
- Collect STL files from `enclosure/stl/` directory
- Generate SHA-256 checksums for all artifacts
- Create GitHub Release with auto-generated notes from PR titles
- Firmware version constant in `config.h` used for display (separate from CalVer release tag)

## Design

### CI Pipeline

```yaml
jobs:
  firmware:
    - Install PlatformIO
    - Cache ~/.platformio
    - Build: pio run -e wt32_sc01_plus
    - Test: pio test -e native

  simulator:
    - Install Node.js
    - npm ci (check package.json)
```

### Release Pipeline

```yaml
jobs:
  release:
    - Checkout code
    - Install PlatformIO
    - Determine CalVer tag (current year.month.patch)
    - Build firmware: pio run -e wt32_sc01_plus
    - Build LittleFS: pio run -e wt32_sc01_plus --target buildfs
    - Collect artifacts (firmware.bin, littlefs.bin, STLs)
    - Generate checksums (sha256sum)
    - Create GitHub Release with tag and artifacts
```

### CalVer Versioning

Format: `v{YYYY}.{M}.{PATCH}` where:
- `YYYY` = current year (e.g., 2026)
- `M` = current month (1-12, no leading zero)
- `PATCH` = incremented from last tag in same month (starts at 1)

Example sequence: v2026.2.1, v2026.2.2, v2026.2.3, v2026.3.1 (new month resets patch)

## Files to Modify

| File | Change |
|------|--------|
| `.github/workflows/ci.yml` | CI workflow: build, test, simulator check |
| `.github/workflows/release.yml` | Release workflow: CalVer tag, build artifacts, create release |
| `firmware/src/config.h` | `FIRMWARE_VERSION` constant for display |

## Test Plan

- [x] CI workflow triggers on push and PR
- [x] Firmware build succeeds in CI
- [x] Desktop tests pass in CI (`pio test -e native`)
- [x] Release workflow creates tagged GitHub Release on main push
- [x] Release includes firmware.bin and littlefs.bin artifacts
- [x] CalVer tag format is correct (v{YEAR}.{MONTH}.{PATCH})

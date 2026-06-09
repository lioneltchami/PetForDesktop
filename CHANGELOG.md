# Changelog

## [Unreleased]

### [Template] Upcoming release
- Version: `vX.Y.Z`
- Date: `YYYY-MM-DD`
- Compatibility impact:
  - ...
- Summary:
  - ...
- Added
  - ...
- Changed
  - ...
- Fixed
  - ...
- Security
  - ...
- Notes
  - ...
- Validation checklist:
  - [ ] ...

### Added
- Added a strict before→now gameplay/runtime review matrix in release notes for the major hardening pass (resolution-aware cursor handling, world sampling-backed physics, update/settings security hardening).

### Changed
- Window/input handling and cursor mapping now normalize through monitor-aware transforms to improve multi-monitor and mixed-DPI behavior.
- Physics collision handling now uses topology snapshots/world sampling and supports location-aware motion scaling.
- Settings parsing now uses schema validation, clamping, warnings/errors, and atomic export writes.
- Update logic moved to a structured metadata-driven pipeline with explicit validation, verification hooks, and safer staging semantics.

### Fixed
- Fixed `WindowGLFW` logical size setter mistake in resize path.
- Improved cleanup/destruction and monitor callback integration in window wrapper.
- Reduced edge-condition instability from mixed-scale monitor boundaries by normalizing coordinate handling and collision/topology checks.

### Notes
- For baseline-to-current behavior diff, see [RELEASE_NOTES.md](/Users/lioneltchami/build/PetForDesktop-main/RELEASE_NOTES.md).

## [v2026.06.08] - 2026-06-08

### Added
- Added strict runtime hardening changes for monitor normalization, physics/world-sampling integration, and updater/settings validation pipeline.

### Changed
- Enabled monitor-aware cursor normalization for all pointer paths in [src/WindowGLFW.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/WindowGLFW.cpp).
- Updated physics collision and boundary logic to consume monitor topology snapshots and per-position scaling in [include/Engine/PhysicSystem.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/PhysicSystem.hpp).
- Refactored pause/release behavior to shared pet logic helpers in [src/Pet.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Pet.cpp) and introduced safer release velocity scaling.
- Replaced simple update check with full metadata pipeline in [src/Updater.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Updater.cpp) and expanded public interface in [include/Engine/Updater.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/Updater.hpp).
- Reworked settings import/export into schema-based validate/sanitize/clamp with bounded I/O and atomic swap in [src/Settings.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Settings.cpp).

### Fixed
- Fixed `WindowGLFW` size setter bug and made destructor safer in [include/Engine/WindowGLFW.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/WindowGLFW.hpp).

### Security and platform robustness
- Added update package/manifest validation, host allowlisting, checksum/signature envelope checks, and staging behavior in [src/Updater.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Updater.cpp).
- Added update metadata model in [include/Engine/UpdateMetadata.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/UpdateMetadata.hpp).
- Added structured settings validation reporting in [include/Engine/Settings.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/Settings.hpp).

### Compatibility notes
- Update policy is now stricter and may reject previously accepted malformed/unsupported update payloads.
- Physics and boundary behavior may change slightly at monitor seams in mixed-DPI layouts; intended effect is higher determinism and reduced edge drift.

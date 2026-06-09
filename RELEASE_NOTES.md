# Release Notes

## [Template] Upcoming release - TBD

### Scope
- Files:
  - ...
- Why this change set:
  - ...

### High-level outcome
- ...

### Before→now matrix
| File | Before | Now | Behavior impact |
| --- | --- | --- | --- |
| ... | ... | ... | ... |

### Security and robustness notes
- ...

### Recommended validation checklist
1. ...

## [v2026.06.08] - 2026-06-08

### Scope
Strict before→now behavior diff for the reviewed gameplay/runtime hardening files:
- [src/WindowGLFW.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/WindowGLFW.cpp)
- [include/Engine/PhysicSystem.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/PhysicSystem.hpp)
- [src/Pet.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Pet.cpp)
- [src/Updater.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Updater.cpp)
- [src/Settings.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Settings.cpp)

### Why this change set
This pass focused on platform hardening without changing visible gameplay goals:
- walking/jumping/flying behavior
- carrying and surface following
- pause/update/menu usability

### High-level outcome
- Input and window-space coordinates became monitor-aware and resilient to mixed DPI/layouts.
- Physics collisions became topology-aware with optional world-sampling-backed surface checks.
- Settings parsing moved from loose assignment to schema validation with safe defaults and atomic writes.
- Updater moved from inline regex flow to a metadata envelope workflow with trust and integrity checks.

### Before→now matrix

| File | Before | Now | Behavior impact |
| --- | --- | --- | --- |
| [src/WindowGLFW.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/WindowGLFW.cpp) | Raw GLFW coordinates for drag/cursor position and no monitor hotplug callback bridge. | Added monitor transform normalization for cursor position/deltas and monitor hotplug callback registration/dispatch. | Improves coordinate correctness on mixed DPI/multi-monitor topologies and reduces off-by-scale jump/edge drift. |
| [include/Engine/WindowGLFW.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/WindowGLFW.hpp) | Size setter used wrong variable in resize path; destruction only terminated GLFW. | Fixed size setter (`windowSize`), added monitor callback registration API and safer context/window cleanup before destroy. | Prevents incorrect size mutation and improves runtime/exit stability. |
| [include/Engine/PhysicSystem.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/PhysicSystem.hpp) | Integer monitor rect checks and always-available texture-edge collision pipeline. | Uses monitor topology snapshot + world sampling paths, introduces epsilon-tolerant geometry checks, and refines edge overlap logic (`<= 1`). | More stable monitor-boundary behavior and reduced collision inconsistencies across DPI/monitor seams; physics now uses location-aware scaling. |
| [src/Pet.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Pet.cpp) | Inline pause and release logic using global constants. | Pause/release delegated to helper logic; release velocity now uses local `pixelPerMeter` from world sampling with validation fallback. | Gameplay intent preserved, with improved trajectory consistency across monitor/surface scaling. |
| [src/Updater.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Updater.cpp) | Minimal version check in header using regex + direct update menu creation. | New structured updater flow: fetch metadata, parse manifest/assets, validate envelope/signature/host policy, stage + verify downloaded payload, and explicit error reporting APIs. | Hardens update trust model; stricter rejection of malformed/untrusted payloads. |
| [src/Settings.cpp](/Users/lioneltchami/build/PetForDesktop-main/src/Settings.cpp) | Direct assignment of YAML values with basic checks. | Full schema validation report system, section/key whitelisting, bounded reads, clamp/default fallback, and atomic write with temp/rename. | Prevents invalid settings from destabilizing runtime and provides actionable warnings/errors. |
| [include/Engine/Updater.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/Updater.hpp) | Single `checkForUpdate(...)` API in header. | Multi-step testable updater methods (`fetch/resolve/validate/verify/apply`) and test hooks. | Better testability and separation of concerns. |
| [include/Engine/Settings.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/Settings.hpp) | Basic `import/export` API only. | Added `ValidationReport` and validation/sanitize/clamp methods. | Structured settings diagnostics and runtime-safe value constraints. |
| [include/Engine/UpdateMetadata.hpp](/Users/lioneltchami/build/PetForDesktop-main/include/Engine/UpdateMetadata.hpp) | No dedicated metadata model. | Added metadata/value structures for updates and assets. | Enables robust parsing and validation pipeline. |

### Security and robustness notes
- Update transport and metadata are now policy-constrained (HTTPS, trusted host allowlist, max-size limits, checksum/signature envelope checks).
- Settings file handling includes stricter parsing boundaries and atomic replacement semantics.

### Recommended validation checklist
1. Multi-monitor mixed DPI run (e.g., 100% + 150%) and surface boundary checks.
2. Corrupt/edge-case settings inputs (missing keys, NaN, out-of-range, unknown sections).
3. Update metadata failure-path testing: invalid URL/host, checksum mismatch, invalid signature envelope.
4. Window callback lifecycle verification during close/startup and monitor connect/disconnect events.

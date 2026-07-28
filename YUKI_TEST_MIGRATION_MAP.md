# YUKI Test Migration Map

## Current State
Tests are split between:
- `tests/` (active)
- `not_in_use/test_files/` (mixed active + legacy)

## Safe Staged Plan

### Phase 1 — Audit & Tag (no file moves)
For each file in `not_in_use/test_files/`:
1. Run the test. If it passes → tag `ACTIVE`.
2. If it fails but tests known legacy behavior → tag `LEGACY`.
3. If it fails and tests deleted functionality → tag `OBSOLETE`.
4. If it spans multiple modules → tag `INTEGRATION`.

### Phase 2 — CMake Target Update Only
Update `CMakeLists.txt` test targets to reference files by absolute path.
Do NOT move files yet. This validates that path changes work.

### Phase 3 — Physical Migration (after one build cycle)
Once Phase 2 builds green:

| Current Path | Target Path | Classification |
|---|---|---|
| `not_in_use/test_files/test_unit_*.cpp` | `tests/unit/` | Active unit tests |
| `not_in_use/test_files/test_integ_*.cpp` | `tests/integration/` | Active integration tests |
| `not_in_use/test_files/test_legacy_*.cpp` | `tests/legacy/` | Legacy regression tests |
| `not_in_use/test_files/test_obsolete_*.cpp` | `tests/obsolete/` | Obsolete tests (retain for audit trail) |

### Phase 4 — Cleanup
After 1 week of green CI, delete `not_in_use/test_files/` directory.

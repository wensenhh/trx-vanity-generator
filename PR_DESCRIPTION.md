## What
CMake `project()` VERSION field only accepts numeric versions (X.Y.Z). The current `VERSION 1.0.0-beta` is invalid and causes `cmake --build --target package` to fail.

## Why
Test #23 (`package_tgz_gpu_runtime_smoke`) fails because CPack cannot parse the non-numeric version string, breaking the TGZ package generation step.

## Changes
- Split version into `VERSION 1.0.0` (CMake-compatible) and `PROJECT_VERSION_SUFFIX "-beta"`.
- Introduce `PROJECT_VERSION_FULL` = `${PROJECT_VERSION}${PROJECT_VERSION_SUFFIX}`.
- Update all CPack outputs to use `PROJECT_VERSION_FULL`:
  - `CPACK_PACKAGE_VERSION`
  - `CPACK_DMG_VOLUME_NAME` (macOS)
  - `CPACK_NSIS_DISPLAY_NAME` (Windows)

## Test
- Full ctest suite (25 tests) passes locally, including Test #23.
- Verified `cmake --build build_test --target package` succeeds and produces `trx_vanity-1.0.0-beta-Darwin-arm64.tar.gz`.

## Risk
Low. Only affects packaging metadata; no runtime or API changes.

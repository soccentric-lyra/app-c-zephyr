# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to
[Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- Kconfig options for the application log level and the IWDG timeout
- `ztest` suite for the HWLINK framing layer on Zephyr's `native_sim` board
- `make test` and `scripts/test-ztest.sh`
- `docs/architecture.md`: thread model, devicetree overlay, flash map,
  boot hand-over and a side-by-side comparison with the FreeRTOS version
- GitHub Actions workflow for build, strict lint, footprint and tests

### Changed

- The Zephyr image now includes a 32-bit host toolchain so `native_sim`
  tests run in the same container as the firmware build

### Fixed

- The README described `app-rust-freertos` as "Rust on Embassy"; it is Rust
  on FreeRTOS

## [1.0.0] - 2026-11-19

### Added

- NUCLEO-F767ZI demo application in C on Zephyr RTOS: LED patterns, the
  user button, HWLINK over UART, watchdog supervision and self-confirmation
- Freestanding CMake build, so no west workspace is required
- Behaviour matches the C FreeRTOS demo for a like-for-like comparison
- Runs from either A/B slot under the bootloader

[Unreleased]: https://github.com/soccentric-stm32/app-c-zephyr/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/soccentric-stm32/app-c-zephyr/releases/tag/v1.0.0

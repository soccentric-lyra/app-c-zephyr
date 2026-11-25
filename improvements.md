# Planned improvements: app-c-zephyr

Gaps found in review and the enhancements planned to close them, highest priority first. Priority 1 is next, 3 is later.

## Gaps

| Area | Gap |
|---|---|
| Tests | No tests. Zephyr ships `ztest`, and none of it is used. |
| CI | No GitHub Actions workflow and no public build status. |
| Docs | `docs/README.md` exists, but there is no architecture page and no explanation of why Zephyr choices (threads, devicetree overlay, `k_` primitives) differ from FreeRTOS. |
| README | Its "Related projects" list describes `app-rust-freertos` as "Rust on Embassy". It should say Rust on FreeRTOS. |
| Duplication | `hwlink.c` and the flash-confirm code are separate copies of the FreeRTOS version, so they can drift. |
| Changelog | Only `[Unreleased]`, while `VERSION` says 0.1.0. |
| Validation | No emulated run. Zephyr supports QEMU and Renode targets for Cortex-M. |

## Enhancements

| Priority | Enhancement | Result |
|---|---|---|
| 1 | `ztest` suite for HWLINK framing, CRC and the pattern logic. Run it on `native_sim`, which needs no hardware. | Tests that run in public CI |
| 1 | GitHub Actions workflow: build, lint, footprint, `native_sim` tests, with a badge | Visible green status |
| 2 | Architecture page: thread and priority map, devicetree overlay, how slot A and B are linked, hand-over record | Shows Zephyr depth |
| 2 | Side-by-side section with the FreeRTOS version: footprint, RAM, boot time, code size per feature | Makes the comparison concrete |
| 2 | Use Kconfig options for features (log level, fail-health, watchdog timeout) and document them | Shows idiomatic Zephyr |
| 3 | Run the image in Renode and check the boot log | Hardware-free validation |
| 3 | Share the HWLINK test vectors with the other four repos | Proves behaviour parity |
| 3 | Tag releases and write release notes | Maintained look |


# Repository Layout

```text
host/ImeDosBridge/  .NET Windows bridge and runtime key configuration
dos/common/         shared packet framing and CRC16
dos/include/        shared protocol constants and declarations
dos/pc98/           PC-98 foreground, TSR, driver, and probe implementations
tools/              protocol tests and build/diagnostic helpers
scripts/            local emulator launch helpers
samples/            guest AUTOEXEC and configuration examples
docs/               design, setup, verification, risks, and release notes
build/               generated DOS binaries and maps (ignored)
release/             generated distribution tree; never edit by hand
external/            local third-party reference sources (ignored)
```

`SPECIFICATION.md` defines the current product requirements. `README.md` and
the documents under `docs/` describe usage and implementation. Incomplete
qualification work lives in `test-matrix.md` and `release-checklist.md`.

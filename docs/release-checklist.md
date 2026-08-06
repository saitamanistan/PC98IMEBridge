# Release Checklist

## Required before a public release

- [x] Add the `GPL-3.0-or-later` project license in `LICENSE`.
- [x] Confirm that no third-party material is bundled. Local `external/`
      reference sources are ignored and must not be included automatically.
- [ ] Build all advertised targets from a clean checkout.
- [ ] Run `make test` and retain the results.
- [ ] Run the PC-98 resident-map check through `make pc98-tsr`.
- [ ] Repeat the np21w manual matrix: normal Space, hotkey ON/OFF, Japanese
      text, repeated send, Enter, Backspace, arrows, Esc OFF, timeout, and
      reconnect.
- [ ] Repeat VZ Editor and FreeCOM tests with the exact release binaries.
- [ ] Verify installation and rollback instructions on a clean np21w share.
- [ ] Confirm the archive contains the one-port `IME98TSR.COM` and `IME98.COM`
      builds, includes `LICENSE`, identifies the corresponding tagged source,
      and excludes `IME98TSD.COM` and `IME98DBG.COM`.
- [ ] Verify that `/U` restores both vectors, releases resident memory, and
      permits reinstall without reboot; also verify ownership-conflict refusal.
- [ ] Push the single SemVer release tag on HEAD to GitHub `origin`, then run
      `scripts/build-release.sh`; do not bypass its clean-tree/tag checks or
      copy stale `build/` files into `release/`.

## Recommended after the emulator release

- [ ] Qualify at least one real PC-9801 or PC-9821 configuration.
- [ ] Add deterministic Named Pipe host tests for OPEN/TEXT/KEY/CLOSE flows.
- [ ] Add protocol semantic vectors for KEY and capability negotiation.

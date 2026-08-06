# Risks and Qualification Gaps

## Current PC-98 qualification risks

- np21w BIOS emulation may differ from real PC-9801/PC-9821 hardware.
- Named Pipe behavior does not prove physical RS-232C timing or recovery.
- BIOS keyboard injection can vary by DOS version, resident software, and
  application. FreeCOM and VZ evidence does not establish universal support.
- `/U` intentionally refuses removal if another program has replaced either
  owned interrupt vector; reboot remains the safe fallback.
- Only explicit debug targets initialize the PC-9861K debug channel; release
  binaries use the normal COM1 transport only.

## Release controls

- Keep platform-specific constants traceable to documentation or exact
  emulator source.
- Retain packet bounds, CRC, timeout, disconnect, and resident-map checks.
- Publish emulator evidence separately from real-hardware claims.
- Complete the current gaps in `test-matrix.md` and `release-checklist.md`.

# AGENTS.md — PC98IMEBridge Development Guide

This file is the operational guide for humans and AI agents working in this
checkout. Read `README.md` for the user overview and `SPECIFICATION.md` for the
current normative requirements. Treat `docs/` and the source as the authority
for implementation details and verification evidence.

## Repository map

- `host/ImeDosBridge/`: .NET 8 Windows Forms bridge.
- `dos/common/`: shared framing and CRC16.
- `dos/include/protocol.h`: wire message and logical-key constants.
- `dos/pc98/`: PC-98 foreground client, TSR, SYS driver, and probes.
- `tools/`: Python protocol tests and TSR linker-map validator.
- `samples/`: deployable PC-98 CONFIG.SYS, AUTOEXEC, and bridge configuration examples.
- `scripts/`: machine-local launch helpers.
- `docs/`: current architecture, setup, verification, and risk notes.
- `build/`, `host/**/bin`, `host/**/obj`, `release/`: generated output.
- `external/`: locally downloaded third-party reference source; ignored and
  not part of a public distribution.

Do not assume directories mentioned only in the original specification exist.
There is currently no `host/ImeDosBridge.Tests` project or `dos/test` tree.

## Current product boundary

The primary working path is:

```text
PC-98 application ⇄ IME98TSR.COM ⇄ np21w COM1/NP2-NamedPipe
                  ⇄ ImeDosBridge ⇄ Windows IME
```

The PC-98 path is emulator-qualified on np21w rev103 and FreeDOS(98), not yet
qualified on real hardware. `IME98.SYS` is a safe diagnostic driver, not the
primary interactive path.

## Toolchain and working directory

Run Unix commands from the repository root in WSL:

```text
/mnt/c/Users/kazuh/vscode/PC98IMEBridge
```

Required tools:

- `ia16-elf-gcc`, `ia16-elf-as`, and `ia16-elf-ld` for DOS binaries.
- Python 3 for protocol tests and map validation.
- Repository-local `.dotnet/dotnet` (SDK 8.0.423 in this checkout), or a
  compatible system .NET SDK.
- Windows Desktop runtime. This machine has runtime 10.0; set
  `DOTNET_ROLL_FORWARD=Major` when launching the net8.0 host with it.

## Build and automated verification

The normal full verification entrypoint is:

```sh
./scripts/build-all.sh
```

It runs the protocol tests, builds all PC-98 targets including probes and the
resident-map check, then builds the Windows bridge. It uses two
parallel make jobs by default; override that with `PC98IMEBRIDGE_BUILD_JOBS`.

Run these individual commands after changing only a specific area:

```sh
make test
make pc98-tsr
make pc98-tsr-debug pc98-debug
make pc98 pc98-sys pc98-device-test
```

`make pc98-tsr` is not just a compile. It uses `-fno-common`, emits
`build/pc98/IME98TSR.COM.map`, and runs `tools/check_pc98_tsr_map.py`. Never
remove or bypass that check: state outside the resident boundary previously
caused an Invalid Opcode after DOS reclaimed memory.

Release `pc98` and `pc98-tsr` targets also run
`tools/check_pc98_release_map.py`, which rejects accidental linkage of the
PC-9861K debug serial channel. Do not bypass that check either.

Build the host from WSL with:

```sh
DOTNET_CLI_HOME=/tmp/pc98imebridge-dotnet-cli \
  ./.dotnet/dotnet build host/ImeDosBridge.sln -p:EnableWindowsTargeting=true
```

The solution currently has no .NET test project, so a successful zero-warning
build plus `make test` is the automated host baseline. Add deterministic Named
Pipe tests under `host/ImeDosBridge.Tests/` when introducing that project.

If the host build reports that `ImeDosBridge.dll` is inaccessible, the running
bridge owns the file. Stop only this project's process; do not terminate all
`dotnet.exe` processes:

```powershell
Get-CimInstance Win32_Process |
  Where-Object { $_.CommandLine -like '*ImeDosBridge.dll*' } |
  ForEach-Object { Stop-Process -Id $_.ProcessId -Force }
```

## Local np21w environment

```text
Emulator: D:\np21w\np21w-starterset\np21x64w.exe
Boot HDI: D:\np21w\np21w-starterset\fdosboot.hdi
Share:    D:\np21w\np21w-starterset\share  (guest Z:)
```

np21w Serial/Parallel COM1 settings:

```text
Type=PIPE
Pipe Name=NP2-NamedPipe
Server Name=.
Speed=19200 bps
```

COM3 is a different np21w device. Do not call the PC-98 transport COM3. Normal
COM1 uses data/status ports `0030h/0032h`; `0130h/0132h` are FIFO-mode-only.

Optional diagnostics use PC-9861K CH1 through np21w COM2, pipe
`NP2-ImeDebug`, data/status ports `00B1h/00B3h`.
Only `IME98TSD.COM` and `IME98DBG.COM`, built by the explicit debug targets,
access this channel. Release `IME98TSR.COM` and `IME98.COM` use COM1 only.

## Deploy and launch

Before an emulator run, copy current binaries and configuration to the share:

```text
build\pc98\IME98TSR.COM  -> D:\np21w\np21w-starterset\share
build\pc98\IME98.COM     -> D:\np21w\np21w-starterset\share
samples\IME98.CFG        -> D:\np21w\np21w-starterset\share
samples\AUTOEXEC.PC98.BAT -> D:\np21w\np21w-starterset\share\AUTOEXEC.BAT
```

The sample AUTOEXEC installs `Z:\IME98TSR.COM` automatically. Foreground
`IME98.COM /VERIFY` remains manual.

`samples/CONFIG.PC98.SYS` is a minimal CONFIG.SYS example; FreeDOS(98) may use
the filename `FDCONFIG.SYS`. The normal TSR path requires no `DEVICE` entry.
The commented `DEVICE=Z:\IME98.SYS` line is only for an explicit diagnostic
driver test and must not be enabled as part of normal bridge setup.

On Windows, `scripts/start-pc98-test.bat` resolves the repository relative to
the script, copies files, stops only this bridge and np21w, then starts the
bridge before np21w. It defaults to the verified paths above. Set
`PC98IMEBRIDGE_NP2_DIR` or `PC98IMEBRIDGE_DOTNET_EXE` to override
machine-specific paths.

Equivalent bridge launch:

```powershell
$env:DOTNET_ROLL_FORWARD='Major'
& 'C:\Program Files\dotnet\dotnet.exe' \
  'C:\Users\kazuh\vscode\PC98IMEBridge\host\ImeDosBridge\bin\Debug\net8.0-windows\ImeDosBridge.dll' \
  --pipe NP2-NamedPipe --pipe-client --debug-pipe NP2-ImeDebug
```

Start the bridge before np21w because the bridge is the Named Pipe client and
np21w exposes the COM1 server endpoint during startup.

## Configuration

`samples/IME98.CFG` is read before the TSR becomes resident:

```ini
DATA_PORT=0030
STATUS_PORT=0032
MODE=02
COMMAND=27
TIMER_COUNT=0008
HOTKEY=SHIFT+SPACE
```

Supported hotkeys are `SHIFT+SPACE`, `CTRL+SPACE`, and `GRAPH+SPACE`.
Configuration keys are uppercase. Keep Space as the primary key unless its BIOS
queue and raw-state behavior are verified and documented for a replacement.

`host/ImeDosBridge/IMEBRIDGE.CFG` is copied beside the built host. It configures
SEND, CLOSE, REMOTE_ENTER, REMOTE_BACKSPACE, and four cursor shortcuts. Values
accept `Ctrl+`, `Shift+`, and `Alt+`; `NONE` disables a shortcut. Restart the
corresponding process after changing either configuration file.

## Manual PC-98 smoke test

Keyboard actions are performed by the user. Agents handle builds, deployment,
process restarts, and log inspection.

1. Boot and verify `IME98 TSR resident` appears once.
2. Confirm ordinary Space inserts exactly one Space.
3. Press the configured modifier+Space; Bridge should receive OPEN_IME and take
   focus.
4. Send ASCII and Japanese text. It must appear immediately without another
   PC-98 key or IME toggle.
5. Send again in the same ON session.
6. With the Bridge input empty, test Enter, Backspace, and all four arrows in
   FreeCOM or VZ Editor.
7. Press Esc in an empty Bridge input. TSR should turn OFF and focus should
   return to np21w.
8. Stop/restart the bridge and verify the TSR times out or reconnects without a
   keyboard lock.

Relevant logs are beside the built host:

```text
host/ImeDosBridge/bin/Debug/net8.0-windows/bridge-status.log
host/ImeDosBridge/bin/Debug/net8.0-windows/pc98-debug.log
```

Useful event chain:

```text
TSR HOTKEY PRESSED -> HELLO/PONG -> OPEN_IME -> Input ready
HOST TX TEXT/KEY -> TSR RECEIVED -> QUEUED AND ACKNOWLEDGED -> next OPEN_IME
CLOSE_IME -> TSR IME OFF -> HOST focus target=np21w
```

## Platform safety rules

- Never guess PC-98 interrupt numbers, BIOS work areas, I/O ports, PIC bits,
  or keyboard words. Verify them against primary platform
  documentation or the exact emulator source and record the result under
  `docs/` before hard-coding it.
- Preserve CP932 byte boundaries and validate packet length, sequence, CRC,
  timeout, and disconnect behavior.
- Never use Windows synthetic key injection for np21w keyboard tests. It once
  left a key in repeat state and caused continuous input and beeps.
- Do not modify `fdosboot.hdi` for routine tests. If an HDI modification is
  explicitly required, stop np21w and create a backup first.
- Do not overwrite user changes in the share without first reading the target.
- `IME98TSR.COM /U` may release the resident block only after verifying direct
  ownership and restoring both installed vectors. Reboot if it refuses removal
  or reports a memory-release error.
- Preserve unrelated work in a dirty worktree. Generated outputs are not source.

## Coding and documentation conventions

- C: `snake_case`, explicit fixed-width assumptions, readable K&R braces.
- C#: `PascalCase` types/public members and `camelCase` private locals/fields.
- Assembly comments must state the verified platform source or observed reason
  for non-obvious interrupt, port, and BIOS-memory behavior.
- Keep protocol constants synchronized between `dos/include/protocol.h`, host
  code, and `docs/protocol.md`.
- Keep active documentation focused on current supported behavior. Remove
  obsolete experiment instructions and development chronology when they no
  longer explain a current constraint or verification requirement.
- Public docs must clearly distinguish np21w evidence from unverified
  real-hardware behavior.

## Release and pull requests

A change should use a focused commit with an imperative subject and name the
affected area (`host`, `pc98`, or `protocol`).
A PR should state behavior, commands and results, emulator/hardware
evidence, configuration changes, and rollback implications.

Before public release, complete `docs/release-checklist.md`. The project is
licensed under `GPL-3.0-or-later`; every binary package must include the
canonical `LICENSE` text and identify the corresponding tagged source. Do not
publish third-party `external/` material.

Release ZIP versions come exclusively from a single SemVer tag on `HEAD`. The
tag must exist on GitHub `origin`, resolve to `HEAD`, and the tracked worktree
must be clean. Do not bypass these checks or assemble files under `release/`
manually; use `scripts/build-release.sh`.

A local ZIP is not a completed public release. After the build succeeds, make
the exact `release/PC98IMEBridge-$release_tag.zip` available from GitHub Releases:

```sh
release_tag=$(git describe --tags --exact-match HEAD)
archive="release/PC98IMEBridge-$release_tag.zip"
sha256sum "$archive"
gh release create "$release_tag" \
  "$archive#PC98IMEBridge-$release_tag.zip" \
  --repo saitamanistan/PC98IMEBridge \
  --title "PC98IMEBridge $release_tag" \
  --generate-notes --verify-tag
gh release view "$release_tag" --repo saitamanistan/PC98IMEBridge \
  --json url,tagName,assets,isDraft,isPrerelease,publishedAt
```

For a SemVer prerelease tag, add `--prerelease` when creating the release.
Treat the upload as complete only when the release is not a draft, the asset
state is `uploaded`, and its name, size, and GitHub-reported SHA-256 digest
match the local ZIP. Pushing a tag alone publishes only GitHub's source
archives; it does not publish the binary package. If a release or same-named
asset already exists, inspect and compare it first; never silently overwrite a
different asset.

# Development Environment Setup

The supported development layout is Windows 10/11 with WSL 2, an Ubuntu
environment for compilation, and np21w on Windows for interactive testing.
The verified checkout uses Ubuntu 24.04 x86-64.

## 1. Prepare WSL

Install WSL 2 and Ubuntu, then open an Ubuntu shell. Microsoft maintains the
[current WSL installation instructions](https://learn.microsoft.com/windows/wsl/install).

Install the ordinary build prerequisites:

```sh
sudo apt update
sudo apt install git make python3 curl software-properties-common
```

Clone or copy the repository onto a Windows or WSL filesystem and change to
its root. All commands below assume that directory is current.

## 2. Install the IA-16 toolchain

This project needs TK Chia's IA-16 GCC port. A normal host `gcc` cannot build
the 16-bit `.COM` and `.SYS` targets. On supported Ubuntu versions, use the
maintainer's [build-ia16 PPA](https://launchpad.net/~tkchia/+archive/ubuntu/build-ia16):

```sh
sudo add-apt-repository ppa:tkchia/build-ia16
sudo apt update
sudo apt install gcc-ia16-elf
```

Confirm that all three required programs are visible:

```sh
ia16-elf-gcc --version
ia16-elf-as --version
ia16-elf-ld --version
```

The verified environment currently reports GCC 6.3.0 and GNU Binutils 2.39.
If the PPA does not support the selected distribution, build or install the
toolchain using the maintainer's
[build-ia16 release sources](https://gitlab.com/tkchia/build-ia16/-/releases),
then put the `ia16-elf-*` executables on `PATH`.

## 3. Install the .NET SDK for the host build

The bridge targets `net8.0-windows`. A system .NET 8 SDK is sufficient. For a
reproducible non-administrator install matching this checkout, install SDK
8.0.423 under the ignored `.dotnet/` directory:

```sh
curl -L https://dot.net/v1/dotnet-install.sh \
  -o /tmp/pc98imebridge-dotnet-install.sh
bash /tmp/pc98imebridge-dotnet-install.sh \
  --version 8.0.423 \
  --install-dir .dotnet
./.dotnet/dotnet --version
```

Microsoft documents this mechanism in
[Install .NET on Linux with a script](https://learn.microsoft.com/dotnet/core/install/linux-scripted-manual).
The build script uses `.dotnet/dotnet` when present and otherwise uses
`dotnet` from `PATH`.

Install the .NET 8 Windows Desktop Runtime on Windows to run the bridge. A
newer Desktop Runtime may be used intentionally by setting
`DOTNET_ROLL_FORWARD=Major`. Check installed Windows runtimes in PowerShell:

```powershell
dotnet --list-runtimes
```

## 4. Build and test

Run the complete automated baseline from WSL or Linux:

```sh
./scripts/build-all.sh
```

This runs the protocol tests, builds every PC-98 binary and diagnostic,
validates the TSR resident map, and builds the Windows bridge. Set
`PC98IMEBRIDGE_BUILD_JOBS` to change the default of two parallel make jobs.

Useful individual commands are:

```sh
make test
make pc98-tsr
make pc98-tsr-debug pc98-debug
make pc98 pc98-sys pc98-device-test
DOTNET_CLI_HOME=/tmp/pc98imebridge-dotnet-cli \
  ./.dotnet/dotnet build host/ImeDosBridge.sln \
  -p:EnableWindowsTargeting=true
```

Generated DOS files are written below `build/pc98/`. Host output is below
`host/ImeDosBridge/bin/`. Both locations are ignored by Git.

`IME98TSR.COM` and `IME98.COM` are release-compatible one-port builds. The
explicit debug targets produce `IME98TSD.COM` and `IME98DBG.COM`, which also
access PC-9861K CH.1 for development logging.

## 5. Prepare np21w

Obtain np21w separately; emulator binaries and source are not redistributed by
this repository. The verified local layout is:

```text
D:\np21w\np21w-starterset\np21x64w.exe
D:\np21w\np21w-starterset\fdosboot.hdi
D:\np21w\np21w-starterset\share\
```

Configure Serial/Parallel COM1 as follows:

```text
Type:        PIPE
Pipe Name:   NP2-NamedPipe
Server Name: .
Speed:       19200 bps
```

Copy `IME98TSR.COM`, `IME98.COM`, `IME98.CFG`, and the AUTOEXEC sample to the
host share as described in [pc98-environment.md](pc98-environment.md). Do not
edit the HDI for a routine TSR test.

## 6. Launch the local verification setup

On the verified Windows layout, run:

```bat
scripts\start-pc98-test.bat
```

The script locates the repository relative to itself. Override local paths
when necessary:

```bat
set PC98IMEBRIDGE_NP2_DIR=D:\path\to\np21w
set PC98IMEBRIDGE_DOTNET_EXE=C:\Program Files\dotnet\dotnet.exe
scripts\start-pc98-test.bat
```

It copies the current files to the share, stops only this project's existing
Bridge and np21w processes, starts the Bridge, then starts np21w. Keyboard
actions during the emulator smoke test remain manual.

## Troubleshooting

- `ia16-elf-gcc: command not found`: install the IA-16 package or correct
  `PATH`; host GCC is not a substitute.
- `EnableWindowsTargeting` error: use `scripts/build-all.sh` or pass
  `-p:EnableWindowsTargeting=true` explicitly.
- `ImeDosBridge.dll` is locked: stop only the running process whose command
  line contains this project's DLL, then rebuild.
- Named Pipe timeouts: start the Bridge first, verify np21w COM1 is `PIPE`, and
  confirm the pipe name is exactly `NP2-NamedPipe`.
- No guest files: confirm np21w's host share maps to guest drive `Z:` and that
  the sample files were copied there.

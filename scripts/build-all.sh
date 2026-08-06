#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "$script_dir/.." && pwd)
build_jobs=${PC98IMEBRIDGE_BUILD_JOBS:-2}

cd "$repo_dir"

echo "[1/3] Running protocol tests"
make test

echo "[2/3] Building PC-98 targets"
make -j "$build_jobs" \
    pc98 pc98-debug pc98-tsr pc98-tsr-debug pc98-keyprobe \
    pc98-sys pc98-device-test \
    pc98-com-probe pc98-int14-probe pc98-com-status-probe

if [[ -x .dotnet/dotnet ]]; then
    dotnet_command=(./.dotnet/dotnet)
elif command -v dotnet >/dev/null 2>&1; then
    dotnet_command=(dotnet)
else
    echo "error: dotnet SDK not found (.dotnet/dotnet or PATH)" >&2
    exit 1
fi

echo "[3/3] Building Windows bridge"
DOTNET_CLI_HOME=${DOTNET_CLI_HOME:-/tmp/pc98imebridge-dotnet-cli} \
    "${dotnet_command[@]}" build host/ImeDosBridge.sln \
    -p:EnableWindowsTargeting=true

echo "Build and tests completed successfully."

#!/usr/bin/env bash
set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_dir=$(cd -- "$script_dir/.." && pwd)
release_dir="$repo_dir/release"

cd "$repo_dir"

if [[ -n $(git status --porcelain --untracked-files=no) ]]; then
    echo "error: tracked files must be clean before building a release" >&2
    exit 1
fi

origin_url=$(git remote get-url origin 2>/dev/null || true)
if [[ -z $origin_url || ! $origin_url =~ github\.com[:/] ]]; then
    echo "error: origin must be a GitHub repository" >&2
    exit 1
fi

semver_number='(0|[1-9][0-9]*)'
semver_prerelease_id='(0|[1-9][0-9]*|[0-9]*[A-Za-z-][0-9A-Za-z-]*)'
semver_build_id='[0-9A-Za-z-]+'
semver_pattern="^v${semver_number}\\.${semver_number}\\.${semver_number}(-${semver_prerelease_id}(\\.${semver_prerelease_id})*)?(\\+${semver_build_id}(\\.${semver_build_id})*)?$"
release_tags=()
while IFS= read -r candidate; do
    if [[ $candidate =~ $semver_pattern ]]; then
        release_tags+=("$candidate")
    fi
done < <(git tag --points-at HEAD)

if (( ${#release_tags[@]} != 1 )); then
    echo "error: HEAD must have exactly one vMAJOR.MINOR.PATCH SemVer tag" >&2
    exit 1
fi

release_tag=${release_tags[0]}
head_commit=$(git rev-parse HEAD)
local_tag_commit=$(git rev-parse "$release_tag^{commit}")
if [[ $local_tag_commit != "$head_commit" ]]; then
    echo "error: local tag $release_tag does not resolve to HEAD" >&2
    exit 1
fi

if ! remote_output=$(git ls-remote --tags origin \
    "refs/tags/$release_tag" "refs/tags/$release_tag^{}"); then
    echo "error: could not read tags from GitHub origin" >&2
    exit 1
fi

remote_tag_commit=$(awk '$2 ~ /\^\{\}$/ { print $1; exit }' <<<"$remote_output")
if [[ -z $remote_tag_commit ]]; then
    remote_tag_commit=$(awk 'NR == 1 { print $1 }' <<<"$remote_output")
fi
if [[ -z $remote_tag_commit ]]; then
    echo "error: tag $release_tag has not been pushed to GitHub origin" >&2
    exit 1
fi
if [[ $remote_tag_commit != "$head_commit" ]]; then
    echo "error: GitHub tag $release_tag does not resolve to HEAD" >&2
    exit 1
fi

product_version=${release_tag#v}
package_name="PC98IMEBridge-$release_tag"
archive_path="$release_dir/$package_name.zip"

mkdir -p "$release_dir"
temp_dir=$(mktemp -d "$release_dir/.build-release.XXXXXX")
trap 'rm -rf -- "$temp_dir"' EXIT
package_dir="$temp_dir/$package_name"
publish_dir="$temp_dir/publish"
mkdir -p \
    "$package_dir/windows" \
    "$package_dir/pc98" \
    "$package_dir/diagnostics" \
    "$publish_dir"

"$script_dir/build-all.sh"

if [[ -x .dotnet/dotnet ]]; then
    dotnet_command=(./.dotnet/dotnet)
elif command -v dotnet >/dev/null 2>&1; then
    dotnet_command=(dotnet)
else
    echo "error: dotnet SDK not found (.dotnet/dotnet or PATH)" >&2
    exit 1
fi

echo "Publishing Windows x64 bridge version $product_version"
DOTNET_CLI_HOME=${DOTNET_CLI_HOME:-/tmp/pc98imebridge-dotnet-cli} \
    "${dotnet_command[@]}" publish host/ImeDosBridge/ImeDosBridge.csproj \
    -c Release -r win-x64 --self-contained false \
    -p:EnableWindowsTargeting=true \
    -p:Version="$product_version" \
    -p:ContinuousIntegrationBuild=true \
    -p:PublishSingleFile=true \
    -p:DebugType=None \
    -p:DebugSymbols=false \
    -o "$publish_dir"

for file in ImeDosBridge.exe IMEBRIDGE.CFG; do
    if [[ ! -f $publish_dir/$file ]]; then
        echo "error: published host file is missing: $file" >&2
        exit 1
    fi
    cp -- "$publish_dir/$file" "$package_dir/windows/$file"
done

if [[ ! -f build/pc98/IME98TSR.COM ]]; then
    echo "error: PC-98 build file is missing: IME98TSR.COM" >&2
    exit 1
fi
cp -- build/pc98/IME98TSR.COM "$package_dir/pc98/IME98TSR.COM"
cp -- samples/IME98.CFG "$package_dir/pc98/IME98.CFG"
cp -- samples/AUTOEXEC.PC98.BAT "$package_dir/pc98/AUTOEXEC.BAT"

for file in IME98.COM IME98.SYS; do
    if [[ ! -f build/pc98/$file ]]; then
        echo "error: PC-98 diagnostic build file is missing: $file" >&2
        exit 1
    fi
    cp -- "build/pc98/$file" "$package_dir/diagnostics/$file"
done
cp -- samples/CONFIG.PC98.SYS "$package_dir/diagnostics/CONFIG.SYS"

cp -- docs/binary-package.md "$package_dir/README.md"
cp -- LICENSE "$package_dir/LICENSE"

printf 'PC98IMEBridge release: %s\nGit commit: %s\nSource: https://github.com/saitamanistan/PC98IMEBridge/tree/%s\nLicense: GPL-3.0-or-later\n' \
    "$release_tag" "$head_commit" "$release_tag" >"$package_dir/BUILD-INFO.txt"

temporary_archive="$temp_dir/$package_name.zip"
(
    cd "$temp_dir"
    python3 -m zipfile -c "$temporary_archive" "$package_name"
)
mv -f -- "$temporary_archive" "$archive_path"

echo "Release package created: $archive_path"

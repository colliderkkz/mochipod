#!/usr/bin/env bash
# Usage: bash tools/build.sh sim|arm /absolute/path/to/rockbox-4.0-final /path/to/build
set -euo pipefail
mode=${1:?sim or arm}
source_dir=$(realpath "${2:?dedicated Rockbox source checkout}")
mkdir -p "${3:?dedicated build directory}"
output=$(realpath "$3")
project=$(cd "$(dirname "$0")/.." && pwd)
python3 "$project/tools/stage.py" "$source_dir"
cd "$output"
case "$mode" in
  sim) args=(--type=s) ;;
  arm) args=(--type=n); if [[ -n ${ARM_PREFIX:-} ]]; then args+=(--compiler-prefix="$ARM_PREFIX"); fi ;;
  *) exit 2 ;;
esac
"$source_dir/tools/configure" --target=ipod6g "${args[@]}"
if [[ $mode == sim ]]; then
    make -j"${JOBS:-8}"
    make fullinstall
else
    make -j"${JOBS:-8}" "$output/apps/plugins/mochipod.rock"
fi

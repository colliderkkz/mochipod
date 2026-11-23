#!/usr/bin/env bash
# Dedicated Rockbox source/build trees. Keep release ARM builds independent.
set -eu
root=$(cd "$(dirname "$0")/.." && pwd)
source_dir=$(realpath "$1")
sim=$(realpath "$2")
output=$(realpath -m "$3")
mkdir -p "$(dirname "$output")"
python3 "$root/tools/stage.py" "$source_dir"
entry="$source_dir/apps/plugins/mochipod.c"
restore() {
    python3 "$root/tools/stage.py" "$source_dir"
    make -s -C "$sim" "$sim/apps/plugins/mochipod.rock"
}
trap restore EXIT
cp "$root/tests/family-route.h" "$root/tests/family-sim.c" "$source_dir/apps/plugins/mochipod_src/"
sed -i '/#include "mochipod_src\/platform\/rockbox.c"/i #define plugin_start production_plugin_start' "$entry"
printf '%s\n' '#undef plugin_start' '#include "mochipod_src/family-sim.c"' >> "$entry"
make -s -C "$sim" "$sim/apps/plugins/mochipod.rock"
cp "$sim/apps/plugins/mochipod.rock" "$output"
